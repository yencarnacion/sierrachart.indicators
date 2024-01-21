#include "sierrachart.h"

SCDLLName("Custom TSV Study")

SCSFExport scsf_CustomTSVStudy(SCStudyInterfaceRef sc)
{
    SCSubgraphRef TSV = sc.Subgraph[0];
    SCSubgraphRef SMA = sc.Subgraph[1];

    if (sc.SetDefaults)
    {
        sc.GraphName = "Custom TSV Study";
        sc.StudyDescription = "This is a custom implementation of the TSV study.";
        sc.AutoLoop = 0;  // Disable automatic looping

        TSV.Name = "TSV";
        TSV.DrawStyle = DRAWSTYLE_BAR;
        TSV.PrimaryColor = RGB(0,255,0); // Green for positive
        TSV.SecondaryColor = RGB(255,0,0); // Red for negative
        TSV.SecondaryColorUsed = 1; // Enable the use of the secondary color

        SMA.Name = "SMA";
        SMA.DrawStyle = DRAWSTYLE_LINE;
        SMA.PrimaryColor = RGB(0,255,0);

        sc.Input[0].Name = "MA Length";
        sc.Input[0].SetInt(6);
        sc.Input[0].SetIntLimits(1, INT_MAX);

        return;
    }

    int maLength = sc.Input[0].GetInt();
    int currentIndex = sc.ArraySize - 1; // Index of the current bar

    // Check if there is at least one candle to process
    if (currentIndex < 1)
        return;

    // Calculate TSV for the current bar
    float closeChange = sc.Close[currentIndex] - sc.Close[currentIndex - 1];
    TSV[currentIndex] = closeChange * sc.Volume[currentIndex];

    // Set the color of the current bar
    TSV.DataColor[currentIndex] = TSV[currentIndex] >= 0 ? TSV.PrimaryColor : TSV.SecondaryColor;

    // Calculate the moving average of TSV
    sc.SimpleMovAvg(TSV, SMA, maLength);
}
