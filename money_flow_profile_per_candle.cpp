#include "sierrachart.h"
#include <math.h>

SCDLLName("Money Flow Profile Per Candle (GDI Clean)")

// 0 = Volume, 1 = Money Flow
enum ProfileSourceEnum
{
    PROFILE_SOURCE_VOLUME     = 0,
    PROFILE_SOURCE_MONEY_FLOW = 1
};

enum SentimentFallbackEnum
{
    SENTIMENT_BAR_POLARITY      = 0,
    SENTIMENT_BUY_SELL_PRESSURE = 1
};

template<typename T>
static T ClampVal(T v, T lo, T hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Slight lighten/darken for stripe lines
static COLORREF StripeColor(COLORREF base, bool lighten)
{
    int r = GetRValue(base);
    int g = GetGValue(base);
    int b = GetBValue(base);

    if (lighten)
    {
        r = (r + 255) / 2;
        g = (g + 255) / 2;
        b = (b + 255) / 2;
    }
    else
    {
        r /= 2;
        g /= 2;
        b /= 2;
    }

    return RGB(ClampVal(r, 0, 255), ClampVal(g, 0, 255), ClampVal(b, 0, 255));
}

// Forward declaration
void DrawMoneyFlowProfile(HWND, HDC, SCStudyInterfaceRef);

//------------------------------------------------------------------------------------------
// Main study
//------------------------------------------------------------------------------------------

SCSFExport scsf_MoneyFlowProfilePerCandleLuxAlgoClone(SCStudyInterfaceRef sc)
{
    SCInputRef In_ProfileSource           = sc.Input[0];
    SCInputRef In_BarsToRender            = sc.Input[1];
    SCInputRef In_MaxWidthPixels          = sc.Input[2];
    SCInputRef In_MinWidthPixels          = sc.Input[3];
    SCInputRef In_MinTotalPercent         = sc.Input[4];
    SCInputRef In_SentimentFallbackMethod = sc.Input[5];
    SCInputRef In_ShowStripes             = sc.Input[6];
    SCInputRef In_StripeBaseColor         = sc.Input[7];
    SCInputRef In_BullColor               = sc.Input[8];
    SCInputRef In_BearColor               = sc.Input[9];
    SCInputRef In_PoCColor                = sc.Input[10];

    if (sc.SetDefaults)
    {
        sc.GraphName = "Money Flow Profile Per Candle (GDI Clean)";
        sc.StudyDescription =
            "Per-candle volume/money-flow profile using Volume-At-Price.\n"
            "Each price level: red bar left (sells), green bar right (buys), width ∝ total flow; "
            "amber line is PoC for that candle.";

        sc.GraphRegion               = 0;
        sc.AutoLoop                  = 0;
        sc.MaintainVolumeAtPriceData = 1;
        sc.UpdateAlways              = 1;

        // 0: Profile source – default Money Flow
        In_ProfileSource.Name = "Profile Source";
        In_ProfileSource.SetCustomInputStrings("Volume;Money Flow");
        In_ProfileSource.SetCustomInputIndex(PROFILE_SOURCE_MONEY_FLOW);

        // 1: Last N bars (within visible range)
        In_BarsToRender.Name = "Bars to Render (Last N Visible)";
        In_BarsToRender.SetInt(10);
        In_BarsToRender.SetIntLimits(1, 200);

        // 2–3: Width scaling inside each candle lane
        In_MaxWidthPixels.Name = "Max Row Width (pixels)";
        In_MaxWidthPixels.SetInt(60);
        In_MaxWidthPixels.SetIntLimits(10, 300);

        In_MinWidthPixels.Name = "Min Row Width (pixels)";
        In_MinWidthPixels.SetInt(2);
        In_MinWidthPixels.SetIntLimits(1, 50);

        // 4: Ignore tiny rows if you want (default 0 = show all)
        In_MinTotalPercent.Name = "Ignore Rows < X% of Bar Max Total";
        In_MinTotalPercent.SetInt(0);
        In_MinTotalPercent.SetIntLimits(0, 50);

        // 5: Fallback sentiment when Bid/Ask not present
        In_SentimentFallbackMethod.Name = "Fallback Sentiment (if no Bid/Ask)";
        In_SentimentFallbackMethod.SetCustomInputStrings(
            "Bar Polarity;Bar Buying/Selling Pressure");
        In_SentimentFallbackMethod.SetCustomInputIndex(SENTIMENT_BAR_POLARITY);

        // 6–7: Optional stripe lines between candles (do NOT hide candles)
        In_ShowStripes.Name = "Show Vertical Stripe Lines Between Candles";
        In_ShowStripes.SetYesNo(true);

        In_StripeBaseColor.Name = "Stripe Base Color";
        In_StripeBaseColor.SetColor(RGB(40,40,40));

        // 8–10: Colors
        In_BullColor.Name = "Bullish (Buy) Color";
        In_BullColor.SetColor(RGB(0, 200, 83));   // vivid green

        In_BearColor.Name = "Bearish (Sell) Color";
        In_BearColor.SetColor(RGB(213, 0, 0));    // vivid red

        In_PoCColor.Name  = "PoC Color (Most Traded Price)";
        In_PoCColor.SetColor(RGB(255, 193, 7));   // amber

        return;
    }

    // Runtime: hook GDI painter
    sc.p_GDIFunction = DrawMoneyFlowProfile;
}

//------------------------------------------------------------------------------------------
// GDI drawing helpers
//------------------------------------------------------------------------------------------

// Center X of the profile for bar `index`, placed BETWEEN this bar and the next.
// This uses Sierra's BarIndexToXPixelCoordinate so it tracks bar spacing
// and fill space exactly like the candles do.
static int GetProfileCenterX(SCStudyInterfaceRef sc, int index)
{
    if (sc.BarIndexToXPixelCoordinate == nullptr)
        return 0;

    const int xThis = sc.BarIndexToXPixelCoordinate(index);
    const int xNext = sc.BarIndexToXPixelCoordinate(index + 1); // > last visible clamps to right edge

    if (xNext == xThis)
        return xThis;

    return (xThis + xNext) / 2;
}

//------------------------------------------------------------------------------------------
// GDI drawing
//------------------------------------------------------------------------------------------

void DrawMoneyFlowProfile(HWND /*WindowHandle*/, HDC dc, SCStudyInterfaceRef sc)
{
    if (sc.ArraySize <= 0 || sc.VolumeAtPriceForBars == nullptr)
        return;
    if (sc.BarIndexToXPixelCoordinate == nullptr)
        return;

    const int arraySize = sc.ArraySize;
    const int lastIndex = arraySize - 1;

    // Inputs
    const int  profileSourceIndex = sc.Input[0].GetIndex();
    const bool useMoneyFlow       = (profileSourceIndex == PROFILE_SOURCE_MONEY_FLOW);

    int barsToRender = ClampVal(sc.Input[1].GetInt(), 1, arraySize);

    int maxWidthPx = sc.Input[2].GetInt();
    maxWidthPx     = ClampVal(maxWidthPx, 10, 300);

    int minWidthPx = sc.Input[3].GetInt();
    minWidthPx     = ClampVal(minWidthPx, 1, maxWidthPx);

    const double minTotalRel = sc.Input[4].GetInt() / 100.0; // 0..0.5

    const int sentimentFallback = sc.Input[5].GetIndex();

    const bool     showStripes    = (sc.Input[6].GetYesNo() != 0);
    const COLORREF stripeBase     = sc.Input[7].GetColor();
    const COLORREF bullColor      = sc.Input[8].GetColor();
    const COLORREF bearColor      = sc.Input[9].GetColor();
    const COLORREF pocColor       = sc.Input[10].GetColor();

    const double tickSize = (sc.TickSize > 0.0) ? sc.TickSize : 0.01;

    // Region geometry
    const int regionLeft   = sc.StudyRegionLeftCoordinate;
    const int regionRight  = sc.StudyRegionRightCoordinate;
    const int regionTop    = sc.StudyRegionTopCoordinate;
    const int regionBottom = sc.StudyRegionBottomCoordinate;

    if (regionRight <= regionLeft || regionBottom <= regionTop)
        return;

    // Visible bars
    int firstVisible = sc.IndexOfFirstVisibleBar;
    int lastVisible  = sc.IndexOfLastVisibleBar;

    if (firstVisible < 0)
        firstVisible = 0;
    if (lastVisible < 0 || lastVisible >= arraySize)
        lastVisible = arraySize - 1;
    if (lastVisible < firstVisible)
        return;

    int visibleBars = lastVisible - firstVisible + 1;
    if (visibleBars <= 0)
        return;

    // Clip last N bars to visible range
    if (barsToRender > visibleBars)
        barsToRender = visibleBars;

    int drawLast  = lastVisible;
    int drawFirst = drawLast - (barsToRender - 1);
    if (drawFirst < firstVisible)
        drawFirst = firstVisible;

    // GDI resources
    HBRUSH bullBrush = CreateSolidBrush(bullColor);
    HBRUSH bearBrush = CreateSolidBrush(bearColor);

    HPEN   stripeLightPen = CreatePen(PS_SOLID, 1, StripeColor(stripeBase, true));
    HPEN   stripeDarkPen  = CreatePen(PS_SOLID, 1, StripeColor(stripeBase, false));
    HPEN   pocPen         = CreatePen(PS_SOLID, 2, pocColor);
    HPEN   nullPen        = (HPEN)GetStockObject(NULL_PEN);

    HGDIOBJ oldBrush = SelectObject(dc, bullBrush);
    HGDIOBJ oldPen   = SelectObject(dc, nullPen);

    // MAIN BAR LOOP
    for (int index = drawFirst; index <= drawLast; ++index)
    {
        const float highF = sc.High[index];
        const float lowF  = sc.Low[index];
        if (highF <= lowF)
            continue;

        const double barHigh = (double)highF;
        const double barLow  = (double)lowF;

        int yHigh = sc.RegionValueToYPixelCoordinate(highF, sc.GraphRegion);
        int yLow  = sc.RegionValueToYPixelCoordinate(lowF , sc.GraphRegion);
        if (yLow < yHigh)
        {
            int tmp = yLow;
            yLow = yHigh;
            yHigh = tmp;
        }
        const int barPixelHeight = yLow - yHigh;

        const int vapCount =
            sc.VolumeAtPriceForBars->GetSizeAtBarIndex(index);
        if (vapCount <= 0 || barPixelHeight <= 0)
            continue;

        // Stripe line between this candle and previous (true between-bar position),
        // using the pattern from your "almost perfect" solution.
        if (showStripes && index > firstVisible)
        {
            const bool even = ((index - firstVisible) % 2 == 0);
            HPEN stripePen  = even ? stripeLightPen : stripeDarkPen;

            int xPrev   = sc.BarIndexToXPixelCoordinate(index - 1);
            int xCurr   = sc.BarIndexToXPixelCoordinate(index);
            int stripeX = (xPrev + xCurr) / 2;

            // Clip inside study region just in case
            if (stripeX < regionLeft)  stripeX = regionLeft;
            if (stripeX > regionRight) stripeX = regionRight;

            HPEN savedPen = (HPEN)SelectObject(dc, stripePen);
            MoveToEx(dc, stripeX, regionTop, NULL);
            LineTo(dc, stripeX, regionBottom);
            SelectObject(dc, savedPen);
        }

        // Fallback bar-level sentiment (only used when no bid/ask split)
        bool barBull;
        if (sentimentFallback == SENTIMENT_BAR_POLARITY)
            barBull = (sc.Close[index] > sc.Open[index]);
        else
        {
            const double upLeg   = sc.Close[index] - lowF;
            const double downLeg = highF - sc.Close[index];
            barBull              = (upLeg > downLeg);
        }

        // X center: strictly between this bar and the next, using my earlier helper.
        const int xCenter = GetProfileCenterX(sc, index);

        // First pass: find max TOTAL metric (for PoC + width scaling)
        double maxTotalMetric = 0.0;
        int    pocPriceTicks  = -1;

        const s_VolumeAtPriceV2* pVAP = nullptr;

        for (int j = 0; j < vapCount; ++j)
        {
            if (!sc.VolumeAtPriceForBars->GetVAPElementAtIndex(index, j, &pVAP))
                break;
            if (pVAP == nullptr || pVAP->Volume <= 0)
                continue;

            const double price   = (double)pVAP->PriceInTicks * tickSize;
            double askVol        = (double)pVAP->AskVolume;
            double bidVol        = (double)pVAP->BidVolume;
            double volSum        = askVol + bidVol;

            if (volSum <= 0.0)
            {
                volSum = (double)pVAP->Volume;
                if (volSum <= 0.0)
                    continue;

                // No bid/ask split → use fallback sentiment
                if (barBull)
                    askVol = volSum;
                else
                    bidVol = volSum;
            }

            double buyMetric  = askVol;
            double sellMetric = bidVol;
            if (useMoneyFlow)
            {
                buyMetric  *= price;
                sellMetric *= price;
            }

            const double totalMetric = buyMetric + sellMetric;
            if (totalMetric <= 0.0)
                continue;

            if (totalMetric > maxTotalMetric)
            {
                maxTotalMetric = totalMetric;
                pocPriceTicks  = pVAP->PriceInTicks;
            }
        }

        if (maxTotalMetric <= 0.0 || pocPriceTicks < 0)
            continue;

        const double pocPrice = (double)pocPriceTicks * tickSize;

        // Vertical thickness for each VAP row
        int rowHalfHeight = barPixelHeight / (vapCount * 2);
        if (rowHalfHeight < 1)
            rowHalfHeight = 1;

        // Track the actual horizontal extent of the PoC row
        int  pocLineX1      = xCenter;
        int  pocLineX2      = xCenter;
        bool havePocExtents = false;

        // Second pass: draw red/green bars for each price level
        for (int j = 0; j < vapCount; ++j)
        {
            if (!sc.VolumeAtPriceForBars->GetVAPElementAtIndex(index, j, &pVAP))
                break;
            if (pVAP == nullptr || pVAP->Volume <= 0)
                continue;

            const double price   = (double)pVAP->PriceInTicks * tickSize;
            double askVol        = (double)pVAP->AskVolume;
            double bidVol        = (double)pVAP->BidVolume;
            double volSum        = askVol + bidVol;

            if (volSum <= 0.0)
            {
                volSum = (double)pVAP->Volume;
                if (volSum <= 0.0)
                    continue;

                if (barBull)
                    askVol = volSum;
                else
                    bidVol = volSum;
            }

            double buyMetric  = askVol;
            double sellMetric = bidVol;
            if (useMoneyFlow)
            {
                buyMetric  *= price;
                sellMetric *= price;
            }

            const double totalMetric = buyMetric + sellMetric;
            if (totalMetric <= 0.0)
                continue;

            const double relTotal = totalMetric / maxTotalMetric; // 0..1

            // Keep PoC row even if below threshold
            if (pVAP->PriceInTicks != pocPriceTicks)
            {
                if (minTotalRel > 0.0 && relTotal < minTotalRel)
                    continue;
            }

            int fullWidth = minWidthPx +
                (int)((double)(maxWidthPx - minWidthPx) * relTotal + 0.5);
            if (fullWidth < minWidthPx)
                fullWidth = minWidthPx;

            // Split width between sellers (left) and buyers (right)
            double buyProp  = buyMetric  / totalMetric;
            double sellProp = sellMetric / totalMetric;

            int wBuy  = (int)(fullWidth * buyProp  + 0.5);
            int wSell = (int)(fullWidth * sellProp + 0.5);
            if (wBuy + wSell <= 0)
                continue;

            if (wBuy  <= 0 && buyMetric  > 0.0) wBuy  = 1;
            if (wSell <= 0 && sellMetric > 0.0) wSell = 1;

            int yMid = sc.RegionValueToYPixelCoordinate((float)price, sc.GraphRegion);
            int y1   = yMid - rowHalfHeight;
            int y2   = yMid + rowHalfHeight;

            // Left = sellers (red)
            if (wSell > 0)
            {
                SelectObject(dc, bearBrush);
                int x1 = xCenter - wSell;
                int x2 = xCenter;
                Rectangle(dc, x1, y1, x2, y2);

                // If this is the PoC row, capture its left extent
                if (pVAP->PriceInTicks == pocPriceTicks)
                {
                    pocLineX1      = xCenter - wSell;
                    havePocExtents = true;
                }
            }

            // Right = buyers (green)
            if (wBuy > 0)
            {
                SelectObject(dc, bullBrush);
                int x1 = xCenter;
                int x2 = xCenter + wBuy;
                Rectangle(dc, x1, y1, x2, y2);

                // If this is the PoC row, capture its right extent
                if (pVAP->PriceInTicks == pocPriceTicks)
                {
                    pocLineX2      = xCenter + wBuy;
                    havePocExtents = true;
                }
            }
        }

        // PoC line across the ACTUAL PoC row width (fixes "amber line too long")
        if (havePocExtents)
        {
            int yPoc = sc.RegionValueToYPixelCoordinate((float)pocPrice, sc.GraphRegion);
            HPEN savedPen = (HPEN)SelectObject(dc, pocPen);
            MoveToEx(dc, pocLineX1, yPoc, NULL);
            LineTo(dc,  pocLineX2, yPoc);
            SelectObject(dc, savedPen);
        }
    } // end bar loop

    // Restore GDI state and free resources
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);

    DeleteObject(bullBrush);
    DeleteObject(bearBrush);
    DeleteObject(stripeLightPen);
    DeleteObject(stripeDarkPen);
    DeleteObject(pocPen);
}
