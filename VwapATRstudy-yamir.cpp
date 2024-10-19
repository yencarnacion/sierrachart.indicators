#include "sierrachart.h"

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
        TextVerticalOffsetTicks.SetInt(100); // Default offset is 100 ticks above the high of the last bar
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
    float CurrentPrice = sc.Close[sc.Index];
    float OpenPrice = sc.Open[sc.Index];
    float ClosePrice = sc.Close[sc.Index];
    float VWAPValue = 0.0f;
    float DailyATRValue = 0.0f;

    // Ensure that the VWAP study data is available
    SCFloatArray VWAPArray;
    if (!sc.GetStudyArrayUsingID(VWAPStudyID.GetStudyID(), VWAPSubgraphIndex.GetInt(), VWAPArray))
    {
        // Unable to retrieve VWAP data; exit
        if (sc.Index == sc.ArraySize - 1)
        {
            sc.AddMessageToLog("Unable to retrieve VWAP data. Check VWAP Study ID and Subgraph Index.", 1);
        }

        return;
    }

    // Ensure that the VWAP value is valid
    VWAPValue = VWAPArray[sc.Index];
    if (VWAPValue == 0.0f)
    {
        if (sc.Index == sc.ArraySize - 1)
        {
            SCString MessageText;
            MessageText.Format("VWAP value is zero at index %d.", sc.Index);
            sc.AddMessageToLog(MessageText, 1);
        }

        return;
    }

    // Ensure that the Daily ATR study data is available
    SCFloatArray ATRArray;
    if (!sc.GetStudyArrayUsingID(ATRStudyID.GetStudyID(), ATRSubgraphIndex.GetInt(), ATRArray))
    {
        // Unable to retrieve ATR data; exit
        if (sc.Index == sc.ArraySize - 1)
        {
            sc.AddMessageToLog("Unable to retrieve Daily ATR data. Check ATR Study ID and Subgraph Index.", 1);
        }

        return;
    }

    // Ensure that the Daily ATR value is valid
    if (sc.Index == 0)
    {
        // No previous bar to get ATR value from
        return;
    }

    DailyATRValue = ATRArray[sc.Index - 1];
    if (DailyATRValue <= 0.0f)
    {
        if (sc.Index == sc.ArraySize - 1)
        {
            SCString MessageText;
            MessageText.Format("Daily ATR value is invalid or zero at index %d.", sc.Index);
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
    sc.Subgraph[1][sc.Index] = UpperBand;  // Overbought threshold band
    sc.Subgraph[2][sc.Index] = LowerBand;  // Oversold threshold band

    // Color the bars based on price action and ATR distance
    // Set the Subgraph value to a non-zero value to ensure the bar is colored
    sc.Subgraph[0][sc.Index] = 1.0f;

    if (ClosePrice > OpenPrice)  // Bullish bar
    {
        if (DistanceFromVWAP > Threshold)
        {
            // Overbought condition: Use OverboughtBarColor
            sc.Subgraph[0].DataColor[sc.Index] = OverboughtBarColor.GetColor();
        }
        else
        {
            // Regular bullish bar: Use BullishBarColor
            sc.Subgraph[0].DataColor[sc.Index] = BullishBarColor.GetColor();
        }
    }
    else if (ClosePrice < OpenPrice)  // Bearish bar
    {
        if (DistanceFromVWAP < -Threshold)
        {
            // Oversold condition: Use OversoldBarColor
            sc.Subgraph[0].DataColor[sc.Index] = OversoldBarColor.GetColor();
        }
        else
        {
            // Regular bearish bar: Use BearishBarColor
            sc.Subgraph[0].DataColor[sc.Index] = BearishBarColor.GetColor();
        }
    }
    else
    {
        // Neutral bar: You can choose to set a default color or skip coloring
        sc.Subgraph[0].DataColor[sc.Index] = BullishBarColor.GetColor();  // Example default color
    }

    // Display the ATR distance values as text above only the most recent bar
    if (sc.Index == sc.ArraySize - 1)
    {
        // Get the current time
        SCDateTime CurrentDateTime = sc.BaseDateTimeIn[sc.Index];
        int CurrentHour = CurrentDateTime.GetHour();
        int CurrentMinute = CurrentDateTime.GetMinute();
        int TimeInMinutes = CurrentHour * 60 + CurrentMinute;

        // Determine the period
        bool IsPeriodA = (TimeInMinutes >= 960 || TimeInMinutes < 570); // After 4 PM or before 9:30 AM
        bool IsPeriodB = (TimeInMinutes >= 570 && TimeInMinutes < 960); // After 9:30 AM and before 4 PM

        int TargetHour, TargetMinute;

        SCString refPriceLabel; // Declare outside to use in Tool.Text

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
        for (int i = sc.Index - 1; i >= 0; --i)
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
            {
                // Use the close of the previous 4 PM bar
                ReferencePrice = sc.Close[ReferenceBarIndex];
            }
            else // IsPeriodB
            {
                // Use the open of the previous 9:30 AM bar
                ReferencePrice = sc.Open[ReferenceBarIndex];
            }

            // Calculate the ATR distance to the reference price
            ATRDistanceToReferencePrice = (CurrentPrice - ReferencePrice) / DailyATRValue;
        }
        else
        {
            // If reference bar not found, set to NaN
            ATRDistanceToReferencePrice = std::numeric_limits<float>::quiet_NaN();
        }

        // Prepare the text to display
        s_UseTool Tool;
        Tool.Clear();  // Clear previous settings
        Tool.ChartNumber = sc.ChartNumber;
        Tool.DrawingType = DRAWING_TEXT;
        Tool.Region = sc.GraphRegion;
        Tool.TextAlignment = DT_LEFT | DT_VCENTER; // Align text to the left
        Tool.Color = TextColor.GetColor(); // Text color from input
        Tool.FontSize = TextFontSize.GetInt(); // Font size from input
        Tool.FontBold = 0;
        Tool.FontBackColor = 0; // Transparent background
        Tool.TransparencyLevel = 100;      // 0 is opaque, 100 is fully transparent

        // Set LineNumber to a unique value
        Tool.LineNumber = 123456; // Unique LineNumber to identify the drawing
        Tool.AddMethod = UTAM_ADD_OR_ADJUST; // Update or add the drawing

        // Set the position of the text at the last bar
        Tool.BeginIndex = sc.Index;

        // Calculate the vertical position using the configurable offset
        int VerticalOffsetTicks = TextVerticalOffsetTicks.GetInt();
        Tool.BeginValue = sc.High[sc.Index] + (sc.TickSize * VerticalOffsetTicks);

        // Round distances to two decimal places
        float RoundedDistanceFromVWAP = roundf(DistanceFromVWAP * 100) / 100;
        float RoundedATRDistanceToReferencePrice = roundf(ATRDistanceToReferencePrice * 100) / 100;

        // Build the text with both lines, including the reference time
        if (!_isnan(ATRDistanceToReferencePrice))
        {
            Tool.Text.Format("VWAP: %.2f\n%s: %.2f", RoundedDistanceFromVWAP, refPriceLabel.GetChars(), RoundedATRDistanceToReferencePrice);
        }
        else
        {
            Tool.Text.Format("VWAP: %.2f\n%s: N/A", RoundedDistanceFromVWAP, refPriceLabel.GetChars());
        }

        sc.UseTool(Tool);
    }
}

