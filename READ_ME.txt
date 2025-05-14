CPE 301 Final Project 

Name: Christopher David McCarver  
Course: CPE 301.1001 Embedded Systems  
Professor: Dr. Bashria Akter Anima  
Date: 5/9/2025  


This project is a real-time environmental control system that monitors temperature, humidity, and water level using multiple sensors and controls a fan and stepper motor to simulate a swamp cooler or venting system.

It operates in five states:
- ERROR: when water is too low or sensors are disconnected
- IDLE: water is present but no fan or movement
- ACTIVE: fan is on, vent active
- COMPLETE: system cooled, fan off
- DISABLED: shutdown mode via button interrupt

Key Features
- Custom state machine using memory-mapped I/O 
- LCD display output 
- Manual mode transitions using push buttons
- Stepper motor control via potentiometer
- Fan activation/deactivation logic tied to sensor input
- Real-time clock (RTC) tracking with time-stamped logs


Video

(https://www.youtube.com/watch?v=YOUR_VIDEO_LINK)



Components

- Arduino Mega 2560
- DHT11 Temperature & Humidity Sensor
- Analog Water Level Sensor (A0)
- RTC DS1307 Module
- ULN2003 + 28BYJ-48 Stepper Motor + Driver
- 16x2 LCD (parallel interface)
- 5V DC Fan (via transistor)
- MB V2 Power Module (9V battery source)
- Pushbuttons (x3), LEDs (x4), Resistors (330Ω), Potentiometers (x2)
- Breadboards, jumper wires


Files:

- `CPE_301_Final_Project_Code_Christopher_McCarver.ino` — Full code
- `README.md` 
- `FinalReport.pdf` — report with schematics, component list, and code breakdown
- `IMG_1XXX.jpeg` — photos of the assembled project (add yours)
- `schematic_diagram.png` — Wiring diagram or hand-drawn schematic (add yours)






