#include <Wire.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WiFiUDP.h>

// --- LED & UDP Setup ---
#define NUM_LEDS 514
#define DATA_PIN D1  // WS2812B data pin (with debug LED)
#define UDP_SIGNAL_PIN D10
#define UDP_TX_PACKET_MAX_SIZE 1200

unsigned long udpPacketCounter = 0;

CRGB leds[NUM_LEDS];
const int maxBrightness = 150;

WiFiUDP UDP;
const unsigned int localPort = 8787;
static uint8_t packetBuffer[UDP_TX_PACKET_MAX_SIZE];
const char ReplyBuffer[] = "pong";

int red = 0, green = 0, blue = 0;
unsigned long lastUdpTime = 0;
unsigned int ledCounter = 0, totalLedCounter = 0;
bool active = true;

void fillRange(int _start, int _end, int r, int g, int b)
{
   for (int n = _start; n < NUM_LEDS && n < _end; n++)
   {
      leds[n] = CRGB(r, (int)(g * 0.85), (b * 3) >> 2);
   }
   FastLED.show();
}

// Blink the debug LED on the WS2812 data pin before handing it to FastLED
void blinkDataPinStartup(int times, int delayMs)
{
   pinMode(DATA_PIN, OUTPUT);
   for (int i = 0; i < times; i++)
   {
      digitalWrite(DATA_PIN, HIGH);
      delay(delayMs);
      digitalWrite(DATA_PIN, LOW);
      delay(delayMs);
   }
}

void setup()
{
   pinMode(UDP_SIGNAL_PIN, OUTPUT);
   digitalWrite(UDP_SIGNAL_PIN, LOW);

   Serial.begin(115200);

   // Blink D1 before FastLED configures it for WS2812
   blinkDataPinStartup(3, 200);

   // Now hand pin D1 over to FastLED
   FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
   FastLED.setBrightness(maxBrightness);

   WiFi.config(IPAddress(192, 168, 8, 132), IPAddress(192, 168, 8, 1), IPAddress(255, 255, 255, 0));
   WiFi.begin("Synless_Wifi", "");

   while (WiFi.status() != WL_CONNECTED)
   {
      delay(20);
      Serial.print(".");
   }

   Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

   if (UDP.begin(localPort) == 1)
   {
      Serial.print("UDP listener started on port ");
      Serial.println(localPort);
   }

   lastUdpTime = millis();
}

void loop()
{
   int packetSize = UDP.parsePacket();
   if (packetSize > 0 && packetSize <= UDP_TX_PACKET_MAX_SIZE)
   {
      int len = UDP.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
      if (len <= 0) return;

      udpPacketCounter++;
      Serial.print("UDP Packet #: ");
      Serial.println(udpPacketCounter);

      lastUdpTime = millis();
      active = true;

      if (packetBuffer[0] == 0 && len == 5 && packetBuffer[1] == 'p' && packetBuffer[2] == 'i' && packetBuffer[3] == 'n' && packetBuffer[4] == 'g')
      {
         UDP.beginPacket(UDP.remoteIP(), localPort);
         UDP.write((const uint8_t*)ReplyBuffer, strlen(ReplyBuffer));
         UDP.endPacket();
      }
      else if (packetBuffer[0] == 2 || packetBuffer[0] == 3)
      {
         int ledNb = (len - 1) / 3;
         if (ledNb > NUM_LEDS) ledNb = NUM_LEDS;

         while (ledCounter < ledNb)
         {
            int offset = 1 + ledCounter * 3;
            if (offset + 2 >= len) break;

            red = packetBuffer[offset];
            green = packetBuffer[offset + 1];
            blue = packetBuffer[offset + 2];

            int ledIndex = ledCounter + totalLedCounter;
            if (ledIndex >= NUM_LEDS) break;

            leds[ledIndex] = CRGB(red, (int)(green * 0.85), (blue * 3) >> 2);
            ledCounter++;
         }

         totalLedCounter += ledCounter;
         ledCounter = 0;

         if (packetBuffer[0] == 3)
         {
            Serial.println(".");
            FastLED.show();
            totalLedCounter = 0;
         }
      }
   }

   // Timeout after 5 seconds of no data
   if ((millis() - lastUdpTime > 5000) && active)
   {
      active = false;
      lastUdpTime = millis();
      fillRange(0, NUM_LEDS, 0, 0, 0);  // Turn off LEDs on timeout
      Serial.println("UDP timeout: LEDs cleared");
   }

   digitalWrite(UDP_SIGNAL_PIN, active ? HIGH : LOW);
}
