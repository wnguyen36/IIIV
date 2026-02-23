# MedTech Hackathon 2026 | Team Phlegm | IV Bag Classification System
* All code should be ran in the Arduino IDE.
* Purpose: Detects and classifies occlusions, leaks, (reduced/suspect flow), and normal flow within IV bags. Can be then connected to an external platform to send alerts whenever an IV bag falls into a critical state. 
1. WaterLevel_DripRate.ino: Analyzes and compares drip rate from a photoresistor attached to the drip chamber to the fluid level within the bag. Should work more accurately with real medical-grade IV bags.
2. photoresistor_boolean.ino: Code that was used for our prototype IV bag. Reads water prescence as a boolean rather than drip rate. Works in the context of our team's handmade IV bag but would be less accurate in real medical situations.
