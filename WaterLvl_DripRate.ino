/*
  Updated: 2-21-2026 6:33 PM
  IV Bag Occlusion + Leak Detection & Classification (Arduino UNO + LCD1602 Parallel)
  -------------------------------------------------------------------------------

  LCD1602 (PARALLEL) WIRING (4-bit mode) - Example mapping used in this sketch:
    LCD Pin  Name   -> Arduino UNO
    1        VSS    -> GND
    2        VDD    -> 5V
    3        VO     -> Contrast pot wiper (pot ends to 5V and GND)
    4        RS     -> D12
    5        RW     -> GND
    6        E      -> D11
    11       D4     -> D5
    12       D5     -> D4
    13       D6     -> D3
    14       D7     -> D2
    15       A      -> 5V through ~220Ω (backlight +)  (some modules have resistor onboard)
    16       K      -> GND (backlight -)

  SENSORS / VARIABLES MEASURED:
    - Photoresistor on tubing (PIN_PHOTO = A0):
        photoRaw (ADC 0..1023) and a tracked photoBaseline
        A "drop event" is counted when |photoRaw - photoBaseline| exceeds PHOTO_DROP_DELTA.
        Drops are debounced with PHOTO_MIN_MS_BETWEEN_DROPS.
    - Water level sensor in IV bag (PIN_LEVEL = A1):
        levelRaw (ADC 0..1023) is low-pass filtered into levelFiltered.

  DERIVED METRICS (computed every WINDOW_MS):
    - dropsInWindow         : number of detected drops during the window
    - dropRateDPM           : drops per minute
    - levelDelta            : levelFiltered(now) - levelFiltered(windowStart)
    - levelRateUPM          : "level units per minute" (ADC units/min)

  BASELINE ("FUNCTIONING IV VALUES"):
    - During startup CALIBRATION_MS, the sketch averages:
        baselineDropRateDPM and baselineLevelRateUPM
    - Baseline is accepted only if baselineDropRateDPM > NO_DROP_RATE_DPM (i.e., flow exists).

  CLASSIFICATION LOGIC (candidate state computed from the latest windowed metrics):
    Let:
      noDrops       := dropRateDPM <= NO_DROP_RATE_DPM
      noLevelChange := |levelDelta| <= LEVEL_CHANGE_EPS  OR  |levelRateUPM| <= LEVEL_RATE_EPS
      levelChanging := NOT noLevelChange

    - OCCLUSION:
        noDrops AND noLevelChange
    - LEAK (high confidence):
        noDrops AND levelChanging
    - NORMAL FLOW:
        dropRateDPM close to baselineDropRateDPM  AND  levelRateUPM close to baselineLevelRateUPM
        where "close" is within DROP_RATE_CLOSE_PCT / LEVEL_RATE_CLOSE_PCT.
    - REDUCED FLOW / SUSPECT LEAK:
        Anything off-nominal that isn't the above, especially:
        levelChanging AND (dropRateDPM <= DROP_RATE_LOW_FRAC * baselineDropRateDPM)

  STABILITY REQUIREMENT (sTime):
    - The LCD only shows a classification after the candidate state has stayed the same
      for sTime seconds.
*/

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <math.h>

// ---------- LCD1602 (Parallel) pins ----------
static const uint8_t LCD_RS = 12;
static const uint8_t LCD_E  = 11;
static const uint8_t LCD_D4 = 5;
static const uint8_t LCD_D5 = 4;
static const uint8_t LCD_D6 = 3;
static const uint8_t LCD_D7 = 2;

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ---------- Sensor pins ----------
static const uint8_t PIN_PHOTO = A0;   // photoresistor
static const uint8_t PIN_LEVEL = A1;   // water level sensor

// ---------- Timing ----------
static const uint32_t SAMPLE_MS      = 50;
static const uint32_t WINDOW_MS      = 5000;   // compute rates every 5s
static const uint32_t DISPLAY_MS     = 400;
static const uint32_t CALIBRATION_MS = 4 * WINDOW_MS;  // baseline capture duration

// ---------- "State must persist" ---------- 
static const uint32_t sTime = 1; // seconds (faster response to rate changes)
static const uint32_t STABLE_TIME_MS = sTime * 1000UL;

// ---------- Photoresistor drop detection tuning ----------
static const int      PHOTO_DROP_DELTA = 16;   // lower threshold => detect smaller drip pulses
static const uint32_t PHOTO_MIN_MS_BETWEEN_DROPS = 120; // allow denser drip events
static const float    PHOTO_BASELINE_ALPHA = 0.015f; // slower baseline adaptation keeps transient sensitivity
static const uint8_t  PHOTO_MEDIAN_SAMPLES = 5; // odd number for median filter
static const float    PHOTO_FILTER_ALPHA = 0.30f; // light EMA after median for jitter reduction

