/* -------------------------------------------------
IMPORTANT!
This code works only till ESP32 core (boards) version 3.2.0.
Downgrade if you have compilation errors.

Copyright (c)
Arduino project by Tech Talkies YouTube Channel.
https://www.youtube.com/@techtalkies1
-------------------------------------------------*/

#include <BleMouse.h>
#include <Wire.h>
#include <math.h>

#define LEFTBUTTON 18
#define RIGHTBUTTON 19
#define SPEED 30
#define DEBUG_INTERVAL_MS 250
#define GYRO_DEADZONE 0.05f
#define STILL_RATE_THRESHOLD 0.08f
#define BIAS_TRACK_ALPHA 0.005f
#define GYRO_CALIBRATION_SAMPLES 200
#define BUTTON_DEBOUNCE_MS 30

BleMouse bleMouse;

uint8_t mpuAddress = 0x68;
float gyroBiasX = 0.0f;
float gyroBiasZ = 0.0f;
unsigned long lastDebugMillis;
bool wasConnected;
bool leftLastReading = HIGH;
bool rightLastReading = HIGH;
bool leftStableState = HIGH;
bool rightStableState = HIGH;
unsigned long leftLastChangeMillis;
unsigned long rightLastChangeMillis;

bool probeAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

int16_t readWord() {
  uint8_t highByte = Wire.read();
  uint8_t lowByte = Wire.read();
  return (int16_t)(((uint16_t)highByte << 8) | lowByte);
}

bool readGyroscope(int16_t &gyroX, int16_t &gyroZ) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B); // Start at ACCEL_XOUT_H; gyro registers follow accel and temperature
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)mpuAddress, 14) != 14) {
    return false;
  }
  readWord(); // accel X
  readWord(); // accel Y
  readWord(); // accel Z
  readWord(); // temperature
  gyroX = readWord();
  readWord(); // gyro Y
  gyroZ = readWord();
  return true;
}

void handleButton(uint8_t pin, bool &lastReading, bool &stableState,
                  unsigned long &lastChangeMillis, uint8_t mouseButton,
                  const char *buttonName) {
  const bool reading = digitalRead(pin);
  const unsigned long now = millis();

  if (reading != lastReading) {
    lastReading = reading;
    lastChangeMillis = now;
  }

  if ((now - lastChangeMillis) >= BUTTON_DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {
      Serial.printf("%s click\n", buttonName);
      bleMouse.click(mouseButton);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // Allow the Serial Monitor to attach after reset
  Serial.println("Air Mouse booting");
  Serial.flush();

  pinMode(LEFTBUTTON, INPUT_PULLUP);
  pinMode(RIGHTBUTTON, INPUT_PULLUP);

  // Initialize the I2C sensor before starting Bluetooth.
  delay(100);
  Serial.println("Starting I2C: SDA=21, SCL=22");
  Serial.flush();
  Wire.begin(21, 22);  // ESP32 Dev Module: SDA=GPIO21, SCL=GPIO22
  Wire.setClock(100000);
  Serial.println("Checking MPU6050 at 0x68, then 0x69");
  Serial.flush();
  if (!probeAddress(0x68)) {
    if (probeAddress(0x69)) {
      mpuAddress = 0x69;
    } else {
      Serial.println("No I2C device answered at 0x68 or 0x69");
      while (1) delay(10);
    }
  }
  Serial.printf("I2C device answered at 0x%02X\n", mpuAddress);
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x75); // WHO_AM_I
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)mpuAddress, 1) == 1) {
    Serial.printf("WHO_AM_I = 0x%02X\n", Wire.read());
  } else {
    Serial.println("Could not read WHO_AM_I; continuing with raw sensor access");
  }
  if (!writeRegister(0x6B, 0x00)) { // Wake sensor
    Serial.println("Could not wake MPU6050");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 I2C initialized and awake");

  Serial.println("Keep the mouse still: calibrating gyro...");
  int32_t biasSumX = 0;
  int32_t biasSumZ = 0;
  uint16_t validSamples = 0;
  for (uint16_t i = 0; i < GYRO_CALIBRATION_SAMPLES; ++i) {
    int16_t rawX, rawZ;
    if (readGyroscope(rawX, rawZ)) {
      biasSumX += rawX;
      biasSumZ += rawZ;
      ++validSamples;
    }
    delay(5);
  }
  if (validSamples > 0) {
    gyroBiasX = ((biasSumX / (float)validSamples) / 131.0f) * (PI / 180.0f);
    gyroBiasZ = ((biasSumZ / (float)validSamples) / 131.0f) * (PI / 180.0f);
    Serial.printf("Gyro bias calibrated: X %.3f, Z %.3f rad/s\n", gyroBiasX, gyroBiasZ);
  } else {
    Serial.println("Gyro calibration failed; check I2C wiring");
  }

  bleMouse.begin();
  Serial.println("BLE mouse starting; pair with ESP32 Bluetooth Mouse.");

}

void loop() {
  if (bleMouse.isConnected()) {
    if (!wasConnected) {
      Serial.println("BLE mouse connected.");
      wasConnected = true;
    }
    int16_t rawGyroX, rawGyroZ;
    if (!readGyroscope(rawGyroX, rawGyroZ)) {
      Serial.println("MPU6050 I2C read failed");
      delay(100);
      return;
    }
    // Convert raw gyro readings (131 LSB per degree/second) to radians/second.
    float gyroX = (rawGyroX / 131.0f) * (PI / 180.0f) - gyroBiasX;
    float gyroZ = (rawGyroZ / 131.0f) * (PI / 180.0f) - gyroBiasZ;

    // Treat small rates on both axes as the mouse being still. Follow slow
    // zero-rate drift and suppress those readings so the pointer stays put.
    if (fabsf(gyroX) < STILL_RATE_THRESHOLD && fabsf(gyroZ) < STILL_RATE_THRESHOLD) {
      gyroBiasX += gyroX * BIAS_TRACK_ALPHA;
      gyroBiasZ += gyroZ * BIAS_TRACK_ALPHA;
      gyroX = 0.0f;
      gyroZ = 0.0f;
    } else {
      if (fabsf(gyroX) < GYRO_DEADZONE) gyroX = 0.0f;
      if (fabsf(gyroZ) < GYRO_DEADZONE) gyroZ = 0.0f;
    }

    // Positive axis mapping corrects the reversed pointer direction in the
    // original sketch. Adjust signs here only if the sensor is mounted flipped.
    int dx = constrain((int)lroundf(gyroZ * SPEED), -127, 127);
    int dy = constrain((int)lroundf(gyroX * SPEED), -127, 127);
    bleMouse.move((signed char)dx, (signed char)dy);

    if (millis() - lastDebugMillis >= DEBUG_INTERVAL_MS) {
      Serial.printf("gyro X: %.3f  Z: %.3f rad/s | mouse dx: %d dy: %d\n",
                    gyroX, gyroZ, dx, dy);
      lastDebugMillis = millis();
    }

    handleButton(LEFTBUTTON, leftLastReading, leftStableState,
                 leftLastChangeMillis, MOUSE_LEFT, "Left");
    handleButton(RIGHTBUTTON, rightLastReading, rightStableState,
                 rightLastChangeMillis, MOUSE_RIGHT, "Right");
    // Limit HID notifications to about 50 per second to keep the BLE link stable.
    delay(20);
  } else if (wasConnected) {
    Serial.println("BLE mouse disconnected.");
    wasConnected = false;
  }
}
