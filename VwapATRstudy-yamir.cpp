#include "sierrachart.h"
#include <limits>
#include <math.h>

SCDLLName("ATR Distance from VWAP with Bar Colors and ATR Display (Daily ATR)")

SCSFExport scsf_VWAPDistanceWithDailyATR(SCStudyInterfaceRef sc)
{
    // Input settings
    SCInputRef ATRStudyID = sc.Input[0];
    SCInputRef ATRSubgraphIndex = sc.Input[1];
    SCInputRef VWAPStudyID = sc.Input[2];
    SCInputRef VWAPSubgraphIndex = sc.Input[3];
    SCInputRef OverboughtOversoldThreshold = sc.Input[4];  // Adjustable threshold input
    SCInputRef UpperBandColor = sc.Input[5];
    SCInputRef LowerBandColor = sc.Input[6];
    SCInputRef BullishBarColor = sc.Input[7];
    SCInputRef BearishBarColor = sc.Input[8];
    SCInputRef OverboughtBarColor = sc.Input[9];
    SCInputRef OversoldBarColor = sc.Input[10];
    SCInputRef TextColor = sc.Input[11];
    SCInputRef TextFontSize = sc.Input[12];
    SCInputRef TextVerticalOffsetTicks = sc.Input[13];

    // Set the default settings and inputs
    if (sc.SetDefaults)
    {
        // Study configuration
        sc.GraphName = "ATR Distance from VWAP with Bar Colors and ATR Display (Daily ATR)";
        sc.AutoLoop = 1;                // Automatically loop through each bar
        sc.GraphRegion = 0;             // Draw on the main price graph
        sc.FreeDLL = 0;

        // Inputs for referencing the ATR study
        ATRStudyID.Name = "Daily ATR Study ID";
        ATRStudyID.SetStudyID(21);      // Default to Study ID 21 (adjust as needed)

        ATRSubgraphIndex.Name = "Daily ATR Subgraph Index";
        ATRSubgraphIndex.SetInt(0);    // Index of ATR within the referenced study
        ATRSubgraphIndex.SetIntLimits(0, 50);

        // Inputs for referencing the VWAP study
        VWAPStudyID.Name = "VWAP Study ID";
        VWAPStudyID.SetStudyID(13);      // Replace with your actual VWAP study ID

        VWAPSubgraphIndex.Name = "VWAP Subgraph Index";
        VWAPSubgraphIndex.SetInt(0);    // Index of VWAP within the referenced study
        VWAPSubgraphIndex.SetIntLimits(0, 50);

        // Adjustable threshold for overbought/oversold conditions
        OverboughtOversoldThreshold.Name = "Overbought/Oversold Threshold (Daily ATR Multiples)";
        OverboughtOversoldThreshold.SetFloat(1.5f);  // Default threshold is 1.5 times Daily ATR

        // Inputs for Upper and Lower Band Colors
        UpperBandColor.Name = "Upper Band Color";
        UpperBandColor.SetColor(RGB(255, 215, 0));   // Default Gold color

        LowerBandColor.Name = "Lower Band Color";
        LowerBandColor.SetColor(RGB(255, 215, 0));  // Default Gold color

        // Inputs for Bar Colors
        BullishBarColor.Name = "Bullish Bar Color";
        BullishBarColor.SetColor(RGB(0, 255, 0));    // Default Green

        BearishBarColor.Name = "Bearish Bar Color";
        BearishBarColor.SetColor(RGB(255, 0, 0));    // Default Red

        OverboughtBarColor.Name = "Overbought Bar Color";
        OverboughtBarColor.SetColor(RGB(0, 128, 0)); // Default Dark Green

        OversoldBarColor.Name = "Oversold Bar Color";
        OversoldBarColor.SetColor(RGB(255, 0, 255)); // Default Magenta

        // Input for Text Color and Font Size
        TextColor.Name = "Text Color";
        TextColor.SetColor(RGB(255, 255, 255)); // Default White

        TextFontSize.Name = "Text Font Size";
        TextFontSize.SetInt(12); // Default font size
        TextFontSize.SetIntLimits(1, 50);

        // Input for Text Vertical Offset
        TextVerticalOffsetTicks.Name = "Text Vertical Offset (Ticks)";
        TextVerticalOffsetTicks.SetInt(100); // User-requested offset; will be clamped at runtime
        TextVerticalOffsetTicks.SetIntLimits(0, INT_MAX);

        // Subgraph for coloring bars based on price action and ATR distance
        sc.Subgraph[0].Name = "Colored Bars Based on ATR Distance and Price Action";
        sc.Subgraph[0].DrawStyle = DRAWSTYLE_COLOR_BAR;
        sc.Subgraph[0].LineWidth = 2;
        sc.Subgraph[0].PrimaryColor = BullishBarColor.GetColor();    // Set default colors
        sc.Subgraph[0].SecondaryColor = BearishBarColor.GetColor();
        sc.Subgraph[0].SecondaryColorUsed = 1;  // Enable secondary color
        sc.Subgraph[0].DrawZeros = false;

        // Subgraphs for VWAP bands (overbought/oversold thresholds)
        sc.Subgraph[1].Name = "VWAP + Threshold * Daily ATR";      // Upper band
        sc.Subgraph[1].DrawStyle = DRAWSTYLE_LINE;
        sc.Subgraph[1].PrimaryColor = UpperBandColor.GetColor();  // Use color from input
        sc.Subgraph[1].LineWidth = 1;
        sc.Subgraph[1].DrawZeros = false;

        sc.Subgraph[2].Name = "VWAP - Threshold * Daily ATR";      // Lower band
        sc.Subgraph[2].DrawStyle = DRAWSTYLE_LINE;
        sc.Subgraph[2].PrimaryColor = LowerBandColor.GetColor();  // Use color from input
        sc.Subgraph[2].LineWidth = 1;
        sc.Subgraph[2].DrawZeros = false;

        return;
    }

    // Ensure that subgraph colors are updated if inputs change
    sc.Subgraph[0].PrimaryColor = BullishBarColor.GetColor();
    sc.Subgraph[0].SecondaryColor = BearishBarColor.GetColor();
    sc.Subgraph[1].PrimaryColor = UpperBandColor.GetColor();
    sc.Subgraph[2].PrimaryColor = LowerBandColor.GetColor();

    // Variables for storing computed values
    const int idx = sc.Index;
    float CurrentPrice = sc.Close[idx];
    float OpenPrice = sc.Open[idx];
    float ClosePrice = sc.Close[idx];
    float VWAPValue = 0.0f;
    float DailyATRValue = 0.0f;

    // Ensure that the VWAP study data is available
    SCFloatArray VWAPArray;
    if (!sc.GetStudyArrayUsingID(VWAPStudyID.GetStudyID(), VWAPSubgraphIndex.GetInt(), VWAPArray))
    {
        if (idx == sc.ArraySize - 1)
            sc.AddMessageToLog("Unable to retrieve VWAP data. Check VWAP Study ID and Subgraph Index.", 1);
        return;
    }

    // Ensure that the VWAP value is valid
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

    // Ensure that the Daily ATR study data is available
    SCFloatArray ATRArray;
    if (!sc.GetStudyArrayUsingID(ATRStudyID.GetStudyID(), ATRSubgraphIndex.GetInt(), ATRArray))
    {
        if (idx == sc.ArraySize - 1)
            sc.AddMessageToLog("Unable to retrieve Daily ATR data. Check ATR Study ID and Subgraph Index.", 1);
        return;
    }

    // Ensure that the Daily ATR value is valid
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

    // Calculate the distance from VWAP in terms of Daily ATR
    float DistanceFromVWAP = (CurrentPrice - VWAPValue) / DailyATRValue;

    // Calculate and plot the overbought and oversold bands
    float Threshold = OverboughtOversoldThreshold.GetFloat();

    // Upper and lower bands around VWAP
    float UpperBand = VWAPValue + (Threshold * DailyATRValue);
    float LowerBand = VWAPValue - (Threshold * DailyATRValue);

    // Plot the bands on the chart
    sc.Subgraph[1][idx] = UpperBand;  // Overbought threshold band
    sc.Subgraph[2][idx] = LowerBand;  // Oversold threshold band

    // Color the bars based on price action and ATR distance
    sc.Subgraph[0][idx] = 1.0f; // ensure bar is "active" for coloring

    if (ClosePrice > OpenPrice)  // Bullish bar
    {
        if (DistanceFromVWAP > Threshold)
            sc.Subgraph[0].DataColor[idx] = OverboughtBarColor.GetColor();
        else
            sc.Subgraph[0].DataColor[idx] = BullishBarColor.GetColor();
    }
    else if (ClosePrice < OpenPrice)  // Bearish bar
    {
        if (DistanceFromVWAP < -Threshold)
            sc.Subgraph[0].DataColor[idx] = OversoldBarColor.GetColor();
        else
            sc.Subgraph[0].DataColor[idx] = BearishBarColor.GetColor();
    }
    else
    {
        sc.Subgraph[0].DataColor[idx] = BullishBarColor.GetColor();  // neutral default
    }

    // Display the ATR distance values as text above only the most recent bar
    if (idx == sc.ArraySize - 1)
    {
        // Get the current time
        SCDateTime CurrentDateTime = sc.BaseDateTimeIn[idx];
        int CurrentHour = CurrentDateTime.GetHour();
        int CurrentMinute = CurrentDateTime.GetMinute();
        int TimeInMinutes = CurrentHour * 60 + CurrentMinute;

        // Determine the period
        bool IsPeriodA = (TimeInMinutes >= 960 || TimeInMinutes < 570); // After 4 PM or before 9:30 AM
        bool IsPeriodB = (TimeInMinutes >= 570 && TimeInMinutes < 960); // After 9:30 AM and before 4 PM

        int TargetHour, TargetMinute;

        SCString refPriceLabel;

        if (IsPeriodA)
        {
            TargetHour = 16; // 4 PM
            TargetMinute = 0;
            refPriceLabel = "16:00";
        }
        else // IsPeriodB
        {
            TargetHour = 9;
            TargetMinute = 30;
            refPriceLabel = "9:30";
        }

        // Find the previous bar with the target time
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
            float ReferencePrice;
            if (IsPeriodA)
                ReferencePrice = sc.Close[ReferenceBarIndex]; // previous 4 PM close
            else
                ReferencePrice = sc.Open[ReferenceBarIndex];  // previous 9:30 open

            ATRDistanceToReferencePrice = (CurrentPrice - ReferencePrice) / DailyATRValue;
        }
        else
        {
            ATRDistanceToReferencePrice = std::numeric_limits<float>::quiet_NaN();
        }

        // -------- Adaptive text placement (tick-based, clamped) --------
        const double tick = sc.TickSize;

        // bar range & ATR in ticks (non-negative)
        const int barRangeTicks = (int)ceil(((sc.High[idx] - sc.Low[idx]) / tick > 0.0) ? ((sc.High[idx] - sc.Low[idx]) / tick) : 0.0);
        const int atrTicks      = (int)ceil((DailyATRValue / tick > 0.0) ? (DailyATRValue / tick) : 0.0);

        // Minimum clearance (keeps text above bar/markers)
        const int minClearanceTicks = 3;

        // Reasonable upper bounds
        const int upperFromATR = ((int)ceil(0.25 * atrTicks) > 8) ? (int)ceil(0.25 * atrTicks) : 8;
        const int upperFromBar = ((int)ceil(0.35 * barRangeTicks) > 6) ? (int)ceil(0.35 * barRangeTicks) : 6;

        // Absolute cap so text never goes too far away
        const int absCapTicks = 16;

        // maxReasonableTicks = max(minClearance, min(absCap, min(upperFromATR, upperFromBar)))
        int smallerUpper = (upperFromATR < upperFromBar) ? upperFromATR : upperFromBar;
        int cappedUpper  = (absCapTicks < smallerUpper) ? absCapTicks : smallerUpper;
        int maxReasonableTicks = (minClearanceTicks > cappedUpper) ? minClearanceTicks : cappedUpper;

        // Requested offset from input, clamped to [minClearance, maxReasonable]
        int effectiveOffsetTicks = TextVerticalOffsetTicks.GetInt();
        if (effectiveOffsetTicks < minClearanceTicks)
            effectiveOffsetTicks = minClearanceTicks;
        if (effectiveOffsetTicks > maxReasonableTicks)
            effectiveOffsetTicks = maxReasonableTicks;

        // Prepare and place the text
        s_UseTool Tool;
        Tool.Clear();
        Tool.ChartNumber = sc.ChartNumber;
        Tool.DrawingType = DRAWING_TEXT;
        Tool.Region = sc.GraphRegion;

        // Anchor bottom of text at BeginValue so it grows upward (keeps bar/markers visible)
        Tool.TextAlignment = DT_LEFT | DT_BOTTOM;

        Tool.Color = TextColor.GetColor();
        Tool.FontSize = TextFontSize.GetInt();
        Tool.FontBold = 0;
        Tool.FontBackColor = 0;          // Transparent background
        Tool.TransparencyLevel = 100;    // 0=opaque, 100=fully transparent

        Tool.LineNumber = 123456;
        Tool.AddMethod = UTAM_ADD_OR_ADJUST;

        Tool.BeginIndex = idx;
        Tool.BeginValue = (float)(sc.High[idx] + tick * effectiveOffsetTicks);

        // Round distances to two decimals
        float RoundedDistanceFromVWAP = roundf(DistanceFromVWAP * 100.0f) / 100.0f;

        // Build the text with both lines, including the reference time
        if (!_isnan(ATRDistanceToReferencePrice))
        {
            float RoundedATRDistanceToReferencePrice = roundf(ATRDistanceToReferencePrice * 100.0f) / 100.0f;
            Tool.Text.Format("VWAP: %.2f\n%s: %.2f", RoundedDistanceFromVWAP, refPriceLabel.GetChars(), RoundedATRDistanceToReferencePrice);
        }
        else
        {
            Tool.Text.Format("VWAP: %.2f\n%s: N/A", RoundedDistanceFromVWAP, refPriceLabel.GetChars());
        }

        sc.UseTool(Tool);
    }
}
