// ============================================================================
// CVD Δ/Price Divergence (Ratio) with Normalization to [-100, +100]
// Author: ChatGPT (GPT-5 Pro)
// ----------------------------------------------------------------------------
// SG[0] Ratio_N               : MAIN OUTPUT (normalized if enabled; else raw)
// SG[1] DeltaChange_N         : Sum(Δ, N)  where Δ = AskVol - BidVol
// SG[2] PriceChange_N         : Price[i] - Price[i-N]
// SG[3] TwoStepScore          : + for (neg Δ absorption -> up flip), - for opposite
// SG[4] ResidualZ             : Beta-adjusted residual z-score (Mode 1)
// SG[5] RatioRaw_Debug        : Unnormalized raw ratio (for reference)
// ----------------------------------------------------------------------------
// Inputs added for normalization:
//   [6] Normalization Method   : Off; ZScore→Tanh; Sigma Clip
//   [7] Norm Window (bars)     : default 500
//   [8] Z Divisor              : default 1.5 (controls tanh steepness)
//   [9] Sigma @ 100            : default 3.0 (for Sigma Clip; ±Kσ -> ±100)
// ============================================================================

#include "sierrachart.h"
#include <math.h> // fabs, sqrt, tanh
SCDLLName("CVD_Divergence_Ratio")

