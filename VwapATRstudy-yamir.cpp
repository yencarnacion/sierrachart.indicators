#include "sierrachart.h"
#include <limits>
#include <math.h>

SCDLLName("ATR Distance from VWAP with Bar Colors and ATR Display (Daily ATR) + Dev/DDev")

SCSFExport scsf_VWAPDistanceWithDailyATR(SCStudyInterfaceRef sc)
{
    // -------------------- Inputs (existing) --------------------
    SCInputRef ATRStudyID = sc.Input[0];
    SCInputRef ATRSubgraphIndex = sc.Input[1];
    SCInputRef VWAPStudyID = sc.Input[2];
    SCInputRef VWAPSubgraphIndex = sc.Input[3];
    SCInputRef OverboughtOversoldThreshold = sc.Input[4];
    SCInputRef UpperBandColor = sc.Input[5];
    SCInputRef LowerBandColor = sc.Input[6];
    SCInputRef BullishBarColor = sc.Input[7];
    SCInputRef BearishBarColor = sc.Input[8];
    SCInputRef OverboughtBarColor = sc.Input[9];
    SCInputRef OversoldBarColor = sc.Input[10];
    SCInputRef TextColor = sc.Input[11];
    SCInputRef TextFontSize = sc.Input[12];
    SCInputRef TextVerticalOffsetTicks = sc.Input[13];

    // -------------------- Dev/DDev inputs --------------------
    SCInputRef DDevNearZeroThreshold = sc.Input[14];
    SCInputRef DDevPositiveColor = sc.Input[15];
    SCInputRef DDevNegativeColor = sc.Input[16];

    // ===== RS vs QQQ additions: inputs =====
    SCInputRef EnableRS = sc.Input[17];
    SCInputRef RS_OverlayStudyID = sc.Input[18];
    SCInputRef RS_SubgraphIndex = sc.Input[19];
    SCInputRef RS_LookbackBars = sc.Input[20];
    SCInputRef RS_DeadbandPercent = sc.Input[21];
    SCInputRef RS_ArrowOffsetTicks = sc.Input[22];
    SCInputRef RS_UpColor = sc.Input[23];
    SCInputRef RS_DownColor = sc.Input[24];
    SCInputRef DDevNearZeroColor = sc.Input[25];

    if (sc.SetDefaults)
    {
        sc.GraphName = "ATR Distance from VWAP + Dev/DDev + RS Arrows";
        sc.AutoLoop = 1;
        sc.UpdateAlways = 1; // intrabar updates for live Dev/DDev/RS
        sc.GraphRegion = 0;
        sc.FreeDLL = 0;

        ATRStudyID.Name = "Daily ATR Study ID";
        ATRStudyID.SetStudyID(21);

        ATRSubgraphIndex.Name = "Daily ATR Subgraph Index";
        ATRSubgraphIndex.SetInt(0);
        ATRSubgraphIndex.SetIntLimits(0, 50);

        VWAPStudyID.Name = "VWAP Study ID";
        VWAPStudyID.SetStudyID(13);

        VWAPSubgraphIndex.Name = "VWAP Subgraph Index";
        VWAPSubgraphIndex.SetInt(0);
        VWAPSubgraphIndex.SetIntLimits(0, 50);

        OverboughtOversoldThreshold.Name = "Overbought/Oversold Threshold (Daily ATR Multiples)";
        OverboughtOversoldThreshold.SetFloat(1.5f);

        UpperBandColor.Name = "Upper Band Color";
        UpperBandColor.SetColor(RGB(255, 215, 0));
        LowerBandColor.Name = "Lower Band Color";
        LowerBandColor.SetColor(RGB(255, 215, 0));

        BullishBarColor.Name = "Bullish Bar Color";
        BullishBarColor.SetColor(RGB(0, 255, 0));
        BearishBarColor.Name = "Bearish Bar Color";
        BearishBarColor.SetColor(RGB(255, 0, 0));
        OverboughtBarColor.Name = "Overbought Bar Color";
        OverboughtBarColor.SetColor(RGB(0, 128, 0));
        OversoldBarColor.Name = "Oversold Bar Color";
        OversoldBarColor.SetColor(RGB(255, 0, 255));

        TextColor.Name = "Text Color";
        TextColor.SetColor(RGB(255, 255, 255));
        TextFontSize.Name = "Text Font Size";
        TextFontSize.SetInt(12);
        TextFontSize.SetIntLimits(1, 50);

        TextVerticalOffsetTicks.Name = "Text Vertical Offset (Ticks)";
        TextVerticalOffsetTicks.SetInt(100);
        TextVerticalOffsetTicks.SetIntLimits(0, INT_MAX);

        // Price colorer
        sc.Subgraph[0].Name = "Colored Bars Based on ATR Distance and Price Action";
        sc.Subgraph[0].DrawStyle = DRAWSTYLE_COLOR_BAR;
        sc.Subgraph[0].LineWidth = 2;
        sc.Subgraph[0].PrimaryColor = BullishBarColor.GetColor();
        sc.Subgraph[0].SecondaryColor = BearishBarColor.GetColor();
        sc.Subgraph[0].SecondaryColorUsed = 1;
        sc.Subgraph[0].DrawZeros = false;

        // VWAP bands
        sc.Subgraph[1].Name = "VWAP + Threshold * Daily ATR";
        sc.Subgraph[1].DrawStyle = DRAWSTYLE_LINE;
        sc.Subgraph[1].PrimaryColor = UpperBandColor.GetColor();
        sc.Subgraph[1].LineWidth = 1;
        sc.Subgraph[1].DrawZeros = false;

        sc.Subgraph[2].Name = "VWAP - Threshold * Daily ATR";
        sc.Subgraph[2].DrawStyle = DRAWSTYLE_LINE;
        sc.Subgraph[2].PrimaryColor = LowerBandColor.GetColor();
        sc.Subgraph[2].LineWidth = 1;
        sc.Subgraph[2].DrawZeros = false;

        // Dev/DDev subgraphs
        sc.Subgraph[3].Name = "Dev = (Price - VWAP) / ATR";
        sc.Subgraph[3].DrawStyle = DRAWSTYLE_LINE;
        sc.Subgraph[3].PrimaryColor = RGB(255, 255, 255);
        sc.Subgraph[3].LineWidth = 2;
        sc.Subgraph[3].DrawZeros = true;

        sc.Subgraph[4].Name = "DDev = Dev - PrevDev";
        sc.Subgraph[4].DrawStyle = DRAWSTYLE_BAR;
        sc.Subgraph[4].PrimaryColor = RGB(86, 180, 233);   // Okabe-Ito sky blue
        sc.Subgraph[4].SecondaryColor = RGB(213, 94, 0);   // Okabe-Ito vermillion
        sc.Subgraph[4].SecondaryColorUsed = 1;
        sc.Subgraph[4].LineWidth = 2;
        sc.Subgraph[4].DrawZeros = true;

        sc.Subgraph[5].Name = "DDev Zero Line";
        sc.Subgraph[5].DrawStyle = DRAWSTYLE_LINE;
        sc.Subgraph[5].PrimaryColor = RGB(90, 90, 90);
        sc.Subgraph[5].LineWidth = 1;
        sc.Subgraph[5].DrawZeros = true;

        sc.Subgraph[6].Name = "Unused";
        sc.Subgraph[6].DrawStyle = DRAWSTYLE_IGNORE;
        sc.Subgraph[6].DrawZeros = false;

        DDevNearZeroThreshold.Name = "DDev Near-Zero Threshold";
        DDevNearZeroThreshold.SetFloat(0.03f);

        DDevPositiveColor.Name = "DDev Color Above 0";
        DDevPositiveColor.SetColor(RGB(86, 180, 233)); // colorblind-safe

        DDevNegativeColor.Name = "DDev Color Below 0";
        DDevNegativeColor.SetColor(RGB(213, 94, 0)); // colorblind-safe

        // ===== RS vs QQQ additions: default inputs =====
        EnableRS.Name = "RS vs QQQ: Enable";
        EnableRS.SetYesNo(1);

        RS_OverlayStudyID.Name = "RS: QQQ Overlay Study ID (Study/Price Overlay of QQQ Close)";
        RS_OverlayStudyID.SetStudyID(0); // set this to your overlay’s ID

        RS_SubgraphIndex.Name = "RS: Overlay Subgraph Index";
        RS_SubgraphIndex.SetInt(0);
        RS_SubgraphIndex.SetIntLimits(0, 50);

        RS_LookbackBars.Name = "RS: Lookback (bars)";
        RS_LookbackBars.SetInt(3);
        RS_LookbackBars.SetIntLimits(1, 1000);

        RS_DeadbandPercent.Name = "RS: Deadband (%) to filter noise";
        RS_DeadbandPercent.SetFloat(0.03f);

        RS_ArrowOffsetTicks.Name = "RS: Arrow Offset (ticks below low)";
        RS_ArrowOffsetTicks.SetInt(2);
        RS_ArrowOffsetTicks.SetIntLimits(0, 100);

        RS_UpColor.Name = "RS Up Arrow Color (Stronger than QQQ)";
        RS_UpColor.SetColor(RGB(255, 255, 255)); // white

        RS_DownColor.Name = "RS Down Arrow Color (Weaker than QQQ)";
        RS_DownColor.SetColor(RGB(255, 140, 0)); // orange

        DDevNearZeroColor.Name = "DDev Color Near/At 0";
        DDevNearZeroColor.SetColor(RGB(240, 228, 66)); // Okabe-Ito yellow

        // ===== RS vs QQQ additions: subgraphs =====
        sc.Subgraph[7].Name = "RS Up Arrow (vs QQQ)";
        sc.Subgraph[7].DrawStyle = DRAWSTYLE_TRIANGLE_UP;
        sc.Subgraph[7].PrimaryColor = RS_UpColor.GetColor();
        sc.Subgraph[7].LineWidth = 3;
        sc.Subgraph[7].DrawZeros = false;

        sc.Subgraph[8].Name = "RS Down Arrow (vs QQQ)";
        sc.Subgraph[8].DrawStyle = DRAWSTYLE_TRIANGLE_DOWN;
        sc.Subgraph[8].PrimaryColor = RS_DownColor.GetColor();
        sc.Subgraph[8].LineWidth = 3;
        sc.Subgraph[8].DrawZeros = false;

        return;
    }

    // Sync colors if changed
    sc.Subgraph[0].PrimaryColor  = BullishBarColor.GetColor();
    sc.Subgraph[0].SecondaryColor= BearishBarColor.GetColor();
    sc.Subgraph[1].PrimaryColor  = UpperBandColor.GetColor();
    sc.Subgraph[2].PrimaryColor  = LowerBandColor.GetColor();
    sc.Subgraph[4].PrimaryColor  = DDevPositiveColor.GetColor();
    sc.Subgraph[4].SecondaryColor= DDevNegativeColor.GetColor();
    // ===== RS vs QQQ additions: keep arrow colors in sync =====
    sc.Subgraph[7].PrimaryColor  = RS_UpColor.GetColor();
    sc.Subgraph[8].PrimaryColor  = RS_DownColor.GetColor();

    const int idx = sc.Index;

    // -------------------- Pull VWAP & ATR --------------------
    float CurrentPrice = sc.Close[idx];
    float OpenPrice = sc.Open[idx];
    float ClosePrice = sc.Close[idx];
    float VWAPValue = 0.0f;
    float DailyATRValue = 0.0f;

    SCFloatArray VWAPArray;
    if (!sc.GetStudyArrayUsingID(VWAPStudyID.GetStudyID(), VWAPSubgraphIndex.GetInt(), VWAPArray))
    {
        if (idx == sc.ArraySize - 1)
            sc.AddMessageToLog("Unable to retrieve VWAP data. Check VWAP Study ID and Subgraph Index.", 1);
        return;
    }

    VWAPValue = VWAPArray[idx];
    if (VWAPValue == 0.0f)
    {
        if (idx == sc.ArraySize - 1)
        {
            SCString MessageText;
            MessageText.Format("VWAP value is zero at index %d.", idx);
            sc.AddMessageToLog(MessageText, 1);
        }
        return;
    }

    SCFloatArray ATRArray;
    if (!sc.GetStudyArrayUsingID(ATRStudyID.GetStudyID(), ATRSubgraphIndex.GetInt(), ATRArray))
    {
        if (idx == sc.ArraySize - 1)
            sc.AddMessageToLog("Unable to retrieve Daily ATR data. Check ATR Study ID and Subgraph Index.", 1);
        return;
    }

    if (idx == 0)
        return;

    DailyATRValue = ATRArray[idx - 1];
    if (DailyATRValue <= 0.0f)
    {
        if (idx == sc.ArraySize - 1)
        {
            SCString MessageText;
            MessageText.Format("Daily ATR value is invalid or zero at index %d.", idx);
            sc.AddMessageToLog(MessageText, 1);
        }
        return;
    }

    // -------------------- ATR distance, Dev/DDev, and bands --------------------
    float DistanceFromVWAP = (CurrentPrice - VWAPValue) / DailyATRValue;
    const double Dev = ((double)CurrentPrice - (double)VWAPValue) / (double)DailyATRValue;

    // Compute previous Dev directly from source arrays to avoid any display/precision truncation.
    double PrevDev = Dev;
    if (idx >= 2)
    {
        const float PrevVWAPValue = VWAPArray[idx - 1];
        const float PrevATRValue = ATRArray[idx - 2];
        if (PrevVWAPValue != 0.0f && PrevATRValue > 0.0f)
            PrevDev = ((double)sc.Close[idx - 1] - (double)PrevVWAPValue) / (double)PrevATRValue;
    }

    const double DDev = Dev - PrevDev;
    const double NearZeroThreshold = fabs((double)DDevNearZeroThreshold.GetFloat());
    float Threshold = OverboughtOversoldThreshold.GetFloat();

    float UpperBand = VWAPValue + (Threshold * DailyATRValue);
    float LowerBand = VWAPValue - (Threshold * DailyATRValue);

    sc.Subgraph[1][idx] = UpperBand;
    sc.Subgraph[2][idx] = LowerBand;
    sc.Subgraph[3][idx] = (float)Dev;
    sc.Subgraph[4][idx] = (float)DDev;
    sc.Subgraph[5][idx] = 0.0f;

    if (fabs(DDev) <= NearZeroThreshold)
        sc.Subgraph[4].DataColor[idx] = DDevNearZeroColor.GetColor();
    else if (DDev > 0.0)
        sc.Subgraph[4].DataColor[idx] = DDevPositiveColor.GetColor();
    else
        sc.Subgraph[4].DataColor[idx] = DDevNegativeColor.GetColor();

    // Color bars
    sc.Subgraph[0][idx] = 1.0f;
    if (ClosePrice > OpenPrice)
        sc.Subgraph[0].DataColor[idx] = (DistanceFromVWAP > Threshold)
            ? OverboughtBarColor.GetColor() : BullishBarColor.GetColor();
    else if (ClosePrice < OpenPrice)
        sc.Subgraph[0].DataColor[idx] = (DistanceFromVWAP < -Threshold)
            ? OversoldBarColor.GetColor() : BearishBarColor.GetColor();
    else
        sc.Subgraph[0].DataColor[idx] = BullishBarColor.GetColor();

    const SCDateTime BarDT = sc.BaseDateTimeIn[idx];

    // ===== RS vs QQQ additions: compute & draw arrows =====
    if (EnableRS.GetYesNo())
    {
        SCFloatArray RSRef; // QQQ overlay values
        if (!sc.GetStudyArrayUsingID(RS_OverlayStudyID.GetStudyID(), RS_SubgraphIndex.GetInt(), RSRef))
        {
            if (idx == sc.ArraySize - 1)
                sc.AddMessageToLog("RS: Could not get QQQ overlay study array. Ensure 'Study/Price Overlay' is added and the Study ID/Subgraph index are correct.", 1);
        }
        else
        {
            const int N = RS_LookbackBars.GetInt();
            // Default: 1-bar ROC vs 1-bar ROC, or N-bar ROC vs N-bar ROC
            if (idx >= N
                && RSRef[idx] != 0.0f && RSRef[idx - N] != 0.0f
                && sc.Close[idx - N] != 0.0f)
            {
                const float symRet = (sc.Close[idx] / sc.Close[idx - N]) - 1.0f;
                const float refRet = (RSRef[idx]     / RSRef[idx - N]) - 1.0f;
                const float delta  = symRet - refRet;

                const float deadband = RS_DeadbandPercent.GetFloat() * 0.01f; // % → fraction
                const double y = (double)sc.Low[idx] - (double)sc.TickSize * (double)RS_ArrowOffsetTicks.GetInt();

                // Clear both by default
                sc.Subgraph[7][idx] = 0.0f; // up
                sc.Subgraph[8][idx] = 0.0f; // down

                if (delta > deadband)
                    sc.Subgraph[7][idx] = (float)y; // yellow up
                else if (delta < -deadband)
                    sc.Subgraph[8][idx] = (float)y; // orange down
            }
            else
            {
                sc.Subgraph[7][idx] = 0.0f;
                sc.Subgraph[8][idx] = 0.0f;
            }
        }
    }
    else
    {
        sc.Subgraph[7][idx] = 0.0f;
        sc.Subgraph[8][idx] = 0.0f;
    }
    // ===== end RS vs QQQ additions =====

    // -------------------- Build stacked text on last bar --------------------
    if (idx == sc.ArraySize - 1)
    {
        // Reference line: 16:00 after hours, otherwise 9:30 intraday
        int CurrentHour = BarDT.GetHour();
        int CurrentMinute = BarDT.GetMinute();
        int TimeInMinutes = CurrentHour * 60 + CurrentMinute;

        bool IsPeriodA = (TimeInMinutes >= 960 || TimeInMinutes < 570); // After 16:00 or before 09:30
        bool IsPeriodB = (TimeInMinutes >= 570 && TimeInMinutes < 960); // 09:30–16:00

        int TargetHour, TargetMinute;
        SCString refPriceLabel;

        if (IsPeriodA)
        {
            TargetHour = 16; TargetMinute = 0; refPriceLabel = "16:00";
        }
        else
        {
            TargetHour = 9;  TargetMinute = 30; refPriceLabel = "9:30";
        }

        int ReferenceBarIndex = -1;
        for (int i = idx - 1; i >= 0; --i)
        {
            SCDateTime BarDateTime = sc.BaseDateTimeIn[i];
            int BarHour = BarDateTime.GetHour();
            int BarMinute = BarDateTime.GetMinute();
            if (BarHour == TargetHour && BarMinute == TargetMinute)
            {
                ReferenceBarIndex = i;
                break;
            }
        }

        float ATRDistanceToReferencePrice = 0.0f;
        if (ReferenceBarIndex >= 0)
        {
            float ReferencePrice = IsPeriodA ? sc.Close[ReferenceBarIndex] : sc.Open[ReferenceBarIndex];
            ATRDistanceToReferencePrice = (CurrentPrice - ReferencePrice) / DailyATRValue;
        }
        else
        {
            ATRDistanceToReferencePrice = std::numeric_limits<float>::quiet_NaN();
        }

        // Adaptive placement (same as before)
        const double ticksize = sc.TickSize;
        const int barRangeTicks = (int)ceil(((sc.High[idx] - sc.Low[idx]) / ticksize > 0.0) ? ((sc.High[idx] - sc.Low[idx]) / ticksize) : 0.0);
        const int atrTicks      = (int)ceil((DailyATRValue / ticksize > 0.0) ? (DailyATRValue / ticksize) : 0.0);

        const int minClearanceTicks = 3;
        const int upperFromATR = ((int)ceil(0.25 * atrTicks) > 8) ? (int)ceil(0.25 * atrTicks) : 8;
        const int upperFromBar = ((int)ceil(0.35 * barRangeTicks) > 6) ? (int)ceil(0.35 * barRangeTicks) : 6;
        const int absCapTicks = 16;

        int smallerUpper = (upperFromATR < upperFromBar) ? upperFromATR : upperFromBar;
        int cappedUpper  = (absCapTicks < smallerUpper) ? absCapTicks : smallerUpper;
        int maxReasonableTicks = (minClearanceTicks > cappedUpper) ? minClearanceTicks : cappedUpper;

        int effectiveOffsetTicks = TextVerticalOffsetTicks.GetInt();
        if (effectiveOffsetTicks < minClearanceTicks)
            effectiveOffsetTicks = minClearanceTicks;
        if (effectiveOffsetTicks > maxReasonableTicks)
            effectiveOffsetTicks = maxReasonableTicks;

        // Prepare the text tool
        s_UseTool Tool;
        Tool.Clear();
        Tool.ChartNumber = sc.ChartNumber;
        Tool.DrawingType = DRAWING_TEXT;
        Tool.Region = sc.GraphRegion;
        Tool.TextAlignment = DT_LEFT | DT_BOTTOM;
        Tool.Color = TextColor.GetColor();
        Tool.FontSize = TextFontSize.GetInt();
        Tool.FontBold = 0;
        Tool.FontBackColor = 0;
        Tool.TransparencyLevel = 100;
        Tool.LineNumber = 123456; // update in place
        Tool.AddMethod = UTAM_ADD_OR_ADJUST;
        Tool.BeginIndex = idx;
        Tool.BeginValue = (float)(sc.High[idx] + ticksize * effectiveOffsetTicks);

        // ---- Build lines in requested order: Dev -> DDev -> 9:30/16:00 ----
        SCString lineDev;
        SCString lineDDev;
        SCString lineRef;
        lineDev.Format("Dev: %.2f", roundf(Dev * 100.0f) / 100.0f);
        lineDDev.Format("DDev: %.4f", DDev);

        if (!_isnan(ATRDistanceToReferencePrice))
        {
            const float RoundedATRToRef = roundf(ATRDistanceToReferencePrice * 100.0f) / 100.0f;
            lineRef.Format("%s: %.2f", refPriceLabel.GetChars(), RoundedATRToRef);
        }
        else
        {
            lineRef.Format("%s: N/A", refPriceLabel.GetChars());
        }

        SCString finalText;
        finalText.Format("%s\n%s\n%s",
                         lineDev.GetChars(),
                         lineDDev.GetChars(),
                         lineRef.GetChars());

        Tool.Text = finalText;
        sc.UseTool(Tool);
    }
}
