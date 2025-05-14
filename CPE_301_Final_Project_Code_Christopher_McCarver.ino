#include <DHT.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>

//DHT11 
#define DHTPIN 7
#define DHTTYPE DHT11
#define WATER_SENSOR_PIN A0
DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

RTC_DS1307 rtc;

// LEDs & Buttons
volatile unsigned char* port_a = (unsigned char*) 0x22;
volatile unsigned char* ddr_a  = (unsigned char*) 0x21;
volatile unsigned char* pin_a  = (unsigned char*) 0x20;

// Fan
volatile unsigned char* port_h = (unsigned char*) 0x102;
volatile unsigned char* ddr_h  = (unsigned char*) 0x101;
volatile unsigned char* pin_h  = (unsigned char*) 0x100;

// Stepper Motor 
volatile unsigned char* port_c = (unsigned char*) 0x28;  // PORTC (IN1, IN2)
volatile unsigned char* ddr_c  = (unsigned char*) 0x27;
volatile unsigned char* port_d = (unsigned char*) 0x2B;  // PORTD (IN3)
volatile unsigned char* ddr_d  = (unsigned char*) 0x2A;
volatile unsigned char* port_g = (unsigned char*) 0x34;  // PORTG (IN4)
volatile unsigned char* ddr_g  = (unsigned char*) 0x33;

//bit Masks 
#define LED_GREEN_BIT   4   // PA4=Pin26
#define LED_YELLOW_BIT  5   // PA5=Pin27
#define LED_BLUE_BIT    3   // PA3=Pin25
#define LED_RED_BIT     6   // PA6=Pin28
#define BTN_BLUE_BIT    2   // PA2=Pin24
#define BTN_GREEN_BIT   1   // PA1=Pin23
#define BTN_RED_BIT     0   // PA0=Pin22
#define IN1_BIT         1   // PC1=Pin36
#define IN2_BIT         0   // PC0=Pin37
#define IN3_BIT         7   // PD7=Pin38
#define IN4_BIT         2   // PG2=Pin39
#define FAN_BIT         3   // PH3=Pin6

unsigned long lastIdle = 0;
unsigned long lastBlink = 0;
unsigned long lastLCD = 0;
int lcdScreenIndex = 0;
bool gLED = true;

enum SystemState { ERROR_MODE, IDLE_MODE, ACTIVE_MODE, COMPLETE_MODE, DISABLED_MODE };
SystemState currentState = ERROR_MODE;

unsigned long lastSensorDisplay = 0;  

