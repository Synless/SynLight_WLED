#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "Adafruit_VL53L0X.h"

#define PIN_IN1 D2
#define PIN_IN2 D3
#define BUTTON_PIN D6
#define INPUT_UDP_SIGNAL_PIN D0
#define LED_DATA_PIN D1

#ifndef NUM_LEDS
#define NUM_LEDS 514
#endif

#define BRIGHTNESS 60

Adafruit_NeoPixel strip(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_VL53L0X vl = Adafruit_VL53L0X();

const unsigned long DEBOUNCE_MS = 30;
int mode = 0;
int lastStableBtn = HIGH;
int lastReading = HIGH;
unsigned long lastDebounceTime = 0;

int lastUdpState = LOW;

uint32_t warmAmberWhite;

bool sensorAvailable = false;
bool sensorArmed = true;

const uint16_t WINDOW_LEN = 140;
const uint16_t STEP_PIXELS = 7;
const unsigned long FRAME_DELAY_MS = 0;

void playWarmWipeBlocking()
{
   for (uint16_t head = 0; head <= NUM_LEDS + WINDOW_LEN; head += STEP_PIXELS)
   {
      uint16_t start = (head > WINDOW_LEN) ? (head - WINDOW_LEN) : 0;
      uint16_t end = (head < NUM_LEDS) ? head : NUM_LEDS;

      for (uint16_t i = 0; i < NUM_LEDS; i++)
      {
         if (i < start)
         {
            strip.setPixelColor(i, warmAmberWhite);
         }
         else if (i < end)
         {
            float t = float(i - start) / float(WINDOW_LEN);
            t = 1.0f - t;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float b = t * t * (3.0f - 2.0f * t);

            uint8_t r = (uint8_t)(255.0f * b);
            uint8_t g = (uint8_t)(140.0f * b);
            uint8_t bl = (uint8_t)(40.0f * b);

            strip.setPixelColor(i, strip.Color(r, g, bl));
         }
         else
         {
            strip.setPixelColor(i, 0);
         }
      }

      strip.show();
      delay(FRAME_DELAY_MS);
   }

   strip.fill(warmAmberWhite, 0, NUM_LEDS);
   strip.show();
}

void applyMode(int m)
{
   switch (m)
   {
      case 0:
         digitalWrite(PIN_IN2, HIGH);
         digitalWrite(PIN_IN1, HIGH);
         strip.fill(strip.Color(0, 0, 0), 0, NUM_LEDS);
         digitalWrite(PIN_IN2, HIGH);
         digitalWrite(PIN_IN1, LOW);
         Serial.println("Mode 0 → IN2=HIGH, IN1=LOW");
         break;

      case 1:
         digitalWrite(PIN_IN2, HIGH);
         digitalWrite(PIN_IN1, HIGH);
         Serial.println("Mode 1 → IN2=HIGH, IN1=LOW");
         break;

      case 2:
         digitalWrite(PIN_IN2, LOW);
         digitalWrite(PIN_IN1, LOW);
         Serial.println("Mode 2 → IN2=LOW, IN1=HIGH");
         break;
   }

   if (m == 1)
   {
      strip.clear();
      strip.show();
      playWarmWipeBlocking();
   }
   else
   {
      strip.clear();
      strip.show();
   }
}

void updateButton()
{
   int reading = digitalRead(BUTTON_PIN);
   if (reading != lastReading)
   {
      lastDebounceTime = millis();
      lastReading = reading;
   }

   if ((millis() - lastDebounceTime) > DEBOUNCE_MS)
   {
      if (reading != lastStableBtn)
      {
         lastStableBtn = reading;

         if (lastStableBtn == LOW)
         {
            int oldMode = mode;
            mode = (mode + 1) % 3;
            Serial.print("Button pressed → mode = ");
            Serial.println(mode);
            applyMode(mode);
            if (mode == 0 && oldMode != 0)
            {
               strip.clear();
               strip.show();
            }
         }
      }
   }
}

void updateUdpSignal()
{
   int current = digitalRead(INPUT_UDP_SIGNAL_PIN);

   if (lastUdpState == LOW && current == HIGH)
   {
      mode = 0;
      Serial.println("UDP rising edge → mode = 0");
      applyMode(mode);
   }

   lastUdpState = current;
}

void updateDistanceSensor()
{
   if (!sensorAvailable) return;

   VL53L0X_RangingMeasurementData_t measure;
   vl.rangingTest(&measure, false);

   if (measure.RangeStatus != 4)
   {
      if (sensorArmed)
      {
         sensorArmed = false;
         int oldMode = mode;
         mode = (mode + 1) % 3;
         Serial.print("Sensor valid → mode = ");
         Serial.println(mode);
         applyMode(mode);
         if (mode == 0 && oldMode != 0)
         {
            strip.clear();
            strip.show();
         }
      }
   }
   else
   {
      sensorArmed = true;
   }
}

void setup()
{
   Serial.begin(115200);

   pinMode(PIN_IN1, OUTPUT);
   pinMode(PIN_IN2, OUTPUT);
   pinMode(BUTTON_PIN, INPUT_PULLUP);
   pinMode(INPUT_UDP_SIGNAL_PIN, INPUT);

   strip.begin();
   strip.setBrightness(BRIGHTNESS);

   warmAmberWhite = strip.Color(255, 140, 40);

   strip.clear();
   strip.show();

   Wire.begin(D4, D5);
   sensorAvailable = vl.begin();

   applyMode(mode);
}

void loop()
{
   updateButton();
   updateDistanceSensor();
   updateUdpSignal();
}
