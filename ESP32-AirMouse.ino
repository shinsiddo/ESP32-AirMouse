#include <BleMouse.h>
#include <Wire.h>
#include <math.h>

// ESP32-WROOM-32 Dev Module pin assignments
constexpr uint8_t I2C_SDA = 21;
constexpr uint8_t I2C_SCL = 22;
constexpr uint8_t LEFT_BUTTON = 19;
constexpr uint8_t RIGHT_BUTTON = 18;
constexpr uint8_t MPU_ADDRESS_PRIMARY = 0x68;
constexpr uint8_t MPU_ADDRESS_ALT = 0x69;
constexpr uint8_t GYRO_REGISTER_START = 0x3B;
constexpr uint8_t POWER_REGISTER = 0x6B;
constexpr float GYRO_SCALE = 30.0f;
constexpr float GYRO_DEADZONE_RAD_S = 0.02f;
constexpr uint16_t CALIBRATION_SAMPLE_COUNT = 200;
constexpr uint16_t REPORT_INTERVAL_MS = 20;
constexpr uint16_t DEBUG_INTERVAL_MS = 250;

BleMouse bleMouse;
uint8_t mpuAddress = MPU_ADDRESS_PRIMARY;
float gyroBiasX = 0.0f;
float gyroBiasZ = 0.0f;
unsigned long lastDebugAt = 0;
bool previousLeftState = HIGH;
bool previousRightState = HIGH;
bool previouslyConnected = false;

bool deviceResponds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool writeMpuRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

int16_t readSignedWord() {
  const uint8_t highByte = Wire.read();
  const uint8_t lowByte = Wire.read();
  return (int16_t)(((uint16_t)highByte << 8) | lowByte);
}

bool readGyroscope(int16_t &x, int16_t &z) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(GYRO_REGISTER_START);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)mpuAddress, 14) != 14) return false;

  readSignedWord(); // acceleration X
  readSignedWord(); // acceleration Y
  readSignedWord(); // acceleration Z
  readSignedWord(); // temperature
  x = readSignedWord();
  readSignedWord(); // gyro Y
  z = readSignedWord();
  return true;
}

void calibrateGyroscope() {
  Serial.println("Keep still: calibrating gyroscope...");
  int32_t sumX = 0;
  int32_t sumZ = 0;
  uint16_t samples = 0;

  for (uint16_t i = 0; i < CALIBRATION_SAMPLE_COUNT; ++i) {
    int16_t rawX, rawZ;
    if (readGyroscope(rawX, rawZ)) {
      sumX += rawX;
      sumZ += rawZ;
      ++samples;
    }
    delay(5);
  }

  if (samples == 0) {
    Serial.println("Gyroscope calibration failed; check I2C wiring.");
    return;
  }

  const float degreesToRadians = PI / 180.0f;
  gyroBiasX = (sumX / (float)samples) / 131.0f * degreesToRadians;
  gyroBiasZ = (sumZ / (float)samples) / 131.0f * degreesToRadians;
  Serial.printf("Gyro bias: X %.3f, Z %.3f rad/s\n", gyroBiasX, gyroBiasZ);
}

void clickButtonOnPress(uint8_t pin, bool &previousState, uint8_t button) {
  const bool currentState = digitalRead(pin);
  if (previousState == HIGH && currentState == LOW) {
    delay(20);
    if (digitalRead(pin) == LOW) bleMouse.click(button);
  }
  previousState = currentState;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Air Mouse booting");

  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  if (!deviceResponds(MPU_ADDRESS_PRIMARY)) {
    if (deviceResponds(MPU_ADDRESS_ALT)) {
      mpuAddress = MPU_ADDRESS_ALT;
    } else {
      Serial.println("No sensor answered at 0x68/0x69 on GPIO 21/22.");
      while (true) delay(100);
    }
  }

  Serial.printf("I2C device at 0x%02X\n", mpuAddress);
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x75); // WHO_AM_I
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)mpuAddress, 1) == 1) {
    Serial.printf("WHO_AM_I = 0x%02X\n", Wire.read());
  }
  if (!writeMpuRegister(POWER_REGISTER, 0x00)) {
    Serial.println("Could not wake sensor.");
    while (true) delay(100);
  }

  calibrateGyroscope();
  bleMouse.begin();
  Serial.println("BLE mouse advertising as ESP32 Bluetooth Mouse");
}

void loop() {
  if (!bleMouse.isConnected()) {
    if (previouslyConnected) Serial.println("BLE mouse disconnected; advertising again.");
    previouslyConnected = false;
    delay(100);
    return;
  }

  if (!previouslyConnected) {
    Serial.println("BLE mouse connected.");
    previouslyConnected = true;
  }

  int16_t rawX, rawZ;
  if (readGyroscope(rawX, rawZ)) {
    const float degreesToRadians = PI / 180.0f;
    float gyroX = (rawX / 131.0f) * degreesToRadians - gyroBiasX;
    float gyroZ = (rawZ / 131.0f) * degreesToRadians - gyroBiasZ;
    if (fabsf(gyroX) < GYRO_DEADZONE_RAD_S) gyroX = 0.0f;
    if (fabsf(gyroZ) < GYRO_DEADZONE_RAD_S) gyroZ = 0.0f;

    const int dx = constrain((int)lroundf(-gyroZ * GYRO_SCALE), -127, 127);
    const int dy = constrain((int)lroundf(-gyroX * GYRO_SCALE), -127, 127);
    bleMouse.move((signed char)dx, (signed char)dy);

    if (millis() - lastDebugAt >= DEBUG_INTERVAL_MS) {
      Serial.printf("gyro X %.3f Z %.3f rad/s | dx %d dy %d\n", gyroX, gyroZ, dx, dy);
      lastDebugAt = millis();
    }
  }

  clickButtonOnPress(LEFT_BUTTON, previousLeftState, MOUSE_LEFT);
  clickButtonOnPress(RIGHT_BUTTON, previousRightState, MOUSE_RIGHT);
  delay(REPORT_INTERVAL_MS);
}
