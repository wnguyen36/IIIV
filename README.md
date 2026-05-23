# IIIV — Multisensor IV Failure-Mode Detection System

> Real-time IV infusion monitoring for understaffed clinical environments. Built in 36 hours at MedTech Hackathon 2026.

**IIIV** is a low-cost IoT attachment that monitors IV bags in real time, classifies infusion failure modes through multi-sensor fusion, and pushes early-warning alerts to a centralized React dashboard for hospital-wide patient telemetry — designed for the low-resource care settings where missed IV complications most often drive patient deterioration.

> The name **IIIV** is a play on "IV," with three I's for the three states the system detects: occlusion, leak, and normal flow.

📊 **[Pitch Deck](./docs/IIIV-pitch-deck.pdf)** · 🎥 **[Demo Video](./docs/demo.mp4)** · 🔌 **[Wiring Diagram](./docs/wiring.png)** · 🖥️ **[Dashboard](./dashboard)**

---

![IIIV prototype on IV bag](./docs/demo.gif)
![Nursing dashboard](./docs/dashboard.png)

---

## Highlights

- **End-to-end clinical workflow** — edge device to care-team interface, built in a 36-hour sprint
- **Multi-sensor fusion classifier** — disambiguates three failure modes that look identical from any single sensor
- **Low-BOM hardware** — deliberate design constraint for scalability in cost-sensitive deployments
- **Real-time alerting pipeline** — Arduino firmware streams state changes to a React dashboard for ward-wide monitoring
- **Full stack, solo-readable** — firmware (C++ / Arduino) and frontend (React) both in this repo

## Clinical Context

IV complications — occlusions, leaks, and infiltration — are among the most common categories of preventable inpatient harm. In understaffed or low-resource settings, missed events go unnoticed for long stretches, driving delayed treatment, wasted medication, and patient deterioration. Existing smart-IV systems exist but are cost-prohibitive for the environments that need them most. IIIV targets that gap.

## Technical Approach

Two sensors feed an Arduino-based classifier:

- **Photoresistor** on the drip chamber → drip rate via light interruption
- **SL067 water-level sensor** on the bag → fluid volume over time

Fusing the two signals disambiguates failure modes that are indistinguishable from either sensor alone:

| State | Drips at chamber | Fluid level dropping |
|---|---|---|
| Normal flow | ✅ | ✅ |
| Occlusion | ❌ | ❌ |
| Leak | ❌ | ✅ |

Classified state changes stream to a React dashboard that aggregates patient telemetry across an entire ward, so a single nurse can monitor every IV bag from one screen.

## Stack

| Layer | Tech |
|---|---|
| Firmware | Arduino, C++ |
| Sensors | Photoresistor, SL067 water-level sensor |
| Frontend | React |
| Communication | [SERIAL / WIFI / BLUETOOTH — fill in what you actually used] |
| Hardware | [ARDUINO BOARD — e.g. Arduino Uno R3], breadboard, [VALUE]Ω resistor |

## Repo Structure
