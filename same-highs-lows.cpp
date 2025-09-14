// Equal Highs and Lows (ACSIL) — Extend to Current Bar Only
// Efficient port of the TradingView script by @jzstur, with an option to
// extend each equal H/L line to the *current (rightmost) bar only* (not a ray).
//
// Fixes for your build:
// - Use DRAWING_LINE (not DRAWING_TRENDLINE).
// - LineStyle uses SubgraphLineStyles enum (no int conversions).
//
// © 2025

#include "sierrachart.h"
#include <cmath>

SCDLLName("Equal Highs and Lows (ACSIL)")

// Map custom input index to Sierra Chart line style enum
static inline SubgraphLineStyles MapStyleIndexToSCStyle(const int idx)
{
    switch (idx)
    {
    case 1: return LINESTYLE_DASH; // "Dash"
    case 2: return LINESTYLE_DOT;  // "Dotted"
    default: return LINESTYLE_SOLID; // "Solid"
    }
}

// Equality with price tolerance
static inline bool PriceEqual(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

// Unique base offsets to avoid id collisions between high/low lines & labels
enum : int
{
    LN_BASE_HIGH       = 100000000, // + bar index
    LN_BASE_LOW        = 200000000, // + bar index
    LN_BASE_HIGH_LABEL = 300000000, // + bar index
    LN_BASE_LOW_LABEL  = 400000000  // + bar index
};

SCSFExport scsf_EqualHighsAndLows(SCStudyInterfaceRef sc)
{
    if (sc.SetDefaults)
    {
        sc.GraphName  = "Equal Highs and Lows (Efficient, Extend-to-Current)";
        sc.StudyDescription =
            "Draws lines between equal highs and equal lows within a lookback, "
            "breaking the search early on a higher-high/lower-low. Option to extend "
            "each line to the current (rightmost) bar only (not a ray).";
        sc.GraphRegion = 0;
        sc.AutoLoop    = 1;
        sc.FreeDLL     = 0;

        // ---- Inputs
        SCInputRef Lookback = sc.Input[0];
        Lookback.Name = "Lookback Length (bars)";
        Lookback.SetInt(200);
        Lookback.SetIntLimits(1, 500000);

        SCInputRef TolTicks = sc.Input[1];
        TolTicks.Name = "Equality Tolerance (ticks)";
        TolTicks.SetInt(1);
        TolTicks.SetIntLimits(0, 10);

        SCInputRef ColorHigh = sc.Input[2];
        ColorHigh.Name = "Line Color - Highs";
        ColorHigh.SetColor(RGB(255, 0, 0));

        SCInputRef ColorLow = sc.Input[3];
        ColorLow.Name = "Line Color - Lows";
        ColorLow.SetColor(RGB(0, 170, 0));

        SCInputRef LineWidth = sc.Input[4];
        LineWidth.Name = "Line Width";
        LineWidth.SetInt(1);
        LineWidth.SetIntLimits(1, 5);

        SCInputRef LineStyleIn = sc.Input[5];
        LineStyleIn.Name = "Line Style";
        LineStyleIn.SetCustomInputStrings("Solid;Dash;Dotted");
        LineStyleIn.SetCustomInputIndex(0); // Solid

        SCInputRef ShowLabels = sc.Input[6];
        ShowLabels.Name = "Show Debug Labels";
        ShowLabels.SetYesNo(false);

        SCInputRef ExtendToCurrent = sc.Input[7];
        ExtendToCurrent.Name = "Extend Lines to Current Bar (Right)";
        ExtendToCurrent.SetYesNo(false);

        // ---- Hidden subgraphs to store matched index per bar (for re-extend)
        sc.Subgraph[0].Name = "MatchIndexHigh";
        sc.Subgraph[0].DrawStyle = DRAWSTYLE_IGNORE;
        sc.Subgraph[1].Name = "MatchIndexLow";
        sc.Subgraph[1].DrawStyle = DRAWSTYLE_IGNORE;

        return;
    }

    // --------------------------
    // Runtime vars
    // --------------------------
    const int    lookback   = sc.Input[0].GetInt();
    const int    tolTicks   = sc.Input[1].GetInt();
    const double tolPrice   = sc.TickSize * tolTicks;
    const SubgraphLineStyles style = MapStyleIndexToSCStyle(sc.Input[5].GetIndex());
    const int    width      = sc.Input[4].GetInt();
    const COLORREF colorH   = sc.Input[2].GetColor();
    const COLORREF colorL   = sc.Input[3].GetColor();
    const bool   showLbl    = sc.Input[6].GetYesNo();
    const bool   extendCurr = sc.Input[7].GetYesNo();

    const int idx  = sc.Index;
    const int last = sc.ArraySize - 1;
    if (idx < 0 || last < 0)
        return;

    SCFloatArrayRef MatchIdxHigh = sc.Subgraph[0].Data;
    SCFloatArrayRef MatchIdxLow  = sc.Subgraph[1].Data;

    // Initialize default (no match) for this bar
    MatchIdxHigh[idx] = -1.0f;
    MatchIdxLow[idx]  = -1.0f;

    const double curHigh = sc.High[idx];
    const double curLow  = sc.Low[idx];

    // --------------------------
    // Find and draw equal HIGH
    // --------------------------
    {
        int matchJ = -1;
        for (int back = 1; back <= lookback && (idx - back) >= 0; ++back)
        {
            const int j = idx - back;

            if (PriceEqual(sc.High[j], curHigh, tolPrice))
            {
                matchJ = j;

                const double y = sc.High[matchJ];

                s_UseTool ln; ln.Clear();
                ln.ChartNumber = sc.ChartNumber;
                ln.DrawingType = DRAWING_LINE;
                ln.LineWidth   = width;
                ln.LineStyle   = style;
                ln.Color       = colorH;
                ln.RoundToTickSize = 1;

                if (extendCurr)
                {
                    ln.BeginIndex = matchJ;
                    ln.BeginValue = y;
                    ln.EndIndex   = last;
                    ln.EndValue   = y;
                }
                else
                {
                    ln.BeginIndex = idx;
                    ln.BeginValue = y;            // use matched price for both ends
                    ln.EndIndex   = matchJ;
                    ln.EndValue   = y;
                }

                ln.AddMethod  = UTAM_ADD_OR_ADJUST;
                ln.LineNumber = LN_BASE_HIGH + idx; // stable per bar
                sc.UseTool(ln);

                if (showLbl)
                {
                    s_UseTool t; t.Clear();
                    t.ChartNumber = sc.ChartNumber;
                    t.DrawingType = DRAWING_TEXT;
                    t.Color       = colorH;
                    t.FontSize    = 8;
                    t.Text        = "Equal High";
                    t.AddMethod   = UTAM_ADD_OR_ADJUST;
                    t.LineNumber  = LN_BASE_HIGH_LABEL + idx;
                    const int midX = extendCurr ? (matchJ + last) / 2 : (idx + matchJ) / 2;
                    t.BeginIndex  = midX;
                    t.BeginValue  = y;
                    sc.UseTool(t);
                }

                MatchIdxHigh[idx] = static_cast<float>(matchJ);
                break; // first equal match only
            }

            // Early break if a strictly higher high appears
            if (sc.High[j] > curHigh + tolPrice)
                break;
        }
    }

    // --------------------------
    // Find and draw equal LOW
    // --------------------------
    {
        int matchJ = -1;
        for (int back = 1; back <= lookback && (idx - back) >= 0; ++back)
        {
            const int j = idx - back;

            if (PriceEqual(sc.Low[j], curLow, tolPrice))
            {
                matchJ = j;

                const double y = sc.Low[matchJ];

                s_UseTool ln; ln.Clear();
                ln.ChartNumber = sc.ChartNumber;
                ln.DrawingType = DRAWING_LINE;
                ln.LineWidth   = width;
                ln.LineStyle   = style;
                ln.Color       = colorL;
                ln.RoundToTickSize = 1;

                if (extendCurr)
                {
                    ln.BeginIndex = matchJ;
                    ln.BeginValue = y;
                    ln.EndIndex   = last;
                    ln.EndValue   = y;
                }
                else
                {
                    ln.BeginIndex = idx;
                    ln.BeginValue = y;            // use matched price for both ends
                    ln.EndIndex   = matchJ;
                    ln.EndValue   = y;
                }

                ln.AddMethod  = UTAM_ADD_OR_ADJUST;
                ln.LineNumber = LN_BASE_LOW + idx; // stable per bar
                sc.UseTool(ln);

                if (showLbl)
                {
                    s_UseTool t; t.Clear();
                    t.ChartNumber = sc.ChartNumber;
                    t.DrawingType = DRAWING_TEXT;
                    t.Color       = colorL;
                    t.FontSize    = 8;
                    t.Text        = "Equal Low";
                    t.AddMethod   = UTAM_ADD_OR_ADJUST;
                    t.LineNumber  = LN_BASE_LOW_LABEL + idx;
                    const int midX = extendCurr ? (matchJ + last) / 2 : (idx + matchJ) / 2;
                    t.BeginIndex  = midX;
                    t.BeginValue  = y;
                    sc.UseTool(t);
                }

                MatchIdxLow[idx] = static_cast<float>(matchJ);
                break; // first equal match only
            }

            // Early break if a strictly lower low appears
            if (sc.Low[j] < curLow - tolPrice)
                break;
        }
    }

    // --------------------------
    // Dynamic re-extend to current bar (only when enabled)
    // --------------------------
    if (extendCurr && idx == last)
    {
        // For performance, only adjust the last ~lookback bars
        const int start = last > lookback ? (last - lookback) : 0;

        // Re-extend Equal High lines
        for (int k = start; k <= last; ++k)
        {
            const int jh = static_cast<int>(MatchIdxHigh[k]);
            if (jh >= 0)
            {
                const double y = sc.High[jh];

                s_UseTool ln; ln.Clear();
                ln.ChartNumber = sc.ChartNumber;
                ln.DrawingType = DRAWING_LINE;
                ln.BeginIndex  = jh;
                ln.BeginValue  = y;
                ln.EndIndex    = last;
                ln.EndValue    = y;
                ln.LineWidth   = width;
                ln.LineStyle   = style;
                ln.Color       = colorH;
                ln.RoundToTickSize = 1;
                ln.AddMethod   = UTAM_ADD_OR_ADJUST;
                ln.LineNumber  = LN_BASE_HIGH + k;
                sc.UseTool(ln);

                if (showLbl)
                {
                    s_UseTool t; t.Clear();
                    t.ChartNumber = sc.ChartNumber;
                    t.DrawingType = DRAWING_TEXT;
                    t.Color       = colorH;
                    t.FontSize    = 8;
                    t.Text        = "Equal High";
                    t.AddMethod   = UTAM_ADD_OR_ADJUST;
                    t.LineNumber  = LN_BASE_HIGH_LABEL + k;
                    t.BeginIndex  = (jh + last) / 2;
                    t.BeginValue  = y;
                    sc.UseTool(t);
                }
            }
        }

        // Re-extend Equal Low lines
        for (int k = start; k <= last; ++k)
        {
            const int jl = static_cast<int>(MatchIdxLow[k]);
            if (jl >= 0)
            {
                const double y = sc.Low[jl];

                s_UseTool ln; ln.Clear();
                ln.ChartNumber = sc.ChartNumber;
                ln.DrawingType = DRAWING_LINE;
                ln.BeginIndex  = jl;
                ln.BeginValue  = y;
                ln.EndIndex    = last;
                ln.EndValue    = y;
                ln.LineWidth   = width;
                ln.LineStyle   = style;
                ln.Color       = colorL;
                ln.RoundToTickSize = 1;
                ln.AddMethod   = UTAM_ADD_OR_ADJUST;
                ln.LineNumber  = LN_BASE_LOW + k;
                sc.UseTool(ln);

                if (showLbl)
                {
                    s_UseTool t; t.Clear();
                    t.ChartNumber = sc.ChartNumber;
                    t.DrawingType = DRAWING_TEXT;
                    t.Color       = colorL;
                    t.FontSize    = 8;
                    t.Text        = "Equal Low";
                    t.AddMethod   = UTAM_ADD_OR_ADJUST;
                    t.LineNumber  = LN_BASE_LOW_LABEL + k;
                    t.BeginIndex  = (jl + last) / 2;
                    t.BeginValue  = y;
                    sc.UseTool(t);
                }
            }
        }
    }
}
