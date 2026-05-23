# IIIV — Multisensor Failure-Mode Detection for Gravity-Based Infusion Setups

> A $20 clip-on monitor that classifies IV failure modes in real time — built in 36 hours at MedTech Hackathon 2026.

**IIIV** is a low-cost, multi-sensor attachment for gravity-based IV systems that detects and classifies three failure modes — **normal flow, occlusion, and leak/disconnection** — through sensor fusion, then streams alerts to a centralized React dashboard for ward-wide monitoring. Designed for understaffed and low-resource clinical environments where existing smart-IV systems are cost-prohibitive.

> The name **IIIV** is a play on "IV," with three I's for the three states the system detects.

📊 **[Pitch Deck](./docs/IIIV-pitch-deck.pdf)** · 🎥 **[Demo Video](./docs/demo.mp4)**

---

![IIIV prototype on IV bag](./docs/demo.gif)
![Nursing dashboard](./docs/dashboard.png)

---

## Highlights

- **$20 prototype** vs. $400–$500+ for commercial competitors (DripAssist, Monidrop) — ~95% cost reduction
- **Three-state classification** through sensor fusion, addressing the monitoring gap that single-sensor drip monitors can't close
- **Clip-on form factor** — works with any standard gravity IV bag, no tubing modifications required
- **Real-time ward dashboard** — single nurse can monitor every IV bag on a floor from one screen
- **Offline-capable firmware** — runs directly on microcontroller, no internet dependency for the edge device
- **End-to-end stack** — Arduino/C++ firmware and React frontend, both in this repo

## Clinical Context

The deck cites two statistics that frame the problem:

- **36% of IV catheter transfusions fail before therapy completion** (Marsh et al., 2024)
- **72–99% of clinical alarms are false alarms**, driving alarm fatigue (Sendelbach & Funk, 2013)

These numbers describe the same root failure: gravity IV systems have a monitoring gap. Existing devices like Monidrop detect drop rate only, fire threshold-based alarms, and provide no failure classification or sensor redundancy. A nurse hearing an alarm doesn't know whether the bag is empty, occluded, or leaking — so alarms get ignored, and real complications get missed. IIIV closes that gap by classifying the failure mode itself, so alerts are actionable instead of ambient.

## Technical Approach

Two sensors feed an Arduino-based classifier:

- **Water-level sensor (SL067)** on the bag → tracks reservoir depletion trend
- **Photoresistor** on the drip chamber → tracks flow activity via light interruption

Fusing the two signals disambiguates failure modes that are indistinguishable from either sensor alone:

| Condition | Water Level Sensor | Photoresistor |
|---|---|---|
| **Normal** | Decreasing | Active |
| **Occlusion** | Stable | Inactive |
| **Leak / External** | Decreasing | Inactive |

Classified state changes stream to a React dashboard that aggregates patient telemetry across an entire ward. Alerts ping the originating room directly, so medical personnel can respond and triage without parsing which IV setup is failing.

## Cost Breakdown

| Component | Cost |
|---|---|
| Microcontroller | $10 |
| Breadboard | <$2 |
| LED (×2) | <$2.50 |
| Water-level sensor | $1 |
| Photoresistor | <$1 |
| Resistors, potentiometer, buzzer, wires | <$3.50 |
| **Total** | **~$20** |

| Competitor | Cost |
|---|---|
| DripAssist (Couperus, 2019) | ~$400 |
| Monidrop | $500+ |

> *Note: prototype-vs-finished-product comparison. The point isn't apples-to-apples — it's that the failure-mode classification capability is achievable at a fraction of incumbent BOM.*

## Stack

| Layer | Tech |
|---|---|
| Firmware | Arduino, C++ |
| Sensors | Photoresistor, SL067 water-level sensor |
| Frontend | React |
| Communication | [SERIAL / WIFI / BLUETOOTH — fill in what you actually used] |
| Hardware | [ARDUINO BOARD], breadboard, buzzer, LED, [VALUE]Ω resistor, potentiometer |

**Firmware notes:**
- `WaterLvl_DripRate.ino` is the production-intent build — analyzes drip rate against fluid level for use with medical-grade IV bags.
- `photoresistor_boolean.ino` is the hackathon prototype — reads water presence as a boolean to accommodate our team's handmade IV bag. Less accurate in a clinical context, but the right call given the sprint constraint.

## Setup

1. Clone the repo and open the desired `.ino` file in the Arduino IDE.
2. Wire components per .
3. Select your board and port, then upload.
4. Start the dashboard: `cd dashboard && npm install && npm run dev`

## Future Work

- **Movement ML model** — distinguish patient-movement artifacts from true failure signals (gravity IV pumps are sensitive to motion, which is a known false-alarm driver)
- **Predictive failure detection** — anticipate occlusion onset from upstream drip-rate trends rather than detecting after the fact
- **Sensor improvements** — higher-quality and longer-range water-level sensor; photoresistor calibration for varied ambient lighting
- **Enclosure design** — assembly and casing for clinical-environment durability

## Team Phlegm — MedTech Hackathon 2026

- William Nguyen
- Ruy Okaji
- Kristina Siju
- Shreshta Kallem
- Krithi Iyer

Won 2nd place in the MedTech Hackathon's Clinical Application Track.

## References

- Couperus, K., Kmiecik, K., & Kang, C. (2019). IV DripAssist: An innovative way to monitor intravenous infusions away from an outlet? *Military Medicine, 184*(Suppl. 1), 322–325.
- Marsh, N., et al. (2024). Peripheral intravenous catheter infection and failure: A systematic review and meta-analysis. *International Journal of Nursing Studies, 151*, 104673.
- Sendelbach, S., & Funk, M. (2013). Alarm Fatigue. *AACN Advanced Critical Care, 24*(4), 378–386.

Full reference list in the [pitch deck](./docs/IIIV-pitch-deck.pdf).
