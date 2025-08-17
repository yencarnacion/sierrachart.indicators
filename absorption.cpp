#include "sierrachart.h"
SCDLLName("Delta Efficiency v2.1 — Exhaustion/Absorption + CumDiv + TapeGate")

/*
What each symbol/color means

Red ★ (stars) — “Cum‑Delta Divergence (Exhaustion)”

Above a bar (★ near the high) = Price made a higher high, but cumulative delta is lower than at the prior HH → buyer exhaustion risk (fade‑short bias).

Below a bar (★ near the low) = Price made a lower low, but cumulative delta is higher than at the prior LL → seller exhaustion risk (fade‑long bias).

Orange ▲▼ (triangles) — “Exhaustion via Delta‑Efficiency”

Orange ▼ above a bar = Up‑exhaustion: big price progress on thin/weak delta + tape decel → buyers tiring; fade‑short setup.

Orange ▲ below a bar = Down‑exhaustion: mirror logic on the sell side → fade‑long setup.

Blue ▲▼ (triangles) — “Absorption via Delta‑Efficiency”

Blue ▼ above a bar = Buyers absorbed near highs: heavy positive delta, little price progress → likely turn down / mean‑revert.

Blue ▲ below a bar = Sellers absorbed near lows: heavy negative delta, little price progress → likely bounce / mean‑revert.

*/
SCSFExport scsf_DeltaEfficiencyMarkersV21(SCStudyInterfaceRef sc)
{
    // ---- Declare Subgraphs & Inputs (visible both in and out of SetDefaults) ----
    SCSubgraphRef SG_DE         = sc.Subgraph[0];
    SCSubgraphRef SG_DE_Z       = sc.Subgraph[1];
    SCSubgraphRef SG_DeltaAbsZ  = sc.Subgraph[2];
    SCSubgraphRef SG_ExhaustUp  = sc.Subgraph[3];
    SCSubgraphRef SG_ExhaustDn  = sc.Subgraph[4];
    SCSubgraphRef SG_AbsorbUp   = sc.Subgraph[5];
    SCSubgraphRef SG_AbsorbDn   = sc.Subgraph[6];
    SCSubgraphRef SG_DivExhUp   = sc.Subgraph[7];
    SCSubgraphRef SG_DivExhDn   = sc.Subgraph[8];

    SCInputRef In_Lookback   = sc.Input[0];
    SCInputRef In_Eps        = sc.Input[1];
    SCInputRef In_ZThr       = sc.Input[2];
    SCInputRef In_ZDeltaHi   = sc.Input[3];
    SCInputRef In_ZDeltaLo   = sc.Input[4];
    SCInputRef In_UseCO      = sc.Input[5];
    SCInputRef In_CapDE      = sc.Input[6];
    SCInputRef In_SwingLen   = sc.Input[7];
    SCInputRef In_TapeStudyID= sc.Input[8];
    SCInputRef In_EnableAlerts = sc.Input[9];
    SCInputRef In_AlertNum   = sc.Input[10];
    SCInputRef In_ZAbsDE     = sc.Input[11];

    if (sc.SetDefaults)
    {
        sc.GraphName = "Delta Efficiency v2.1 (Exh/Abs + CumDiv + TapeGate)";
        sc.AutoLoop = 1;
        sc.GraphRegion = 0;
        sc.UpdateAlways = 1;

        // Subgraphs styling
        SG_DE.Name = "Signed DE (ticks per |delta|)";
        SG_DE.DrawStyle = DRAWSTYLE_IGNORE;
        SG_DE.PrimaryColor = RGB(120,120,120);

        SG_DE_Z.Name = "DE Z-Score";
        SG_DE_Z.DrawStyle = DRAWSTYLE_IGNORE;
        SG_DE_Z.PrimaryColor = RGB(180,180,180);

        SG_DeltaAbsZ.Name = "|Delta| Z-Score";
        SG_DeltaAbsZ.DrawStyle = DRAWSTYLE_IGNORE;
        SG_DeltaAbsZ.PrimaryColor = RGB(180,180,180);

        // Exhaustion (orange)
        SG_ExhaustUp.Name = "Exhaustion Up";
        SG_ExhaustUp.DrawStyle = DRAWSTYLE_TRIANGLE_DOWN;
        SG_ExhaustUp.PrimaryColor = RGB(255,128,0);
        SG_ExhaustUp.LineWidth = 8;

        SG_ExhaustDn.Name = "Exhaustion Down";
        SG_ExhaustDn.DrawStyle = DRAWSTYLE_TRIANGLE_UP;
        SG_ExhaustDn.PrimaryColor = RGB(255,128,0);
        SG_ExhaustDn.LineWidth = 8;

        // Absorption (blue)
        SG_AbsorbUp.Name = "Absorption Up (buyers absorbed)";
        SG_AbsorbUp.DrawStyle = DRAWSTYLE_TRIANGLE_DOWN;
        SG_AbsorbUp.PrimaryColor = RGB(0,191,255);
        SG_AbsorbUp.LineWidth = 8;

        SG_AbsorbDn.Name = "Absorption Down (sellers absorbed)";
        SG_AbsorbDn.DrawStyle = DRAWSTYLE_TRIANGLE_UP;
        SG_AbsorbDn.PrimaryColor = RGB(0,191,255);
        SG_AbsorbDn.LineWidth = 8;

        SG_DivExhUp.Name = "CumDiv Exhaust Up";
        SG_DivExhUp.DrawStyle = DRAWSTYLE_STAR;
        SG_DivExhUp.PrimaryColor = RGB(255,0,0);
        SG_DivExhUp.LineWidth = 2;

        SG_DivExhDn.Name = "CumDiv Exhaust Down";
        SG_DivExhDn.DrawStyle = DRAWSTYLE_STAR;
        SG_DivExhDn.PrimaryColor = RGB(255,0,0);
        SG_DivExhDn.LineWidth = 2;

        // Inputs
        In_Lookback.Name = "Z-Score Lookback (bars)";   In_Lookback.SetInt(100);
        In_Eps.Name = "Epsilon (min |delta|)";          In_Eps.SetFloat(50.0f);
        In_ZThr.Name = "Z(DE) Threshold (exhaustion)";  In_ZThr.SetFloat(1.5f);
        In_ZDeltaHi.Name = "Z(|Delta|) Min (absorption)"; In_ZDeltaHi.SetFloat(0.5f);
        In_ZDeltaLo.Name = "Z(|Delta|) Max (exhaustion)"; In_ZDeltaLo.SetFloat(0.0f);
        In_UseCO.Name = "ΔPrice = Close-Open (else Close-Close)"; In_UseCO.SetYesNo(1);
        In_CapDE.Name = "Cap |DE| (pre-Z)";             In_CapDE.SetFloat(10.0f);
        In_SwingLen.Name = "Swing Length (bars)";       In_SwingLen.SetInt(3);
        In_TapeStudyID.Name = "Tape Reader Study ID (0=off)"; In_TapeStudyID.SetStudyID(0);
        In_EnableAlerts.Name = "Enable Alerts";         In_EnableAlerts.SetYesNo(0);
        In_AlertNum.Name = "Alert Number (1-50)";       In_AlertNum.SetInt(1);
        In_ZAbsDE.Name = "Absorption |Z(DE)| Max (near-zero band)"; In_ZAbsDE.SetFloat(0.5f);

        return;
    }

    // ---- Persistent state (use Sierra’s persistent storage) ----
    float& p_SumDE      = sc.GetPersistentFloat(1);
    float& p_SumSqDE    = sc.GetPersistentFloat(2);
    float& p_SumAbsD    = sc.GetPersistentFloat(3);
    float& p_SumSqAbsD  = sc.GetPersistentFloat(4);
    int&   p_ZCount     = sc.GetPersistentInt(1);
    int&   p_LastZIdx   = sc.GetPersistentInt(2);

    double& p_CumDelta  = sc.GetPersistentDouble(1);
    int&    p_LastHHIdx = sc.GetPersistentInt(3);
    int&    p_LastLLIdx = sc.GetPersistentInt(4);
    double& p_CumAtLastHH = sc.GetPersistentDouble(2);
    double& p_CumAtLastLL = sc.GetPersistentDouble(3);

    // ---- Refs & inputs ----
    const int idx = sc.Index;
    const int L = In_Lookback.GetInt();
    const float eps = In_Eps.GetFloat();
    const float zThr = In_ZThr.GetFloat();
    const float zDeltaHi = In_ZDeltaHi.GetFloat();
    const float zDeltaLo = In_ZDeltaLo.GetFloat();
    const bool useCO = In_UseCO.GetYesNo();
    const float deCap = In_CapDE.GetFloat();
    const int swingLen = In_SwingLen.GetInt();
    const int tapeID = In_TapeStudyID.GetStudyID();
    const bool alertOn = In_EnableAlerts.GetYesNo();
    const int alertNum = In_AlertNum.GetInt();
    const float zAbsDE = In_ZAbsDE.GetFloat();

    SCFloatArrayRef BidVol = sc.BaseData[SC_BIDVOL];
    SCFloatArrayRef AskVol = sc.BaseData[SC_ASKVOL];

    // ---- Reset on new calc / new session ----
    if (idx == 0 || sc.IsFullRecalculation)
    {
        p_SumDE = p_SumSqDE = p_SumAbsD = p_SumSqAbsD = 0.0f;
        p_ZCount = 0;
        p_LastZIdx = -1;
        p_CumDelta = 0.0;
        p_LastHHIdx = p_LastLLIdx = -1;
        p_CumAtLastHH = p_CumAtLastLL = 0.0;
    }
    if (idx == 0 || sc.IsNewTradingDay(idx))
    {
        p_CumDelta = 0.0;
        p_LastHHIdx = p_LastLLIdx = -1;
        p_CumAtLastHH = p_CumAtLastLL = 0.0;
    }

    // ---- Compute signed DE ----
    float dP = 0.0f;
    if (useCO) dP = float(sc.Close[idx] - sc.Open[idx]);
    else if (idx > 0) dP = float(sc.Close[idx] - sc.Close[idx - 1]);
    const float dPTicks = dP / float(sc.TickSize);

    const float delta = float(AskVol[idx] - BidVol[idx]);
    const float absDelta = fabsf(delta);
    const float sgnDelta = (delta >= 0.0f ? 1.0f : -1.0f);

    float DE = (dPTicks / (absDelta + eps)) * sgnDelta;
    if (fabsf(DE) > deCap) DE = (DE > 0.0f ? deCap : -deCap);
    SG_DE[idx] = DE;

    // ---- Rolling Z (incremental; update once per bar) ----
    float DE_Z = 0.0f, DeltaAbsZ = 0.0f;

    if (idx != p_LastZIdx)
    {
        // add current
        p_SumDE += DE;
        p_SumSqDE += DE * DE;
        p_SumAbsD += absDelta;
        p_SumSqAbsD += absDelta * absDelta;
        ++p_ZCount;

        // remove oldest if beyond window
        if (p_ZCount > L)
        {
            float oldDE = SG_DE[idx - L];
            float oldAbsD = fabsf(AskVol[idx - L] - BidVol[idx - L]);
            p_SumDE -= oldDE;
            p_SumSqDE -= oldDE * oldDE;
            p_SumAbsD -= oldAbsD;
            p_SumSqAbsD -= oldAbsD * oldAbsD;
            p_ZCount = L;
        }
        p_LastZIdx = idx;
    }

    // compute Zs from sums
    const float n = (float)p_ZCount;
    const float meanDE = (n > 0 ? p_SumDE / n : 0.0f);
    const float varDE  = (n > 1 ? (p_SumSqDE - n * meanDE * meanDE) / (n - 1) : 0.0f);
    const float sdDE   = (varDE > 0.0f ? sqrtf(varDE) : 1.0f);
    DE_Z = (DE - meanDE) / sdDE;
    SG_DE_Z[idx] = DE_Z;

    const float meanAbsD = (n > 0 ? p_SumAbsD / n : 0.0f);
    const float varAbsD  = (n > 1 ? (p_SumSqAbsD - n * meanAbsD * meanAbsD) / (n - 1) : 0.0f);
    const float sdAbsD   = (varAbsD > 0.0f ? sqrtf(varAbsD) : 1.0f);
    DeltaAbsZ = (absDelta - meanAbsD) / sdAbsD;
    SG_DeltaAbsZ[idx] = DeltaAbsZ;

    // ---- Cum delta + simple swing HH/LL ----
    p_CumDelta += delta;

    bool newHH = true, newLL = true;
    for (int i = 1; i <= swingLen; ++i)
    {
        if (idx - i < 0) { newHH = newLL = false; break; }
        if (sc.High[idx] <= sc.High[idx - i]) newHH = false;
        if (sc.Low[idx]  >= sc.Low[idx - i])  newLL = false;
    }

    SG_DivExhUp[idx] = 0.0f;
    SG_DivExhDn[idx] = 0.0f;
    if (newHH)
    {
        if (p_LastHHIdx >= 0 && p_CumDelta < p_CumAtLastHH)
            SG_DivExhUp[idx] = float(sc.High[idx] + sc.TickSize * 1.5f);
        p_LastHHIdx = idx;  p_CumAtLastHH = p_CumDelta;
    }
    if (newLL)
    {
        if (p_LastLLIdx >= 0 && p_CumDelta > p_CumAtLastLL)
            SG_DivExhDn[idx] = float(sc.Low[idx] - sc.TickSize * 1.5f);
        p_LastLLIdx = idx;  p_CumAtLastLL = p_CumDelta;
    }

    // ---- Clear markers ----
    SG_ExhaustUp[idx] = SG_ExhaustDn[idx] = 0.0f;
    SG_AbsorbUp[idx]  = SG_AbsorbDn[idx]  = 0.0f;

    // ---- Optional tape gate ----
    bool tapeDecelOK = true;
    if (tapeID > 0)
    {
        SCFloatArray TapeArr;
        sc.GetStudyArrayUsingID(tapeID, 0, TapeArr);
        if (idx < TapeArr.GetArraySize())
            tapeDecelOK = (TapeArr[idx] <= 0.0f);
    }

    // ---- Conditions ----
    const bool upBar = (sc.Close[idx] >= sc.Open[idx]);

    // Exhaustion: strong |Z(DE)|, low |Δ| (Z<=zDeltaLo), tape decel
    if (fabsf(DE_Z) >= zThr && DeltaAbsZ <= zDeltaLo && tapeDecelOK)
    {
        if (DE_Z > 0.0f && upBar)
            SG_ExhaustUp[idx] = float(sc.High[idx] + sc.TickSize * 1.0f); // offset to avoid clipping
        else if (DE_Z < 0.0f && !upBar)
            SG_ExhaustDn[idx] = float(sc.Low[idx]  - sc.TickSize * 1.0f); // offset to avoid clipping

        if (alertOn) sc.SetAlert(alertNum, "Exhaustion");
    }

    // Absorption: near-zero DE, high |Δ|
    if (fabsf(DE_Z) <= zAbsDE && DeltaAbsZ >= zDeltaHi)
    {
        if (upBar)
            SG_AbsorbUp[idx] = float(sc.High[idx] + sc.TickSize * 1.0f);  // offset to avoid clipping
        else
            SG_AbsorbDn[idx] = float(sc.Low[idx]  - sc.TickSize * 1.0f);  // offset to avoid clipping

        if (alertOn) sc.SetAlert(alertNum, "Absorption");
    }
}