int stepIndex = 0;
const int stepSequence[8][4] = {
  {1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
  {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}
};

void stepMotor(int index) {
  if (stepSequence[index][0]) *port_c |=  (1 << IN1_BIT); else *port_c &= ~(1 << IN1_BIT);
  if (stepSequence[index][1]) *port_c |=  (1 << IN2_BIT); else *port_c &= ~(1 << IN2_BIT);
  if (stepSequence[index][2]) *port_d |=  (1 << IN3_BIT); else *port_d &= ~(1 << IN3_BIT);
  if (stepSequence[index][3]) *port_g |=  (1 << IN4_BIT); else *port_g &= ~(1 << IN4_BIT);
}

void setLEDs(bool green, bool yellow, bool blue, bool red) {
  if (green)
    *port_a |= (1 << LED_GREEN_BIT);
  else
    *port_a &= ~(1 << LED_GREEN_BIT);
  if (yellow)
    *port_a |= (1 << LED_YELLOW_BIT);
  else
    *port_a &= ~(1 << LED_YELLOW_BIT);
  if (blue)
    *port_a |= (1 << LED_BLUE_BIT);
  else
    *port_a &= ~(1 << LED_BLUE_BIT);
  if (red)
    *port_a |= (1 << LED_RED_BIT);
  else
    *port_a &= ~(1 << LED_RED_BIT);
  gLED = green;
}

void resumeFromDisabled() {
  currentState = IDLE_MODE;
  setLEDs(true, false, false, false);
  lcd.clear();

  DateTime nowTime = rtc.now();
  Serial.print("Resumed from DISABLED at: ");
  Serial.print(nowTime.hour()); Serial.print(":");
  Serial.print(nowTime.minute()); Serial.print(":");
  Serial.println(nowTime.second());

  lastIdle = millis();
}

volatile bool resumeFlag = false;

void resumeISR() {
  resumeFlag = true;
}

void setup() {
  dht.begin();
  lcd.begin(16, 2);
  Serial.begin(9600);

  *ddr_a |= (1 << LED_GREEN_BIT) | (1 << LED_YELLOW_BIT) | (1 << LED_BLUE_BIT) | (1 << LED_RED_BIT);
  *ddr_h |= (1 << FAN_BIT);
  *ddr_c |= (1 << IN1_BIT) | (1 << IN2_BIT); 
  *ddr_d |= (1 << IN3_BIT);                  
  *ddr_g |= (1 << IN4_BIT);                  

  *port_h &= ~(1 << FAN_BIT);

  *ddr_a &= ~((1 << BTN_BLUE_BIT) | (1 << BTN_GREEN_BIT) | (1 << BTN_RED_BIT));
  *port_a |= (1 << BTN_BLUE_BIT) | (1 << BTN_GREEN_BIT) | (1 << BTN_RED_BIT);

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1); 
  }

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); 
  }

  currentState = ERROR_MODE;

  setLEDs(false, false, false, true);

  *port_c |= (1 << IN4_BIT);
  delay(1000);
  *port_c &= ~(1 << IN4_BIT);

  attachInterrupt(digitalPinToInterrupt(23), resumeISR, FALLING);
}

