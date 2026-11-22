# ESP32 BLE Air Mouse

A BLE HID air mouse built with an ESP32-WROOM-32 Dev Module and an MPU6050-compatible sensor. Wrist rotation controls the pointer. Two momentary pushbuttons provide left and right clicks.

## Features

- Reads the MPU6050 gyroscope directly over I2C; no Adafruit MPU6050 library is required.
- Calibrates gyro bias on startup and slowly tracks zero-rate bias while the mouse is still to reduce cursor drift.
- Uses a dead zone to ignore small residual sensor noise.
- Sends BLE mouse movement at a nominal 50 reports per second.
- Left-click button on GPIO 18 and right-click button on GPIO 19.
- Button inputs use internal pull-ups and a 30 ms debounce interval.

## Hardware components

### Required

- ESP32-WROOM-32 development board (classic ESP32, selected as `ESP32 Dev Module` in Arduino IDE)
- MPU6050-compatible I2C accelerometer/gyroscope breakout module
- Two normally-open momentary pushbuttons
- Jumper wires and a breadboard or equivalent wiring
- USB data cable for programming and initial power
- Computer or phone that supports Bluetooth Low Energy HID mouse devices

### Optional battery power

- Protected single-cell 3.7 V LiPo battery
- TP4056 charger/protection module with protected output terminals
- Regulated 5 V boost converter
- Multimeter for checking the boost output before connecting it

USB power is sufficient for initial setup and testing. The battery parts are not required to upload or operate the firmware over USB.

## Connections

Power off the ESP32 while wiring. Connect all grounds together.

### MPU6050 sensor

| MPU6050 pin | ESP32-WROOM-32 pin | Purpose |
|---|---|---|
| VCC | 3V3 | Sensor supply |
| GND | GND | Common ground |
| SDA | GPIO 21 | I2C data |
| SCL | GPIO 22 | I2C clock |

Use 3.3 V for the sensor supply unless the breakout board documentation specifies its supported input range. The firmware probes I2C address `0x68`, then `0x69` (the address changes with the module's AD0 setting).

### Pushbuttons

| Button | First terminal | Second terminal |
|---|---|---|
| Left click | GPIO 18 | GND |
| Right click | GPIO 19 | GND |

Use the two terminals of a normally-open pushbutton. There is no polarity. Do not connect the button to 3V3: the sketch configures each input as `INPUT_PULLUP`, so pressing the button connects its GPIO to GND.

### Optional LiPo power

Use this only after the USB-powered build works:

```text
LiPo + / -                -> TP4056 B+ / B-
TP4056 OUT+ / OUT-        -> boost converter IN+ / IN-
Boost OUT+ (set to 5.0 V) -> ESP32 5V/VIN
Boost OUT-                -> ESP32 GND
```

Verify the boost converter output is 5.0 V with a multimeter before connecting it to the board. Use a protected battery and a charger/protection board with suitable output terminals; TP4056 modules vary. Use USB or battery power, not both at once. Do not charge the battery while the project is running unless the circuit supports power-path/load-sharing.

## Software requirements

- Arduino IDE 2.x
- Espressif Arduino-ESP32 boards package **3.2.0**. The sketch header notes that this project works through core 3.2.0; use that version if a newer core causes compilation errors.
- **ESP32 BLE Mouse v0.3.1** by T-vK: [official releases](https://github.com/T-vK/ESP32-BLE-Mouse/releases)
- The Arduino `Wire` library is included with the ESP32 core. No MPU6050-specific Arduino library is used because this firmware reads the sensor registers directly.

## Install the firmware

1. Install Arduino IDE and open **Boards Manager** from the Tools > Board menu.
2. Search for `esp32` and install Espressif's Arduino-ESP32 package version 3.2.0. Restart the IDE if prompted.
3. In Boards Manager, select Tools > Board > ESP32 Arduino > **ESP32 Dev Module**.
4. Install the BLE mouse library: download the v0.3.1 ZIP from the [T-vK releases page](https://github.com/T-vK/ESP32-BLE-Mouse/releases), then choose Sketch > Include Library > Add .ZIP Library and select that ZIP.
5. Download or clone this repository and open `ESP32-AirMouse.ino` in Arduino IDE.
6. Wire the MPU6050 and buttons as shown above. Connect the ESP32 to the computer with a USB data cable.
7. Select the ESP32's serial port under Tools > Port. Click Verify to compile, then click Upload. If upload waits for a connection, hold the board's BOOT button while upload starts and release it when writing begins.
8. Open Tools > Serial Monitor and set the baud rate to **115200**.
9. Reset the ESP32 and keep the mouse still for about one second while the gyro bias is calibrated. Wait for the BLE advertising message.
10. On the host, open Bluetooth settings and pair with **ESP32 Bluetooth Mouse**. If it was paired before, remove/forget the old pairing first and pair again.

## Use the mouse

- After pairing, rotate your wrist to move the pointer. Straight-line translation without rotation does not move it.
- Press the button wired to GPIO 18 for a left click; press the GPIO 19 button for a right click. Click events are sent only while BLE is connected.
- Keep the device still during startup calibration. The firmware averages 200 samples taken 5 ms apart.
- The current pointer scale is `30`. The motion axes are mapped to match the corrected direction for the intended sensor orientation. If your sensor is mounted differently, adjust the signs in the `dx` and `dy` calculations.
- Stationary-rate threshold (`STILL_RATE_THRESHOLD`), residual dead zone (`GYRO_DEADZONE`), and sensitivity (`SPEED`) are constants near the top of the sketch. Raising the stillness threshold suppresses more drift but can ignore slower wrist movement; lower it in small steps if the mouse feels unresponsive.

## Troubleshooting

| Symptom | Checks |
|---|---|
| `No I2C device answered at 0x68 or 0x69` | Check sensor 3V3/GND, SDA on GPIO 21, SCL on GPIO 22, and the module's AD0/address setting. |
| `Gyro calibration failed` | Check the I2C wiring and sensor power, then reset while the device is still. |
| Cursor moves while the device is still | Reset on a stable surface during calibration. Check that the sensor is firmly mounted and wires are not loose. If small drift remains, raise `STILL_RATE_THRESHOLD` slightly. |
| Pointer direction is reversed | Confirm the sensor orientation. If it is mounted differently, reverse the sign of the corresponding `dx` or `dy` expression. |
| A button does not click | Verify the left switch connects GPIO 18 to GND and the right switch connects GPIO 19 to GND when pressed. Pair the BLE mouse and check Serial Monitor at 115200 baud. |
| Device does not appear for pairing | Confirm startup reached BLE advertising, remove any stale pairing on the host, then scan again. |
| Port is missing or upload fails | Use a USB data cable, select the correct port, install the board's USB-to-serial driver if needed, and try the BOOT-button procedure. |

## Firmware settings

| Setting | Default |
|---|---:|
| I2C pins | SDA 21, SCL 22 |
| Button pins | Left 18, right 19 |
| Sensor addresses | `0x68`, then `0x69` |
| I2C clock | 100 kHz |
| Startup gyro calibration | 200 samples, 5 ms apart |
| Stillness threshold | 0.08 rad/s |
| Residual dead zone | 0.05 rad/s |
| Bias tracking factor | 0.005 per still sample |
| Pointer scale | 30 |
| BLE report interval | 20 ms (nominally 50 reports/s) |
| Serial Monitor | 115200 baud |

## Attribution

The source project credits Tech Talkies in the sketch header. BLE HID mouse support is provided by the separately installed [ESP32 BLE Mouse library](https://github.com/T-vK/ESP32-BLE-Mouse).
