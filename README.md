# ESP32 Air Mouse

BLE air mouse for an ESP32-WROOM-32 Dev Module and an MPU6050-compatible I2C gyroscope. The firmware reads the sensor directly over I2C, calibrates gyro bias at startup, and limits BLE reports to 50 per second.

## Wiring

### MPU6050 module

| MPU6050 | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Click buttons

Connect a button between GPIO 19 and GND for left click, and another between GPIO 18 and GND for right click. The firmware uses internal pull-ups.

### Battery power

For a single-cell 3.7 V LiPo, use a TP4056 charger/protection module and a regulated 5 V boost converter:

```text
Battery +/−           → TP4056 B+/B−
TP4056 OUT+/OUT−       → boost IN+/IN−
Boost OUT+ (set to 5V) → ESP32 5V/VIN
Boost OUT−             → ESP32 GND
```

Set the boost converter to 5.0 V with a multimeter before connecting it. Disconnect USB while battery-powered; use only one power source at a time. Use a protected battery/module. Do not charge while the project is running unless the circuit supports power-path/load-sharing. TP4056 boards vary; if yours has no OUT terminals, use an appropriate protection board/module.

## Build and upload

1. Install **Arduino-ESP32 core 3.2.0** and select **ESP32 Dev Module**. The T-vK BLE Mouse library used here is incompatible with core 4.x.
2. Install **ESP32 BLE Mouse v0.3.1** from [T-vK's official repository/releases](https://github.com/T-vK/ESP32-BLE-Mouse). This dependency is intentionally not bundled here.
3. Open `ESP32-AirMouse.ino` in Arduino IDE and upload.
4. Open Serial Monitor at **115200 baud**. Keep the device still for about one second after reset while gyro bias is calibrated.
5. Pair **ESP32 Bluetooth Mouse**. Rotate your wrist to move the pointer; straight-line movement without rotation does not move the cursor.

To restore advertising after a disconnect, apply [`patches/BleConnectionStatus-reconnect.patch`](patches/BleConnectionStatus-reconnect.patch) to the installed library's `BleConnectionStatus.cpp`.

## Troubleshooting

- `No sensor answered...`: verify the I2C wires and the pins above.
- `BLE mouse advertising...` but no pairing: forget any old pairing on the computer and scan again.
- Cursor drift: reset while the mouse is completely still so the firmware can calibrate.
- Serial diagnostics use 115200 baud.

## Attribution

This is an independent implementation of the air-mouse behavior demonstrated by Tech Talkies. BLE HID support is provided by the separately installed [ESP32 BLE Mouse library](https://github.com/T-vK/ESP32-BLE-Mouse).
