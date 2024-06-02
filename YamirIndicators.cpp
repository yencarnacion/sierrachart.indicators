#include "sierrachart.h"

SCDLLName("Yamir Indicators")

// Study function
SCSFExport scsf_VolumeDeltaIndicator(SCStudyInterfaceRef sc) {
    // Set configuration variables
    if (sc.SetDefaults) {
        sc.GraphName = "Volume Delta Indicator";
        sc.AutoLoop = 1;  // Automatically loop through each bar
        sc.GraphRegion = 0;  // Main price graph

        // Subgraph configuration
        SCSubgraphRef SubgraphDelta = sc.Subgraph[0];
        SubgraphDelta.Name = "Volume Delta";
        SubgraphDelta.DrawStyle = DRAWSTYLE_BAR;
        SubgraphDelta.PrimaryColor = RGB(0, 191, 255); // Color for buys (blue)
        SubgraphDelta.SecondaryColor = RGB(255, 0, 0); // Color for sells (red)
        SubgraphDelta.SecondaryColorUsed = true;
		SubgraphDelta.LineWidth = 2; // Make the bars twice as thick as default


        // Input configuration
        SCInputRef InputDisplayAsBars = sc.Input[0];
        InputDisplayAsBars.Name = "Display as Bars";
        InputDisplayAsBars.SetYesNo(1);

        return;
    }

    // Define the subgraph and input references
    SCSubgraphRef SubgraphDelta = sc.Subgraph[0];
    SCInputRef InputDisplayAsBars = sc.Input[0];

    // Get the current bar's data
    int index = sc.Index;
    SCFloatArrayRef BidVolume = sc.BaseData[SC_BIDVOL];
    SCFloatArrayRef AskVolume = sc.BaseData[SC_ASKVOL];

    // Calculate volume delta
    float volumeDelta = AskVolume[index] - BidVolume[index];
    SubgraphDelta[index] = volumeDelta;

    // Set the color based on the value of volume delta
    if (volumeDelta >= 0) {
        SubgraphDelta.DataColor[index] = SubgraphDelta.PrimaryColor; // Blue for buys
    } else {
        SubgraphDelta.DataColor[index] = SubgraphDelta.SecondaryColor; // Red for sells
    }

    // Optional: Display as bars
    if (InputDisplayAsBars.GetYesNo()) {
        SubgraphDelta.DrawStyle = DRAWSTYLE_BAR;
    } else {
        SubgraphDelta.DrawStyle = DRAWSTYLE_LINE;
    }
}


SCSFExport scsf_CustomTapeReader(SCStudyInterfaceRef sc)
{
    // Define input variables
    SCInputRef Input_LookbackPeriod = sc.Input[0];

    // Set the study to update on every trade
    sc.UpdateAlways = 1;

    // Define persistent variables
    SCSubgraphRef Subgraph_TapeReader = sc.Subgraph[0];

    // Set default input values
    if (sc.SetDefaults)
    {
        sc.GraphName = "Custom Pace of Tape Reader";
        sc.AutoLoop = 1;
        sc.FreeDLL = 0;

        Subgraph_TapeReader.Name = "Tape Reader";
        Subgraph_TapeReader.DrawStyle = DRAWSTYLE_BAR;
        Subgraph_TapeReader.PrimaryColor = RGB(0, 192, 255);  // Blue for up
        Subgraph_TapeReader.SecondaryColorUsed = 1;
        Subgraph_TapeReader.SecondaryColor = RGB(255, 0, 0);  // Red for down
        Subgraph_TapeReader.LineWidth = 2; // Increase line width to 4

        Input_LookbackPeriod.Name = "Lookback Period";
        Input_LookbackPeriod.SetInt(12);
        Input_LookbackPeriod.SetIntLimits(1, 100);

        return;
    }

    int LookbackPeriod = Input_LookbackPeriod.GetInt();

    // Calculate the change in the number of trades per bar
    int TradeCountChange = 0;
    if (sc.Index >= LookbackPeriod)
    {
        int TradeCountPreviousPeriod = 0;
        int TradeCountCurrentPeriod = 0;

        // Sum trades in the current period
        for (int i = 0; i < LookbackPeriod; i++)
        {
            TradeCountCurrentPeriod += sc.NumberOfTrades[sc.Index - i];
        }

        // Sum trades in the previous period
        for (int i = LookbackPeriod; i < 2 * LookbackPeriod; i++)
        {
            TradeCountPreviousPeriod += sc.NumberOfTrades[sc.Index - i];
        }

        TradeCountChange = TradeCountCurrentPeriod - TradeCountPreviousPeriod;
    }

    Subgraph_TapeReader[sc.Index] = TradeCountChange;

    // Set the color based on the value
    if (TradeCountChange >= 0)
    {
        Subgraph_TapeReader.DataColor[sc.Index] = Subgraph_TapeReader.PrimaryColor;
    }
    else
    {
        Subgraph_TapeReader.DataColor[sc.Index] = Subgraph_TapeReader.SecondaryColor;
    }
}


SCSFExport scsf_CustomPriceChangeTapeReader(SCStudyInterfaceRef sc)
{
    // Define input variables
    SCInputRef Input_LookbackPeriod = sc.Input[0];

    // Set the study to update on every trade
    sc.UpdateAlways = 1;

    // Define persistent variables
    SCSubgraphRef Subgraph_TapeReader = sc.Subgraph[0];

    // Set default input values
    if (sc.SetDefaults)
    {
        sc.GraphName = "Custom Price Change Tape Reader";
        sc.AutoLoop = 1;
        sc.FreeDLL = 0;

        Subgraph_TapeReader.Name = "Tape Reader";
        Subgraph_TapeReader.DrawStyle = DRAWSTYLE_BAR;
        Subgraph_TapeReader.PrimaryColor = RGB(0, 192, 255);  // Blue for up
        Subgraph_TapeReader.SecondaryColorUsed = 1;
        Subgraph_TapeReader.SecondaryColor = RGB(255, 0, 0);  // Red for down
        Subgraph_TapeReader.LineWidth = 4; // Increase line width to 4

        Input_LookbackPeriod.Name = "Lookback Period";
        Input_LookbackPeriod.SetInt(12);
        Input_LookbackPeriod.SetIntLimits(1, 100);

        return;
    }

    int LookbackPeriod = Input_LookbackPeriod.GetInt();

    // Calculate the change in the price
    float PriceChange = 0.0f;
    if (sc.Index >= LookbackPeriod)
    {
        float PreviousPrice = sc.Close[sc.Index - LookbackPeriod];
        float CurrentPrice = sc.Close[sc.Index];

        PriceChange = CurrentPrice - PreviousPrice;
    }

    Subgraph_TapeReader[sc.Index] = PriceChange;

    // Set the color based on the value
    if (PriceChange >= 0)
    {
        Subgraph_TapeReader.DataColor[sc.Index] = Subgraph_TapeReader.PrimaryColor;
    }
    else
    {
        Subgraph_TapeReader.DataColor[sc.Index] = Subgraph_TapeReader.SecondaryColor;
    }
}

