// yamir-rvol.cpp  (drop-in replacement)
#include "sierrachart.h"

SCDLLName("Yamir – RVOL Tools (TOD Histogram)")

/*
  RVOL Histogram (Time-of-Day, N-Day Lookback)
  - Plots a volume-like histogram with colors driven by RVOL buckets.
  - RVOL = Today's volume (cumulative or per-bar) / Avg over same bar-slot across prior N sessions.
  - Uses session-aware day boundaries via sc.IsNewTradingDay().
*/

SCSFExport scsf_RVOL_Histogram_TOD(SCStudyInterfaceRef sc)
{
  // -------------------- Defaults --------------------
  if (sc.SetDefaults)
  {
	// RVOL Histogram (TOD, N-Day Lookback)
    sc.GraphName = "RVOL Hist";
    sc.StudyDescription = "Colors a volume-style histogram by RVOL using time-of-day averaging across the last N sessions.";
    sc.AutoLoop = 0;                 // manual loop for full control
    sc.GraphRegion = 1;              // region 2 (like standard Volume)
    sc.ScaleRangeType = SCALE_ZEROBASED; // zero-anchored scale (valid ACSIL constant)
    sc.DrawZeros = 0;

    // Subgraphs
	// RVOL Ratio
    sc.Subgraph[0].Name = "RVOL R";
    sc.Subgraph[0].DrawStyle = DRAWSTYLE_LINE;        // toggleable
    sc.Subgraph[0].PrimaryColor = RGB(128,186,255);
    sc.Subgraph[0].LineWidth = 2;
    sc.Subgraph[0].DrawZeros = 0;

    // RVOL-Colored Volume
    sc.Subgraph[1].Name = "RVOL-Vol";
    sc.Subgraph[1].DrawStyle = DRAWSTYLE_BAR;         // histogram bars
    sc.Subgraph[1].PrimaryColor = RGB(180,188,198);   // default muted
    sc.Subgraph[1].LineWidth = 3;
    sc.Subgraph[1].DrawZeros = 0;

    // Inputs
	// Lookback Sessions (N)
    sc.Input[0].Name = "Lookback";
    sc.Input[0].SetInt(14); sc.Input[0].SetIntLimits(1, 60);

    sc.Input[1].Name = "Use Cumulative RVOL (true) or Per-Bar (false)";
    sc.Input[1].SetYesNo(true);

    sc.Input[2].Name = "Minimum Baseline Volume (avoid tiny dividers)";
    sc.Input[2].SetFloat(1000.0f);

    sc.Input[3].Name = "Threshold 1 (Normal)";
    sc.Input[3].SetFloat(1.0f);

    sc.Input[4].Name = "Threshold 2 (Elevated)";
    sc.Input[4].SetFloat(1.5f);

    sc.Input[5].Name = "Threshold 3 (High / Momentum)";
    sc.Input[5].SetFloat(3.0f);

    sc.Input[6].Name = "Show RVOL Ratio Line";
    sc.Input[6].SetYesNo(true);

    sc.Input[7].Name = "Smooth RVOL Line (SMA length; 1 = off)";
    sc.Input[7].SetInt(1); sc.Input[7].SetIntLimits(1, 100);

    return;
  }

  // -------------------- Read settings --------------------
  const int   N          = sc.Input[0].GetInt();
  const bool  UseCum     = sc.Input[1].GetYesNo();
  const float MinBaseVol = sc.Input[2].GetFloat();
  const float T1         = sc.Input[3].GetFloat();
  const float T2         = sc.Input[4].GetFloat();
  const float T3         = sc.Input[5].GetFloat();
  const bool  ShowLine   = sc.Input[6].GetYesNo();
  const int   SMALen     = sc.Input[7].GetInt();

  // Subgraph aliases
  SCFloatArrayRef RVOL = sc.Subgraph[0].Data;           // ratio output (optionally shown)
  SCFloatArrayRef Hist = sc.Subgraph[1].Data;           // histogram height (actual volume)

  // Work arrays (auto-sized to sc.ArraySize): use Subgraph "extra arrays"
  SCFloatArrayRef CumVolToday = sc.Subgraph[1].Arrays[0];  // per-bar cumulative volume for all days
  SCFloatArrayRef RvolBuffer  = sc.Subgraph[0].Arrays[0];  // for optional smoothing

  // Hide/show RVOL line
  sc.Subgraph[0].DrawStyle = ShowLine ? DRAWSTYLE_LINE : DRAWSTYLE_IGNORE;

  const int size = sc.ArraySize;
  if (size < 1)
    return;

  // -------------------- Build trading-day boundaries --------------------
  // dayStarts: indices where a new trading day begins (uses session-aware function)
  static std::vector<int> dayStarts;
  dayStarts.clear();
  dayStarts.reserve(64);
  dayStarts.push_back(0);
  for (int i = 1; i < size; ++i)
    if (sc.IsNewTradingDay(i))
      dayStarts.push_back(i);

  // Convenience lambda to get the day index for a bar index
  auto dayIndexFor = [&](int idx)->int {
    // linear scan is fine; dayStarts is relatively small
    int d = 0;
    for (int k = 0; k < (int)dayStarts.size(); ++k)
    {
      if (dayStarts[k] <= idx) d = k; else break;
    }
    return d;
  };

  // -------------------- Compute cumulative volume (per bar) --------------------
  int startForCum = 0;
  if (sc.UpdateStartIndex > 0)
  {
    // Recompute from start of the day containing UpdateStartIndex (ensures correct reset)
    startForCum = dayIndexFor(sc.UpdateStartIndex);
    startForCum = dayStarts[startForCum];
  }

  float run = 0.0f;
  int curDayIdx = dayIndexFor(startForCum);
  for (int i = startForCum; i < size; ++i)
  {
    // reset run at new trading day
    if (sc.IsNewTradingDay(i) && i != startForCum)
    {
      run = 0.0f;
      curDayIdx++;
    }
    run += (float)sc.Volume[i];
    CumVolToday[i] = run;
  }

  // -------------------- Precompute dayEnds for indexing convenience --------------------
  std::vector<int> dayEnds;
  dayEnds.reserve(dayStarts.size());
  for (size_t k = 0; k < dayStarts.size(); ++k)
  {
    int end = (k + 1 < dayStarts.size()) ? dayStarts[k + 1] : size;
    dayEnds.push_back(end);
  }

  // -------------------- RVOL & histogram coloring --------------------
  const int startIndex = (sc.UpdateStartIndex < 0 ? 0 : sc.UpdateStartIndex);

  int curDay = dayIndexFor(startIndex);
  int nextDayStart = (curDay + 1 < (int)dayStarts.size()) ? dayStarts[curDay + 1] : size;

  for (int i = startIndex; i < size; ++i)
  {
    if (i >= nextDayStart && curDay + 1 < (int)dayStarts.size())
    {
      ++curDay;
      nextDayStart = (curDay + 1 < (int)dayStarts.size()) ? dayStarts[curDay + 1] : size;
    }

    const int dayStart = dayStarts[curDay];
    const int dayEnd   = dayEnds[curDay];
    const int offset   = i - dayStart;

    const float vBar = (float)sc.Volume[i];
    const float vCum = CumVolToday[i];

    // Build average baseline at same slot across previous N sessions
    double sumRef = 0.0;
    int    used   = 0;

    for (int d = 1; d <= N; ++d)
    {
      const int prevDay = curDay - d;
      if (prevDay < 0) break;

      const int prevStart = dayStarts[prevDay];
      const int prevEnd   = dayEnds[prevDay];
      const int prevLen   = prevEnd - prevStart;
      if (prevLen <= 0) continue;

      const int prevIdx = prevStart + (offset < prevLen ? offset : (prevLen - 1));

      if (UseCum) sumRef += (double)CumVolToday[prevIdx];
      else        sumRef += (double)sc.Volume[prevIdx];
      ++used;
    }

    const double avgRef   = (used > 0) ? (sumRef / (double)used) : 0.0;
    const double baseline = (avgRef < MinBaseVol ? (double)MinBaseVol : avgRef);
    const double rvol     = UseCum ? (vCum / baseline) : (vBar / baseline);

    RVOL[i] = (float)rvol;
    Hist[i] = vBar;

    // Color by thresholds
    COLORREF c = RGB(180,188,198);               // muted gray
    if (rvol >= T3)        c = RGB(20,193,132);  // strong (green)
    else if (rvol >= T2)   c = RGB(255,176,0);   // elevated (amber)
    else if (rvol >= T1)   c = RGB(110,160,255); // slightly above avg (blue-ish)

    sc.Subgraph[1].DataColor[i] = c;
  }

  // -------------------- Optional smoothing of the RVOL line --------------------
  if (ShowLine && SMALen > 1)
  {
    sc.SimpleMovAvg(RVOL, RvolBuffer, SMALen);
    const int s = (sc.UpdateStartIndex < 0 ? 0 : sc.UpdateStartIndex);
    for (int i = s; i < size; ++i)
      RVOL[i] = RvolBuffer[i];
  }
}
