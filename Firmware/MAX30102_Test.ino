/*
 * MAX30102 Test - BPM & SpO2 (Arduino ESP32)
 *
 * Wiring (MAX3010x breakout):
 *   VIN  -> 3.3V
 *   GND  -> GND
 *   SDA  -> GPIO21
 *   SCL  -> GPIO22
 *   INT  -> (optional) GPIO INT, unused in this sketch
 *
 * Library: SparkFun MAX3010x Pulse and Proximity Sensor Library
 *  (SparkFun MAX3010x Pulse and Proximity Sensor Library by SparkFun)
 *
 *   Sketch > Include Library > Manage Libraries > search "MAX3010x"
 *   also requires "SparkFun MAX3010x Pulse and Proximity Sensor Library"
 *   and Arduino's built-in Wire.
 *
 * INFO_LOGGING and DEBUG_LOGGING in SparkFun_SoftwareSerial / MAX30105
 * can be toggled in the library for extra serial debug.
 */

#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

#define MAX_BRIGHTNESS 255

// Buffers used by the SpO2 algorithm. "sampleAverage" of 4 with
// "sampleRate" of 100 -> storing ~4s window each time for auto-correlation.
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)
  // Arduino Uno doesn't have enough SRAM; shrink the buffers.
  uint32_t irBuffer[100];
  uint32_t redBuffer[100];
  int32_t bufferLength;
#else
  uint32_t irBuffer[200];
  uint32_t redBuffer[200];
  int32_t bufferLength;
#endif

int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

byte pulseLED = 11;   // must be PWM pin (blinks on each detected pulse)
byte readLED  = 13;   // built-in LED, lights every time a sample is read

void setup() {
  Serial.begin(115200);
  Serial.println(F("MAX30102 initializing..."));

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("MAX30102 was not found. Check wiring/power."));
    while (1);
  }
  Serial.println(F("MAX30102 found. Configuring..."));

  // Use a high pulsatile LED mode.
  particleSensor.setup(60, 4, 2, 200, 411, 0x1F4);

  // Turn off the secondary LED (S2) — MAX30102 has only one LED pair,
  // but this keeps the MAX30105-compatible call valid.
  particleSensor.enableSlot(2, false);
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x1F4);

  // Blink setup()
  particleSensor.shutDown();
  pinMode(pulseLED, OUTPUT);
  pinMode(readLED, OUTPUT);
}

void loop() {
  // Take 4 seconds of samples (100 samples/sec = 400), fill two buffers.
  bufferLength = 100; // 1 second worth of data (sampleRate 100)
  for (byte i = 0; i < bufferLength; i++) {
    while (particleSensor.available() == false)  // wait for fresh data
      particleSensor.check();

    redBuffer[i]   = particleSensor.getRed();
    irBuffer[i]    = particleSensor.getIR();
    particleSensor.nextSample();

    Serial.print(F("red="));
    Serial.print(redBuffer[i]);
    Serial.print(F(", ir="));
    Serial.println(irBuffer[i]);

    digitalWrite(readLED, !digitalRead(readLED));
  }

  // Calculate heart rate and SpO2 after first 4 seconds of data.
  maxim_heart_rate_and_oxygen_saturation(
    irBuffer, bufferLength, redBuffer,
    &spo2, &validSPO2,
    &heartRate, &validHeartRate);

  // Continuously take data from the sensor and feed it to the algorithm.
  while (1) {
    for (byte i = 25; i < 100; i++) {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25]  = irBuffer[i];
    }

    for (byte i = 75; i < 100; i++) {
      while (particleSensor.available() == false)
        particleSensor.check();

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i]  = particleSensor.getIR();
      particleSensor.nextSample();
    }

    // After averaging 4 samples (sampleAverage 4), update SpO2 every second.
    maxim_heart_rate_and_oxygen_saturation(
      irBuffer, bufferLength, redBuffer,
      &spo2, &validSPO2,
      &heartRate, &validHeartRate);

    // Display results to serial monitor.
    Serial.print(F("HR="));
    if (validHeartRate) {
      Serial.print(heartRate);
    } else {
      Serial.print(F("--"));
    }
    Serial.print(F(", SpO2="));
    if (validSPO2) {
      Serial.print(spo2);
    } else {
      Serial.print(F("--"));
    }
    Serial.println();

    // Slow down to ~1 Hz output.
    delay(100);
  }
}