// ---------- Water level filtering/tuning ----------
static const float LEVEL_FILTER_ALPHA = 0.07f; // lower alpha for stronger smoothing
static const uint8_t LEVEL_MEDIAN_SAMPLES = 5; // odd number for median filter
static const float LEVEL_CHANGE_EPS   = 20.0f;   // delta threshold over WINDOW_MS; change between 20 and 40 depending on what works and what doesn't
static const float LEVEL_RATE_EPS     = LEVEL_CHANGE_EPS * (60000 / WINDOW_MS);   // near-zero rate threshold (units/min)
// Leak detection can use a tighter threshold than occlusion deadband.
static const float LEAK_LEVEL_DROP_EPS = 8.0f; // minimum decrease over WINDOW_MS to treat as leak-like
static const float LEAK_LEVEL_RATE_EPS = LEAK_LEVEL_DROP_EPS * (60000 / WINDOW_MS);

// ---------- Classification thresholds ----------
static const float DROP_RATE_CLOSE_PCT  = 0.20f; // within ±20% baseline => close (stricter drip-rate matching)
static const float LEVEL_RATE_CLOSE_PCT = 0.30f; // within ±30% baseline => close
static const float DROP_RATE_LOW_FRAC   = 0.80f; // <= 80% of baseline => reduced/suspect
static const float NO_DROP_RATE_DPM     = 2.0f;  // <2 drops/min => no drops

// ---------- Status ----------
enum class Status {
  CALIBRATING,
  NORMAL_FLOW,
  OCCLUSION,
  LEAK,
  REDUCED_SUSPECT,
  UNKNOWN
};

struct Rates {
  float dropRateDPM;    // drops/min
  float levelRateUPM;   // ADC units/min
  float levelDelta;     // ADC units over window
  uint32_t dropsInWindow;
};

// ---------- State ----------
static uint32_t lastSampleMs  = 0;
static uint32_t lastDisplayMs = 0;

// init flags
static bool photoInit = false;
static bool photoFilterInit = false;
static bool levelInit = false;

static float photoBaseline = 0.0f;
static float photoFiltered = 0.0f;
static float levelFiltered = 0.0f;

static uint32_t lastDropMs = 0;
static bool dropArmed = true;

static uint32_t windowStartMs = 0;
static float levelAtWindowStart = 0.0f;
static uint32_t dropsInCurrentWindow = 0;

static Rates currentRates = {0, 0, 0, 0};
static bool haveRates = false; // valid after first full WINDOW_MS compute

// Baselines
static bool baselinesReady = false;
static float baselineDropRateDPM = 0.0f;
static float baselineLevelRateUPM = 0.0f;

// Calibration accumulators
static uint32_t calibStartMs = 0;
static uint32_t calibWindowCount = 0;
static float calibDropRateSum = 0.0f;
static float calibLevelRateSum = 0.0f;

// Candidate vs Committed (stability timer)
static Status committedStatus = Status::CALIBRATING;
static Status candidateStatus = Status::CALIBRATING;
static uint32_t candidateSinceMs = 0;

// ---------- Helpers ----------
static float pctDiff(float a, float b) {
  const float denom = (fabsf(b) < 1e-3f) ? 1.0f : fabsf(b);
  return fabsf(a - b) / denom;
}

static const char* statusToText(Status s) {
  switch (s) {
    case Status::CALIBRATING:     return "CALIBRATING";
    case Status::NORMAL_FLOW:     return "NORMAL FLOW";
    case Status::OCCLUSION:       return "OCCLUSION";
    case Status::LEAK:            return "LEAK";
    case Status::REDUCED_SUSPECT: return "REDUCED/SUSPECT";
    default:                      return "UNKNOWN";
  }
}

static void lcdPrintPadded(const char* l1, const char* l2) {
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print(l1);

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(l2);
}

// UNO NOTE: printf/snprintf generally don't support %f; format LCD with integers.
static void buildRatesLine(char out[17]) {
  long d = lroundf(currentRates.dropRateDPM);
  long l = lroundf(currentRates.levelRateUPM);
  d = constrain(d, -999L, 9999L);
  l = constrain(l, -9999L, 9999L);
  snprintf(out, 17, "D%3ld L%4ld", d, l);
}

