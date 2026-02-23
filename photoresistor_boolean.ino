/*
  Note:
  This program is specifically designed for our prototypical IV bag/system. For a
  theoretically more accurate model, refer to the branch iv-3-type-output.
*/

/*
  Updated: 2-21-2026 Time: 10:28pm
  IV Bag Occlusion + Leak Detection & Classification (Arduino UNO + LCD1602 Parallel)
  -------------------------------------------------------------------------------

  LCD1602 (PARALLEL) WIRING (4-bit mode):
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
    15       A      -> 5V through ~220Ohm (backlight +)
    16       K      -> GND (backlight -)

  SENSORS / VARIABLES MEASURED:
    - Photoresistor on tubing (PIN_PHOTO = A0):
        photoRaw (ADC 0..1023) and a tracked photoBaseline.
        Water presence uses directional hysteresis against photoBaseline.
        PHOTO_WATER_MAKES_DARKER = true  -> water lowers the ADC reading
        PHOTO_WATER_MAKES_DARKER = false -> water raises the ADC reading
    - Water level sensor in IV bag (PIN_LEVEL = A1):
        levelRaw (ADC 0..1023) is low-pass filtered into levelFiltered.

  DERIVED METRICS (computed every WINDOW_MS):
    - photoPresentSamplesInWindow : samples in window where water was present
    - photoSamplesInWindow        : total photo samples counted in window
    - photoPresentFrac            : ratio of the two above
    - waterPresent                : true if photoPresentFrac >= PHOTO_PRESENT_FRAC_THRESHOLD
    - waterAbsent                 : true if photoPresentFrac <= PHOTO_ABSENT_FRAC_THRESHOLD
      NOTE: if neither flag is set, photoPresentFrac is in the "grey zone" between the two
            thresholds, which is treated as an ambiguous / partial-flow condition.
    - levelDelta                  : levelFiltered(now) - levelFiltered(windowStart)
    - levelRateUPM                : ADC units / minute

  CLASSIFICATION LOGIC (candidate recomputed every window; stability gate runs every tick):
    Let:
      levelDecreasing := levelDelta   <= -LEVEL_DECREASING_DROP_EPS
                      OR levelRateUPM <= -LEVEL_DECREASING_RATE_EPS

    +------------------+------------------+-------------------------------+
    | levelDecreasing  | water state      | Classification                |
    +------------------+------------------+-------------------------------+
    | true             | present          | NORMAL_FLOW                   |
    | true             | grey / absent    | LEAK                          |
    | false            | absent           | OCCLUSION (immediate)         |
    | false            | grey             | OCCLUSION (immediate)         |
    | false            | present          | OCCLUSION (timer-gated 10 s)  |
    +------------------+------------------+-------------------------------+

  STABILITY REQUIREMENT (sTime):
    The LCD only updates to a new classification after the candidate state
    has remained the same for sTime seconds. The candidate is refreshed on
    every new window, but the stability gate is checked on every sample tick
    so that sTime is honoured precisely.

  FIXES APPLIED (this revision):
    A. Stability timer now works as documented: lastCandidate is updated on
       each new window, but updateCommittedStatus() is called on every tick
       so the 1-second gate is evaluated continuously, not once per 5s window.
    B. LCD line 2 label corrected: "Rt" (rate) instead of "dL" (delta).
       The value shown is levelRateUPM (ADC units/min), not levelDelta.
    C. waterAbsent is now used directly in classifyCandidate() for the
       absent vs. grey distinction. Grey zone still maps to OCCLUSION/LEAK,
       but the logic now reflects the actual flag rather than just !waterPresent.
    D. Serial debug output is gated to print only when a new window closes,
       eliminating 99 identical stale lines per window.
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
static const uint8_t PIN_PHOTO = A0;
static const uint8_t PIN_LEVEL = A1;

// ---------- Timing ----------
static const uint32_t SAMPLE_MS      = 50;
static const uint32_t WINDOW_MS      = 5000;
static const uint32_t DISPLAY_MS     = 400;
static const uint32_t CALIBRATION_MS = 4 * WINDOW_MS;

// ---------- Stability gate ----------
static const uint32_t sTime          = 1;   // seconds -- gate is now checked every tick
static const uint32_t STABLE_TIME_MS = sTime * 1000UL;

// ---------- Photoresistor water-presence tuning ----------
static const bool     PHOTO_WATER_MAKES_DARKER  = true;
static const int      PHOTO_WATER_PRESENT_DELTA  = 14;
static const int      PHOTO_WATER_ABSENT_DELTA   = 6;
static const float    PHOTO_BASELINE_ALPHA        = 0.015f;
static const uint8_t  PHOTO_MEDIAN_SAMPLES        = 5;   // must be odd
static const float    PHOTO_FILTER_ALPHA          = 0.30f;

// ---------- Water level filtering/tuning ----------
static const float    LEVEL_FILTER_ALPHA          = 0.07f;
static const uint8_t  LEVEL_MEDIAN_SAMPLES         = 5;   // must be odd
static const float    LEVEL_DECREASING_DROP_EPS   = 8.0f;   // ADC units drop per window
static const float    LEVEL_DECREASING_RATE_EPS   = 60.0f;  // ADC units per minute

// ---------- Photo fraction thresholds ----------
//   waterPresent : photoPresentFrac >= PHOTO_PRESENT_FRAC_THRESHOLD  (>= 25%)
//   grey zone    : PHOTO_ABSENT_FRAC_THRESHOLD < frac < PHOTO_PRESENT_FRAC_THRESHOLD
//   waterAbsent  : photoPresentFrac <= PHOTO_ABSENT_FRAC_THRESHOLD   (<=  8%)
//
// Grey zone is treated as waterAbsent for classification: partial/intermittent
// flow is never silently labelled NORMAL_FLOW.
static const float    PHOTO_PRESENT_FRAC_THRESHOLD = 0.25f;
static const float    PHOTO_ABSENT_FRAC_THRESHOLD  = 0.08f;

static const uint32_t FILLED_OCCLUSION_CONFIRM_MS = 2 * WINDOW_MS; // 10 s

// ---------- Status ----------
enum class Status {
  CALIBRATING,
  NORMAL_FLOW,
  OCCLUSION,
  LEAK
};

struct Rates {
  bool     waterPresent;   // frac >= PHOTO_PRESENT_FRAC_THRESHOLD
  bool     waterAbsent;    // frac <= PHOTO_ABSENT_FRAC_THRESHOLD  (grey = neither)
  float    photoPresentFrac;
  float    levelRateUPM;   // ADC units / minute
  float    levelDelta;     // ADC units drop over the window
  uint32_t photoPresentSamplesInWindow;
  uint32_t photoSamplesInWindow;
};

// ---------- State ----------
static uint32_t lastSampleMs  = 0;
static uint32_t lastDisplayMs = 0;

static bool  photoInit       = false;
static bool  photoFilterInit = false;
static bool  levelInit       = false;

static float photoBaseline        = 0.0f;
static float photoFiltered        = 0.0f;
static float levelFiltered        = 0.0f;

static bool  photoWaterPresentNow = false;

static uint32_t windowStartMs                      = 0;
static float    levelAtWindowStart                 = 0.0f;
static uint32_t photoPresentSamplesInCurrentWindow = 0;
static uint32_t photoSamplesInCurrentWindow        = 0;

static Rates currentRates   = {false, true, 0.0f, 0.0f, 0.0f, 0, 0};
static bool  haveRates      = false;
static bool  newWindowReady = false;

static bool     warmupReady  = false;
static uint32_t calibStartMs = 0;

static uint32_t filledOcclusionStartMs = 0;

// FIX A: lastCandidate stores the most recent window classification so
// updateCommittedStatus() can be called on every tick to honour sTime precisely.
static Status   lastCandidate    = Status::CALIBRATING;

static Status   committedStatus  = Status::CALIBRATING;
static Status   candidateStatus  = Status::CALIBRATING;
static uint32_t candidateSinceMs = 0;

// ---------- Helpers ----------
static const char* statusToText(Status s) {
  switch (s) {
    case Status::CALIBRATING: return "CALIBRATING";
    case Status::NORMAL_FLOW: return "NORMAL FLOW";
    case Status::OCCLUSION:   return "OCCLUSION";
    case Status::LEAK:        return "LEAK";
    default:                  return "UNKNOWN";
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

// FIX B: Label changed from "dL" (delta) to "Rt" (rate) to match the value shown.
// The value is levelRateUPM (ADC units/min), not levelDelta.
static void buildRatesLine(char out[17]) {
  long l = lroundf(currentRates.levelRateUPM);
  l = constrain(l, -9999L, 9999L);
  snprintf(out, 17, "W:%c Rt:%4ld",
           currentRates.waterPresent ? 'Y' : 'N', l);
}

// ---------- Photo reading (median + EMA) ----------
static int readDenoisedPhotoAnalog() {
  int samples[PHOTO_MEDIAN_SAMPLES];
  for (uint8_t i = 0; i < PHOTO_MEDIAN_SAMPLES; ++i) {
    samples[i] = analogRead(PIN_PHOTO);
    delayMicroseconds(80);
  }
  for (uint8_t i = 1; i < PHOTO_MEDIAN_SAMPLES; ++i) {
    int key = samples[i], j = i;
    while (j > 0 && samples[j - 1] > key) { samples[j] = samples[j - 1]; --j; }
    samples[j] = key;
  }
  const int medianRaw = samples[PHOTO_MEDIAN_SAMPLES / 2];

  if (!photoFilterInit) {
    photoFiltered   = (float)medianRaw;
    photoFilterInit = true;
  } else {
    photoFiltered = (1.0f - PHOTO_FILTER_ALPHA) * photoFiltered
                  + PHOTO_FILTER_ALPHA * (float)medianRaw;
  }
  return (int)lroundf(photoFiltered);
}

// ---------- Level reading (median) ----------
static int readMedianAnalog(uint8_t pin) {
  int samples[LEVEL_MEDIAN_SAMPLES];
  for (uint8_t i = 0; i < LEVEL_MEDIAN_SAMPLES; ++i) {
    samples[i] = analogRead(pin);
    delayMicroseconds(120);
  }
  for (uint8_t i = 1; i < LEVEL_MEDIAN_SAMPLES; ++i) {
    int key = samples[i], j = i;
    while (j > 0 && samples[j - 1] > key) { samples[j] = samples[j - 1]; --j; }
    samples[j] = key;
  }
  return samples[LEVEL_MEDIAN_SAMPLES / 2];
}

// ---------- Photo water-presence (directional hysteresis) ----------
static void updatePhotoPresenceState(int photoRaw) {
  if (!photoInit) {
    photoBaseline = (float)photoRaw;
    photoInit     = true;
  }

  const float directionalDelta = PHOTO_WATER_MAKES_DARKER
      ? (photoBaseline - (float)photoRaw)
      : ((float)photoRaw - photoBaseline);

  if (!photoWaterPresentNow && directionalDelta >= (float)PHOTO_WATER_PRESENT_DELTA) {
    photoWaterPresentNow = true;
  } else if (photoWaterPresentNow && directionalDelta <= (float)PHOTO_WATER_ABSENT_DELTA) {
    photoWaterPresentNow = false;
  }

  if (!photoWaterPresentNow || directionalDelta < 0.0f) {
    photoBaseline = (1.0f - PHOTO_BASELINE_ALPHA) * photoBaseline
                  + PHOTO_BASELINE_ALPHA * (float)photoRaw;
  }
}

// ---------- Window / rate computation ----------
static void rollWindowAndCompute(uint32_t nowMs) {
  if (nowMs - windowStartMs >= WINDOW_MS) {
    const float minutes    = (float)(nowMs - windowStartMs) / 60000.0f;
    const float levelDelta = levelFiltered - levelAtWindowStart;

    const uint32_t totalSamples = photoSamplesInCurrentWindow;
    const float frac = (totalSamples > 0)
        ? ((float)photoPresentSamplesInCurrentWindow / (float)totalSamples)
        : 0.0f;

    currentRates.photoPresentSamplesInWindow = photoPresentSamplesInCurrentWindow;
    currentRates.photoSamplesInWindow        = totalSamples;
    currentRates.photoPresentFrac            = frac;
    currentRates.waterPresent = (frac >= PHOTO_PRESENT_FRAC_THRESHOLD);
    currentRates.waterAbsent  = (frac <= PHOTO_ABSENT_FRAC_THRESHOLD);
    currentRates.levelDelta    = levelDelta;
    currentRates.levelRateUPM  = (minutes > 0.0f) ? (levelDelta / minutes) : 0.0f;

    haveRates      = true;
    newWindowReady = true;

    windowStartMs                      = nowMs;
    levelAtWindowStart                 = levelFiltered;
    photoSamplesInCurrentWindow        = 0;
    photoPresentSamplesInCurrentWindow = 0;
  }
}

// ---------- Calibration ----------
static void updateCalibration(uint32_t nowMs) {
  if ((nowMs - calibStartMs) >= CALIBRATION_MS && haveRates) {
    warmupReady = true;
  }
}

// ---------- Classification (candidate) ----------
// FIX C: waterAbsent is now used directly so the absent / grey distinction is
// explicit in the code and matches the comments and struct documentation.
// Grey zone (waterPresent=false, waterAbsent=false) still maps to the same
// outcome as absent, but via an explicit branch rather than implicit fall-through.
static Status classifyCandidate(const Rates& r, uint32_t nowMs) {
  if (!warmupReady || !haveRates) {
    filledOcclusionStartMs = 0;
    return Status::CALIBRATING;
  }

  const bool levelDecreasing =
      (r.levelDelta    <= -LEVEL_DECREASING_DROP_EPS) ||
      (r.levelRateUPM  <= -LEVEL_DECREASING_RATE_EPS);

  // Three explicit water zones:
  //   waterPresent = true                    -> clearly present
  //   waterAbsent  = true                    -> clearly absent
  //   waterPresent = false, waterAbsent = false -> grey (ambiguous)
  // For classification, grey is treated the same as absent.
  const bool waterClearlyPresent = r.waterPresent;
  const bool waterAbsentOrGrey   = !r.waterPresent; // covers absent AND grey

  if (levelDecreasing) {
    filledOcclusionStartMs = 0;
    // Level dropping + water clearly in tube -> normal drip infusion.
    // Level dropping + tube dry or ambiguous -> fluid escaping without passing
    //   through the sensor (leak, dislodged line, cracked bag, etc.).
    return waterClearlyPresent ? Status::NORMAL_FLOW : Status::LEAK;
  }

  // Level is NOT decreasing past this point.

  if (waterAbsentOrGrey) {
    // Tube appears dry (absent zone) or ambiguous (grey zone) with no level drop.
    // No confirmation timer: if the tube looks empty and nothing is draining,
    // call it an occlusion immediately.
    filledOcclusionStartMs = 0;
    return Status::OCCLUSION;
  }

  // Water IS clearly present but level is not dropping.
  // Could be a primed tube at startup or a genuine full-tube occlusion.
  // Require the condition to persist for FILLED_OCCLUSION_CONFIRM_MS (10 s)
  // before committing, to avoid false alarms during brief flow pauses.
  if (filledOcclusionStartMs == 0) filledOcclusionStartMs = nowMs;
  if (nowMs - filledOcclusionStartMs >= FILLED_OCCLUSION_CONFIRM_MS) {
    return Status::OCCLUSION;
  }

  return Status::NORMAL_FLOW;
}

// ---------- Stability gating ----------
// FIX A: Called on every tick (not just on new windows) so sTime is honoured
// with ~50ms resolution rather than being rounded up to the next 5s window.
static void updateCommittedStatus(Status newCandidate, uint32_t nowMs) {
  if (newCandidate != candidateStatus) {
    candidateStatus  = newCandidate;
    candidateSinceMs = nowMs;
    return;
  }
  if (committedStatus != candidateStatus) {
    if (nowMs - candidateSinceMs >= STABLE_TIME_MS) {
      committedStatus = candidateStatus;
    }
  }
}

// =============================================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_PHOTO, INPUT);
  pinMode(PIN_LEVEL, INPUT);

  lcd.begin(16, 2);
  lcdPrintPadded("IV Monitor", "Starting...");

  const uint32_t startMs = millis();
  calibStartMs   = startMs;
  windowStartMs  = startMs;

  committedStatus  = Status::CALIBRATING;
  candidateStatus  = Status::CALIBRATING;
  lastCandidate    = Status::CALIBRATING;
  candidateSinceMs = startMs;
}

// =============================================================================
void loop() {
  const uint32_t nowMs = millis();

  // ---------- Sampling ----------
  if (nowMs - lastSampleMs >= SAMPLE_MS) {
    lastSampleMs = nowMs;

    const int photoRaw = readDenoisedPhotoAnalog();
    const int levelRaw = readMedianAnalog(PIN_LEVEL);

    if (!levelInit) {
      levelFiltered = (float)levelRaw;
      levelInit     = true;
    } else {
      levelFiltered = (1.0f - LEVEL_FILTER_ALPHA) * levelFiltered
                    + LEVEL_FILTER_ALPHA * (float)levelRaw;
    }

    updatePhotoPresenceState(photoRaw);
    photoSamplesInCurrentWindow++;
    if (photoWaterPresentNow) photoPresentSamplesInCurrentWindow++;

    newWindowReady = false;
    rollWindowAndCompute(nowMs);

    if (!warmupReady) {
      updateCalibration(nowMs);
      // During warmup the committed status is always CALIBRATING.
      updateCommittedStatus(Status::CALIBRATING, nowMs);
    } else {
      // FIX A: Update lastCandidate only when a fresh window is ready.
      // Then call updateCommittedStatus every tick so the 1-second gate
      // is checked with full 50ms resolution.
      if (newWindowReady) {
        lastCandidate = classifyCandidate(currentRates, nowMs);
      }
      updateCommittedStatus(lastCandidate, nowMs);
    }

    // FIX D: Serial debug prints only when a new window is ready, so each
    // unique set of values appears exactly once instead of ~100 times.
    if (newWindowReady) {
      Serial.print("cand=");        Serial.print(statusToText(candidateStatus));
      Serial.print(" comm=");       Serial.print(statusToText(committedStatus));
      Serial.print(" water=");      Serial.print(currentRates.waterPresent ? "Y" : "N");
      Serial.print(" absent=");     Serial.print(currentRates.waterAbsent  ? "Y" : "N");
      Serial.print(" pSamp=");      Serial.print(currentRates.photoPresentSamplesInWindow);
      Serial.print("/");            Serial.print(currentRates.photoSamplesInWindow);
      Serial.print(" pFrac=");      Serial.print(currentRates.photoPresentFrac, 2);
      Serial.print(" levelRt=");    Serial.print(currentRates.levelRateUPM, 1);
      Serial.print(" levelDelta="); Serial.println(currentRates.levelDelta, 1);
    }
  }

  // ---------- Display ----------
  if (nowMs - lastDisplayMs >= DISPLAY_MS) {
    lastDisplayMs = nowMs;

    char line1[17];
    char line2[17];

    snprintf(line1, 17, "%-16s", statusToText(committedStatus));

    if (!warmupReady) {
      const uint32_t elapsed = nowMs - calibStartMs;
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