void loop() {
  unsigned long now = millis();

  if (currentState == DISABLED_MODE && resumeFlag) {
    noInterrupts();
    resumeFlag = false;
    interrupts();
    resumeFromDisabled();
    delay(500);
  }

  if (currentState == ACTIVE_MODE) {
    float temp = dht.readTemperature();
    int waterReading = readWaterSensor();

    Serial.print("Temp = "); Serial.print(temp);
    Serial.print(" | Water = "); Serial.println(waterReading);

    if (waterReading < 400) {
      currentState = ERROR_MODE;
      setLEDs(false, false, false, true);
      *port_h &= ~(1 << FAN_BIT);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("ERROR: LOW WATER");
      lcd.setCursor(0, 1); lcd.print("Fan OFF");
      Serial.println("Water LOW, Moving to ERROR_MODE.");
    }
    else if (waterReading >= 600 && temp < 25.0) {
      currentState = COMPLETE_MODE;
      setLEDs(true, false, true, false); // GREEN + BLUE
      *port_h &= ~(1 << FAN_BIT);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("COMPLETE MODE");
      lcd.setCursor(0, 1); lcd.print("Stable & Cool");

      DateTime nowTime = rtc.now();
      Serial.print("Entered COMPLETE_MODE at ");
      Serial.print(nowTime.hour()); Serial.print(":");
      Serial.print(nowTime.minute()); Serial.print(":");
      Serial.println(nowTime.second());
    }
    else if (waterReading <= 580) {
      currentState = IDLE_MODE;
      setLEDs(true, false, false, false);
      *port_h &= ~(1 << FAN_BIT);
      lcd.clear();
      Serial.println("Water MEDIUM, Moving to IDLE_MODE");
    }
  }


  if (!(*pin_a & (1 << BTN_RED_BIT)) && currentState != DISABLED_MODE) {
    currentState = DISABLED_MODE;
    setLEDs(false, true, false, false);
    *port_h &= ~(1 << FAN_BIT); 
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DISABLED MODE");
    lcd.setCursor(0, 1);
    lcd.print("Press GREEN btn");
    Serial.println("System disabled by user.");
    delay(500);
  }

  if ((currentState == ERROR_MODE || currentState == DISABLED_MODE) && !(*pin_a & (1 << BTN_GREEN_BIT))) {
    SystemState previousState = currentState;

    if (previousState == ERROR_MODE) {
      int waterReading = readWaterSensor();
      if (waterReading < 400) {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("WATER LOW");
        lcd.setCursor(0, 1); lcd.print("Can't resume");
        delay(1000);
        return;  
      }
    }

    currentState = IDLE_MODE;
    setLEDs(true, false, false, false);

    DateTime nowTime = rtc.now();
    Serial.print("Resumed from ");
    Serial.print(previousState == ERROR_MODE ? "ERROR" : "DISABLED");
    Serial.print(" at: ");
    Serial.print(nowTime.hour()); Serial.print(":");
    Serial.print(nowTime.minute()); Serial.print(":");
    Serial.println(nowTime.second());

    lastIdle = now;
    delay(500); 
  }

  if ((currentState == IDLE_MODE || currentState == ERROR_MODE)) {
    if (now - lastLCD >= 5000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(currentState == IDLE_MODE ? "IDLE Mode" : "ERROR NEED WATER");
      lcd.setCursor(0, 1);
      DateTime nowTime = rtc.now();
      lcd.print("Time: ");
      if (nowTime.hour() < 10) lcd.print('0'); lcd.print(nowTime.hour()); lcd.print(":");
      if (nowTime.minute() < 10) lcd.print('0'); lcd.print(nowTime.minute()); lcd.print(":");
      if (nowTime.second() < 10) lcd.print('0'); lcd.print(nowTime.second());
      lastLCD = now;
    }

    if (now - lastSensorDisplay >= 60000 && currentState == IDLE_MODE) {
      float temp = dht.readTemperature();
      float hum = dht.readHumidity();

      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Temp: "); lcd.print(temp, 1); lcd.print(" C");
      lcd.setCursor(0, 1); lcd.print("Hum: "); lcd.print(hum, 1); lcd.print(" %");

      Serial.print("[Sensor Update] Temp: "); Serial.print(temp); Serial.print(" C | Hum: ");
      Serial.print(hum); Serial.println(" %");

      lastSensorDisplay = now;
    }
  }

  if (currentState == IDLE_MODE && now - lastBlink >= 2000) {
    lastBlink = now;
    gLED = !gLED;

    if (gLED) {
      *port_a |= (1 << LED_GREEN_BIT); 
    } else {
      *port_a &= ~(1 << LED_GREEN_BIT); 
    }
  }

  if (!(*pin_a & (1 << BTN_BLUE_BIT)) && (currentState == IDLE_MODE || currentState == ACTIVE_MODE)) {
    currentState = ERROR_MODE;
    setLEDs(false, false, false, true);  
    *port_h &= ~(1 << FAN_BIT);  
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Stopped");
    lcd.setCursor(0, 1); lcd.print("in ERROR");
    delay(500);
  }

  else if (currentState == IDLE_MODE) {
    int waterReading = readWaterSensor();
    float temp = dht.readTemperature();
    Serial.print("Temp = "); Serial.print(temp);
    Serial.print(" | Water = "); Serial.println(waterReading);

    if (waterReading < 400) {
      currentState = ERROR_MODE;
      setLEDs(false, false, false, true);
      *port_h &= ~(1 << FAN_BIT);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("ERROR: LOW WATER");
      lcd.setCursor(0, 1); lcd.print("Fan OFF");
      Serial.println("IDLE, Water LOW, ERROR_MODE.");
    }
    else if (waterReading >= 550 && temp >= 22.0) {
      currentState = ACTIVE_MODE;
      setLEDs(false, false, true, false); // BLUE
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Water HIGH");
      lcd.setCursor(0, 1); lcd.print("Fan ON");
      *port_h |=  (1 << FAN_BIT);
      Serial.println("Water HIGH: Fan ON. ACTIVE_MODE.");
    }
    else if (waterReading >= 575 && temp < 25.0) {
      currentState = COMPLETE_MODE;
      setLEDs(true, false, true, false); // GREEN + BLUE
      *port_h &= ~(1 << FAN_BIT);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("COMPLETE MODE");
      lcd.setCursor(0, 1); lcd.print("Stable & Cool");

      DateTime nowTime = rtc.now();
      Serial.print("Entered COMPLETE_MODE at ");
      Serial.print(nowTime.hour()); Serial.print(":");
      Serial.print(nowTime.minute()); Serial.print(":");
      Serial.println(nowTime.second());
    }
  }


  if (currentState != ERROR_MODE && currentState != DISABLED_MODE && currentState != COMPLETE_MODE) {
    int potVal = readPotentiometer();
    int newStepIndex = map(potVal, 0, 1023, 0, 7);

    Serial.print("Potentiometer ADC: ");
    Serial.print(potVal);
    Serial.print(" → newStepIndex = ");
    Serial.print(newStepIndex);
    Serial.print(" | stepIndex = ");
    Serial.println(stepIndex);

    if (abs(newStepIndex - stepIndex) >= 1) {
      bool fanWasOn = false;

      if (currentState == ACTIVE_MODE) {
        fanWasOn = true;
        *port_h &= ~(1 << FAN_BIT);
        delay(20);
      }

      int direction = (newStepIndex > stepIndex) ? 1 : -1;
      int stepsToMove = 1024;
      int stepDelay = 2;

      for (int i = 0; i < stepsToMove; i++) {
        stepIndex = (stepIndex + direction + 8) % 8;
        stepMotor(stepIndex);
        delay(stepDelay);

        if (i % 64 == 0) {
          DateTime nowTime = rtc.now();
          Serial.print("StepPos: "); Serial.print(stepIndex);
          Serial.print(" @ ");
          if (nowTime.hour() < 10) Serial.print('0'); Serial.print(nowTime.hour()); Serial.print(":");
          if (nowTime.minute() < 10) Serial.print('0'); Serial.print(nowTime.minute()); Serial.print(":");
          if (nowTime.second() < 10) Serial.print('0'); Serial.print(nowTime.second());
          Serial.println();
        }
      }

      stepIndex = newStepIndex;

      if (fanWasOn) {
        delay(20);
        *port_h |= (1 << FAN_BIT);
      }

      DateTime nowTime = rtc.now();
      Serial.print("Vent demo spin at ");
      if (nowTime.hour() < 10) Serial.print('0'); Serial.print(nowTime.hour()); Serial.print(":");
      if (nowTime.minute() < 10) Serial.print('0'); Serial.print(nowTime.minute()); Serial.print(":");
      if (nowTime.second() < 10) Serial.print('0'); Serial.print(nowTime.second());
      Serial.println();
    }
  }

  if (currentState == COMPLETE_MODE) {
    float temp = dht.readTemperature();
    int water = readWaterSensor();

    if (temp >= 25.0 || water < 600) {
      currentState = IDLE_MODE;
      setLEDs(true, false, false, false);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("COMPLETE → IDLE");

      DateTime nowTime = rtc.now();
      Serial.print("Exited COMPLETE_MODE at ");
      Serial.print(nowTime.hour()); Serial.print(":");
      Serial.print(nowTime.minute()); Serial.print(":");
      Serial.print(nowTime.second());
      Serial.println();
    }
  }
}

int readPotentiometer() {
  if (currentState != ERROR_MODE && currentState != DISABLED_MODE) {
    ADMUX = (1 << REFS0) | (1 << MUX1);  // MUX1 = 1 → ADC2
    ADCSRA = (1 << ADEN) | (1 << ADSC) |
             (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    while (ADCSRA & (1 << ADSC));
    return ADC;
  }
  return 0;
}

int readWaterSensor() {
  ADMUX = (1 << REFS0);  
  ADCSRA = (1 << ADEN) | (1 << ADSC) |
           (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
  while (ADCSRA & (1 << ADSC));
  return ADC;
}