static int readDenoisedPhotoAnalog() {
  int samples[PHOTO_MEDIAN_SAMPLES];
  for (uint8_t i = 0; i < PHOTO_MEDIAN_SAMPLES; ++i) {
    samples[i] = analogRead(PIN_PHOTO);
    delayMicroseconds(80);
  }

  for (uint8_t i = 1; i < PHOTO_MEDIAN_SAMPLES; ++i) {
    int key = samples[i];
    int j = i;
    while (j > 0 && samples[j - 1] > key) {
      samples[j] = samples[j - 1];
      --j;
    }
    samples[j] = key;
  }

  const int medianRaw = samples[PHOTO_MEDIAN_SAMPLES / 2];
  if (!photoFilterInit) {
    photoFiltered = (float)medianRaw;
    photoFilterInit = true;
  } else {
    photoFiltered = (1.0f - PHOTO_FILTER_ALPHA) * photoFiltered
                  + PHOTO_FILTER_ALPHA * (float)medianRaw;
  }

  return (int)lroundf(photoFiltered);
}

static int readMedianAnalog(uint8_t pin) {
  int samples[LEVEL_MEDIAN_SAMPLES];
  for (uint8_t i = 0; i < LEVEL_MEDIAN_SAMPLES; ++i) {
    samples[i] = analogRead(pin);
    delayMicroseconds(120);
  }

  for (uint8_t i = 1; i < LEVEL_MEDIAN_SAMPLES; ++i) {
    int key = samples[i];
    int j = i;
    while (j > 0 && samples[j - 1] > key) {
      samples[j] = samples[j - 1];
      --j;
    }
    samples[j] = key;
  }

  return samples[LEVEL_MEDIAN_SAMPLES / 2];
}

// ---------- Drop detection ----------
static void updateDropDetection(int photoRaw, uint32_t nowMs) {
  if (!photoInit) {
    photoBaseline = (float)photoRaw;
    photoInit = true;
  }

  const float diff = fabsf((float)photoRaw - photoBaseline);

  if (dropArmed) {
    if (diff >= (float)PHOTO_DROP_DELTA) {
        if (nowMs - lastDropMs >= PHOTO_MIN_MS_BETWEEN_DROPS) {
            dropsInCurrentWindow++;
            lastDropMs = nowMs;
        }
        dropArmed = false;
        // ✅ baseline NOT updated — photoRaw is anomalous
    } else {
        // ✅ only update baseline when sensor is truly idle
        photoBaseline = (1.0f - PHOTO_BASELINE_ALPHA) * photoBaseline
                      + PHOTO_BASELINE_ALPHA * (float)photoRaw;
    }
  } else {
    if (diff <= (float)(PHOTO_DROP_DELTA / 2)) {
        dropArmed = true;
    }
  }

}

// ---------- Rate computation ----------
static void rollWindowAndCompute(uint32_t nowMs) {
  if (windowStartMs == 0) {
    windowStartMs = nowMs;
    levelAtWindowStart = levelFiltered;
    dropsInCurrentWindow = 0;
    return;
  }

  if (nowMs - windowStartMs >= WINDOW_MS) {
    const float minutes = (float)(nowMs - windowStartMs) / 60000.0f;
    const float levelDelta = levelFiltered - levelAtWindowStart;

    currentRates.dropsInWindow = dropsInCurrentWindow;
    currentRates.dropRateDPM   = (minutes > 0.0f) ? (dropsInCurrentWindow / minutes) : 0.0f;
    currentRates.levelDelta    = levelDelta;
    currentRates.levelRateUPM  = (minutes > 0.0f) ? (levelDelta / minutes) : 0.0f;

    haveRates = true;

    windowStartMs = nowMs;
    levelAtWindowStart = levelFiltered;
    dropsInCurrentWindow = 0;
  }
}

// ---------- Calibration ----------
static void updateCalibration(uint32_t nowMs) {
  if (calibStartMs == 0) calibStartMs = nowMs;

  static uint32_t lastAccumulatedWindowStart = 0;

  // Only average after real computed rates exist
  if (haveRates && windowStartMs != 0 && windowStartMs != lastAccumulatedWindowStart) {
    if ((nowMs - calibStartMs) > WINDOW_MS) {
      calibDropRateSum  += currentRates.dropRateDPM;
      calibLevelRateSum += currentRates.levelRateUPM;
      calibWindowCount++;
    }
    lastAccumulatedWindowStart = windowStartMs;
  }

  if ((nowMs - calibStartMs) >= CALIBRATION_MS && calibWindowCount >= 2) {
    baselineDropRateDPM  = calibDropRateSum  / (float)calibWindowCount;
    baselineLevelRateUPM = calibLevelRateSum / (float)calibWindowCount;

    // Avoid calibration loops: if startup had little/no flow, finish once
    // using a conservative fallback instead of restarting calibration.
    if (baselineDropRateDPM <= NO_DROP_RATE_DPM) {
      baselineDropRateDPM = NO_DROP_RATE_DPM + 0.1f;
      baselineLevelRateUPM = 0.0f;
    }

    baselinesReady = true;
  }
}

