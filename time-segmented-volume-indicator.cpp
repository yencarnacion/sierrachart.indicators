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
        sc.AutoLoop = 1;

        TSV.Name = "TSV";
        TSV.DrawStyle = DRAWSTYLE_BAR;
        TSV.PrimaryColor = RGB(0,255,0); // Green for positive
        TSV.SecondaryColor = RGB(255,0,0); // Red for negative
        TSV.SecondaryColorUsed = 1; // Enable the use of the secondary color

        SMA.Name = "SMA";
        SMA.DrawStyle = DRAWSTYLE_LINE;
        SMA.PrimaryColor = RGB(0,255,0);

        sc.Input[0].Name = "Length";
        sc.Input[0].SetInt(12);
        sc.Input[0].SetIntLimits(1, INT_MAX);

        sc.Input[1].Name = "MA Length";
        sc.Input[1].SetInt(6);
        sc.Input[1].SetIntLimits(1, INT_MAX);

        return;
    }

    int length = sc.Input[0].GetInt();
    int maLength = sc.Input[1].GetInt();

    // Check if there is enough data
    if (sc.ArraySize < length)
        return;

    for (int idx = length; idx < sc.ArraySize; idx++)
    {
        if (idx == length)  // Initial full calculation for the first sum
        {
            for (int backIdx = idx - 1; backIdx >= idx - length; backIdx--)
            {
                float volume = sc.Volume[backIdx];
                float closeChange = sc.Close[backIdx] - sc.Close[backIdx-1];

                TSV[idx] = closeChange*volume;

            }
        }
        else  // Incremental update for subsequent sums
        {
            int newestIdx = idx - 1;
            int oldestIdx = idx - length - 1;

            float volumeNew = sc.Volume[newestIdx];
            float closeChangeNew = sc.Close[newestIdx] - sc.Close[newestIdx-1];

            TSV[idx] = closeChangeNew*volumeNew;

        }


        // Set the color of the bar
        TSV.DataColor[idx] = TSV[idx] >= 0 ? TSV.PrimaryColor : TSV.SecondaryColor;
    }

    sc.SimpleMovAvg(TSV, SMA, maLength);
}