SCSFExport scsf_CVDDivergenceRatio(SCStudyInterfaceRef sc)
{
    // --- Subgraphs ---
    SCSubgraphRef Ratio_N        = sc.Subgraph[0]; // will be normalized if enabled
    SCSubgraphRef DeltaChange_N  = sc.Subgraph[1];
    SCSubgraphRef PriceChange_N  = sc.Subgraph[2];
    SCSubgraphRef TwoStepScore   = sc.Subgraph[3];
    SCSubgraphRef ResidualZ      = sc.Subgraph[4];
    SCSubgraphRef RatioRaw_Debug = sc.Subgraph[5];

    // --- Inputs (existing) ---
    SCInputRef In_LookbackN      = sc.Input[0];
    SCInputRef In_PriceField     = sc.Input[1];
    SCInputRef In_MinPriceTicks  = sc.Input[2];
    SCInputRef In_Mode           = sc.Input[3];
    SCInputRef In_RegLookback    = sc.Input[4];
    SCInputRef In_ScoreDenomBias = sc.Input[5];

    // --- New Inputs (normalization) ---
    SCInputRef In_NormMethod     = sc.Input[6];
    SCInputRef In_NormWindow     = sc.Input[7];
    SCInputRef In_ZDivisor       = sc.Input[8];
    SCInputRef In_SigmaAt100     = sc.Input[9];

    if (sc.SetDefaults)
    {
        sc.GraphName        = "CVD Δ/Price Divergence (Ratio) + Normalization";
        sc.StudyDescription = "Sum(Δ, N) / PriceChange(N) with tick floor; "
                              "optional two-step pattern score & residual Z; "
                              "and optional normalization of SG1 to [-100, +100].";
        sc.AutoLoop    = 1;
        sc.GraphRegion = 1;
        sc.ValueFormat = VALUEFORMAT_INHERITED;

        // Subgraphs
        Ratio_N.Name         = "Δ/Price Ratio (SG1; normalized if enabled)";
        Ratio_N.DrawStyle    = DRAWSTYLE_LINE;
        Ratio_N.DrawZeros    = 0;

        DeltaChange_N.Name   = "Δ Change (N bars)";
        DeltaChange_N.DrawStyle = DRAWSTYLE_IGNORE;
        DeltaChange_N.DrawZeros  = 0;

        PriceChange_N.Name   = "Price Change (N bars)";
        PriceChange_N.DrawStyle = DRAWSTYLE_IGNORE;
        PriceChange_N.DrawZeros  = 0;

        TwoStepScore.Name    = "Two‑Step Pattern Score";
        TwoStepScore.DrawStyle = DRAWSTYLE_LINE;
        TwoStepScore.DrawZeros  = 0;

        ResidualZ.Name       = "Residual Z (Mode 1)";
        ResidualZ.DrawStyle  = DRAWSTYLE_IGNORE;
        ResidualZ.DrawZeros  = 0;

        RatioRaw_Debug.Name      = "Ratio Raw (Debug)";
        RatioRaw_Debug.DrawStyle = DRAWSTYLE_IGNORE; // hidden unless you want to compare
        RatioRaw_Debug.DrawZeros = 0;

        // Inputs
        In_LookbackN.Name = "Lookback (bars)";
        In_LookbackN.SetInt(3);
        In_LookbackN.SetIntLimits(1, 200);

        In_PriceField.Name = "Price for Change";
        In_PriceField.SetInputDataIndex(SC_LAST); // SC_LAST, SC_CLOSE, etc.

        In_MinPriceTicks.Name = "Min Price Move (ticks) for Ratio Denominator";
        In_MinPriceTicks.SetFloat(1.0f);

        In_Mode.Name = "Mode";
        In_Mode.SetCustomInputStrings("Simple Ratio;Beta-Adjusted Residual Z-Score");
        In_Mode.SetCustomInputIndex(0); // default: Simple Ratio

        In_RegLookback.Name = "Regression Lookback (bars) [Mode 1]";
        In_RegLookback.SetInt(100);
        In_RegLookback.SetIntLimits(20, 5000);

        In_ScoreDenomBias.Name = "Two-Step Score Denominator Bias";
        In_ScoreDenomBias.SetFloat(1.0f);

        // Normalization inputs
        In_NormMethod.Name = "Normalization (for SG1)";
        In_NormMethod.SetCustomInputStrings("Off (Raw Ratio);ZScore→Tanh [-100..100];Sigma Clip [-100..100]");
        In_NormMethod.SetCustomInputIndex(1); // default to ZScore→Tanh

        In_NormWindow.Name = "Norm Window (bars)";
        In_NormWindow.SetInt(500);
        In_NormWindow.SetIntLimits(50, 50000);

        In_ZDivisor.Name = "Z Divisor (tanh steepness)";
        In_ZDivisor.SetFloat(1.5f); // smaller -> more aggressive saturation

        In_SigmaAt100.Name = "Sigma @ 100 (Sigma Clip)";
        In_SigmaAt100.SetFloat(3.0f);

        return;
    }

    // --- Local references to chart data ---
    const int    N            = In_LookbackN.GetInt();
    const int    priceIndex   = In_PriceField.GetInputDataIndex();
    const float  minTicks     = (float)In_MinPriceTicks.GetFloat();
    const double tickSize     = sc.TickSize > 0 ? sc.TickSize : 0.01;
    const double priceFloor   = (minTicks > 0 ? minTicks : 1.0f) * tickSize;
    const int    mode         = In_Mode.GetIndex(); // fixed getter
    const int    R            = In_RegLookback.GetInt();
    const double scoreBias    = In_ScoreDenomBias.GetFloat();

    const int    normMethod   = In_NormMethod.GetIndex();
    const int    normWindow   = In_NormWindow.GetInt();
    const double zDivisor     = In_ZDivisor.GetFloat();
    const double sigmaAt100   = In_SigmaAt100.GetFloat();

    SCFloatArrayRef AskVol = sc.BaseData[SC_ASKVOL];
    SCFloatArrayRef BidVol = sc.BaseData[SC_BIDVOL];
    SCFloatArrayRef Px     = sc.BaseData[priceIndex];

    // --- Initialize once ---
    if (sc.Index == 0)
    {
        Ratio_N[0]        = 0.0f;
        DeltaChange_N[0]  = 0.0f;
        PriceChange_N[0]  = 0.0f;
        TwoStepScore[0]   = 0.0f;
        ResidualZ[0]      = 0.0f;
        RatioRaw_Debug[0] = 0.0f;

        // Persistent state:
        // 1: rolling Δ sum over N bars
        // 2: sum of RatioRaw over normalization window
        // 3: sum of RatioRaw^2 over normalization window
        sc.SetPersistentDouble(1, 0.0);
        sc.SetPersistentDouble(2, 0.0);
        sc.SetPersistentDouble(3, 0.0);
    }

    // --- Rolling Δ sum over last N bars ---
    const double deltaNow = (double)(AskVol[sc.Index] - BidVol[sc.Index]);

    double rollingDeltaSum = sc.GetPersistentDouble(1);
    rollingDeltaSum += deltaNow;
    if (sc.Index >= N)
    {
        const double deltaToDrop = (double)(AskVol[sc.Index - N] - BidVol[sc.Index - N]);
        rollingDeltaSum -= deltaToDrop;
    }
    sc.SetPersistentDouble(1, rollingDeltaSum);

    // --- N-bar price change ---
    double priceChangeN = 0.0;
    if (sc.Index >= N)
        priceChangeN = (double)Px[sc.Index] - (double)Px[sc.Index - N];

    auto SafeDenominator = [&](double pc) -> double
    {
        const double apc = fabs(pc);
        if (apc < priceFloor) return (pc >= 0.0 ? +priceFloor : -priceFloor);
        return pc;
    };

    // --- Raw ratio for this bar ---
    double ratioRaw = 0.0;
    if (sc.Index >= N)
        ratioRaw = rollingDeltaSum / SafeDenominator(priceChangeN);

    RatioRaw_Debug[sc.Index] = (float)ratioRaw;

    // --- Maintain rolling sums for normalization (include current bar) ---
    if (normMethod != 0) // if normalization not Off
    {
        double sum  = sc.GetPersistentDouble(2);
        double sum2 = sc.GetPersistentDouble(3);

        sum  += ratioRaw;
        sum2 += ratioRaw * ratioRaw;

        if (sc.Index >= normWindow)
        {
            const double old = (double)RatioRaw_Debug[sc.Index - normWindow];
            sum  -= old;
            sum2 -= old * old;
        }

        sc.SetPersistentDouble(2, sum);
        sc.SetPersistentDouble(3, sum2);

        const int sampleCount = (sc.Index + 1 < normWindow) ? (sc.Index + 1) : normWindow;

        // Compute normalized value
        double ratioNorm = ratioRaw;

        if (sampleCount >= 2)
        {
            const double mean = sum / (double)sampleCount;
            const double var  = fmax(0.0, sum2 / (double)sampleCount - mean * mean);
            const double sd   = (var > 0.0) ? sqrt(var) : 0.0;

            if (sd > 0.0)
            {
                const double z = (ratioRaw - mean) / sd;

                if (normMethod == 1) // ZScore→Tanh
                {
                    const double x = zDivisor > 0.0 ? (z / zDivisor) : z;
                    ratioNorm = 100.0 * tanh(x);
                }
                else // normMethod == 2: Sigma Clip
                {
                    const double k = (sigmaAt100 > 0.0) ? sigmaAt100 : 3.0;
                    double scaled = (z / k) * 100.0;
                    if (scaled > 100.0)  scaled = 100.0;
                    if (scaled < -100.0) scaled = -100.0;
                    ratioNorm = scaled;
                }
            }
            else
            {
                ratioNorm = 0.0; // degenerate early case
            }
        }
        else
        {
            ratioNorm = 0.0;
        }

        // Guarantee hard bounds
        if (ratioNorm > 100.0)  ratioNorm = 100.0;
        if (ratioNorm < -100.0) ratioNorm = -100.0;

        Ratio_N[sc.Index]       = (float)ratioNorm;
        DeltaChange_N[sc.Index] = (float)rollingDeltaSum;
        PriceChange_N[sc.Index] = (float)priceChangeN;
    }
    else
    {
        // Normalization Off: SG1 is raw ratio
        Ratio_N[sc.Index]       = (float)ratioRaw;
        DeltaChange_N[sc.Index] = (float)rollingDeltaSum;
        PriceChange_N[sc.Index] = (float)priceChangeN;
    }

    // --- Two‑Step Pattern Score over two adjacent N‑bar windows ---
    if (sc.Index >= 2 * N)
    {
        const double deltaPrev  = (double)DeltaChange_N[sc.Index - N];
        const double pricePrev  = (double)PriceChange_N[sc.Index - N];
        const double ratioPrev  = deltaPrev / SafeDenominator(pricePrev);

        const double deltaCurr  = (double)DeltaChange_N[sc.Index];
        const double priceCurr  = (double)PriceChange_N[sc.Index];
        const double ratioCurr  = deltaCurr / SafeDenominator(priceCurr);

        const double denomMag   = fabs(ratioCurr) + scoreBias;

        const double negPrevAbsorp = (deltaPrev < 0.0) ? fabs(ratioPrev) : 0.0;
        const double posPrevAbsorp = (deltaPrev > 0.0) ? fabs(ratioPrev) : 0.0;
        const double wentUp        = (priceCurr > 0.0) ? 1.0 : 0.0;
        const double wentDown      = (priceCurr < 0.0) ? 1.0 : 0.0;

        const double score = (negPrevAbsorp * wentUp - posPrevAbsorp * wentDown) / denomMag;

        TwoStepScore[sc.Index] = (float)score;
    }
    else
    {
        TwoStepScore[sc.Index] = 0.0f;
    }

    // --- Optional Mode 1: Beta-Adjusted Residual Z-Score ---
    if (mode == 1 && sc.Index >= (N + R))
    {
        double sumX=0, sumY=0, sumXX=0, sumXY=0;
        const int kStart = sc.Index - R + 1;

        for (int k = kStart; k <= sc.Index; ++k)
        {
            const double X = (double)DeltaChange_N[k];
            const double Y = (double)PriceChange_N[k];
            sumX  += X; sumY  += Y;
            sumXX += X*X; sumXY += X*Y;
        }

        const double n = (double)R;
        const double Den = (n*sumXX - sumX*sumX);
        double b = 0.0, a = 0.0;
        if (fabs(Den) > 1e-12)
        {
            b = (n*sumXY - sumX*sumY) / Den;
            a = (sumY - b*sumX) / n;
        }
        else
        {
            b = 0.0; a = sumY / n;
        }

        double sse = 0.0;
        for (int k = kStart; k <= sc.Index; ++k)
        {
            const double X = (double)DeltaChange_N[k];
            const double Y = (double)PriceChange_N[k];
            const double e = Y - (a + b*X);
            sse += e*e;
        }
        const double dof = n - 2.0;
        const double sigma = (dof > 0.0 && sse >= 0.0) ? sqrt(sse / dof) : 0.0;

        const double Xc = (double)DeltaChange_N[sc.Index];
        const double Yc = (double)PriceChange_N[sc.Index];
        const double ec = Yc - (a + b*Xc);
        const double z  = (sigma > 0.0) ? (ec / sigma) : 0.0;

        ResidualZ[sc.Index] = (float)z;
    }
}