// ---------- Classification (candidate) ----------
static Status classifyCandidate(const Rates& r) {
  if (!baselinesReady || !haveRates) return Status::CALIBRATING;

  const bool noDrops = (r.dropRateDPM <= NO_DROP_RATE_DPM);
  const bool levelDroppingForLeak =
      (r.levelDelta <= -LEAK_LEVEL_DROP_EPS) ||
      (r.levelRateUPM <= -LEAK_LEVEL_RATE_EPS);

  const bool noLevelChange =
      (fabsf(r.levelDelta) <= LEVEL_CHANGE_EPS) &&
      (fabsf(r.levelRateUPM) <= LEVEL_RATE_EPS);

  const bool levelChanging = !noLevelChange;

  // Prioritize no-drops + level decrease as leak, even if decrease is smaller
  // than the wider occlusion deadband.
  if (noDrops && levelDroppingForLeak) return Status::LEAK;
  if (noLevelChange && noDrops) return Status::OCCLUSION;
  if (levelChanging && noDrops) return Status::LEAK;

  const bool dropClose  = (pctDiff(r.dropRateDPM,  baselineDropRateDPM)  <= DROP_RATE_CLOSE_PCT);
  const bool levelClose = (pctDiff(r.levelRateUPM, baselineLevelRateUPM) <= LEVEL_RATE_CLOSE_PCT);

  if (dropClose && levelClose) return Status::NORMAL_FLOW;

  // Reduced flow / suspect leak
  if (baselineDropRateDPM > 0.1f) {
    const float frac = r.dropRateDPM / baselineDropRateDPM;
    if (levelChanging && frac <= DROP_RATE_LOW_FRAC) {
      return Status::REDUCED_SUSPECT;
    }
  }

  return Status::REDUCED_SUSPECT;
}

// ---------- Stability gating ----------
static void updateCommittedStatus(Status newCandidate, uint32_t nowMs) {
  if (newCandidate != candidateStatus) {
    candidateStatus = newCandidate;
    candidateSinceMs = nowMs;
    return;
  }

  if (committedStatus != candidateStatus) {
    if (nowMs - candidateSinceMs >= STABLE_TIME_MS) {
      committedStatus = candidateStatus;
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_PHOTO, INPUT);
  pinMode(PIN_LEVEL, INPUT);

  lcd.begin(16, 2);
  lcdPrintPadded("IV Monitor", "Starting...");

  committedStatus = Status::CALIBRATING;
  candidateStatus = Status::CALIBRATING;
  candidateSinceMs = millis();
}

void loop() 
{
  const uint32_t nowMs = millis();

  if (nowMs - lastSampleMs >= SAMPLE_MS) {
    lastSampleMs = nowMs;

    const int photoRaw = readDenoisedPhotoAnalog();
    const int levelRaw = readMedianAnalog(PIN_LEVEL);

    // Filter level
    if (!levelInit) {
      levelFiltered = (float)levelRaw;
      levelInit = true;
    } else {
      levelFiltered = (1.0f - LEVEL_FILTER_ALPHA) * levelFiltered
                    + LEVEL_FILTER_ALPHA * (float)levelRaw;
    }

    updateDropDetection(photoRaw, nowMs);
    rollWindowAndCompute(nowMs);

    if (!baselinesReady) {
      updateCalibration(nowMs);
      updateCommittedStatus(Status::CALIBRATING, nowMs);
    } else {
      Status cand = classifyCandidate(currentRates);
      updateCommittedStatus(cand, nowMs);
    }

    // Debug (Serial supports floats fine on UNO)
    Serial.print("cand="); Serial.print(statusToText(candidateStatus));
    Serial.print(" comm="); Serial.print(statusToText(committedStatus));
    Serial.print(" dropDPM="); Serial.print(currentRates.dropRateDPM, 1);
    Serial.print(" levelUPM="); Serial.print(currentRates.levelRateUPM, 1);
    Serial.print(" baseDrop="); Serial.print(baselineDropRateDPM, 1);
    Serial.print(" baseLvl="); Serial.println(baselineLevelRateUPM, 1);
  }

  if (nowMs - lastDisplayMs >= DISPLAY_MS) 
  {
    lastDisplayMs = nowMs;

    char line1[17];
    char line2[17];

    snprintf(line1, 17, "%-16s", statusToText(committedStatus));

    if (!baselinesReady) {
      uint32_t elapsed = (calibStartMs == 0) ? 0 : (millis() - calibStartMs);
      uint32_t pct = (CALIBRATION_MS == 0) ? 0 : (elapsed * 100UL / CALIBRATION_MS);
      if (pct > 99) pct = 99;
      snprintf(line2, 17, "Cal%2lu%% s=%lus",
               (unsigned long)pct,
               (unsigned long)sTime);
    } else {
      buildRatesLine(line2);
    }

    lcdPrintPadded(line1, line2);
    }
  }
