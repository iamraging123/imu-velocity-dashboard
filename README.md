# IMU Velocity Dashboard — ICM-42670-P

Demo-grade velocity estimation from an ICM-42670-P accelerometer, streamed
over serial and plotted live in the browser. **Not a precision speedometer.**
Integration drift is fundamental; ZUPT + high-pass mitigate but do not
eliminate it.

---

## Wiring

| ICM-42670-P | MCU pin |
|-------------|---------|
| SDA         | PB7     |
| SCL         | PB6     |
| VCC         | 3.3 V   |
| GND         | GND     |

Pull-ups (4.7 kΩ to 3.3 V on SDA/SCL) are required if your breakout board
does not include them. The firmware uses 400 kHz fast-mode I²C.

If your board uses different I²C pins, edit the two `#define` lines near
the top of `imu_velocity.ino`:

```cpp
#define PIN_I2C_SDA PB7
#define PIN_I2C_SCL PB6
```

---

## Required Libraries

| Library  | Where |
|----------|-------|
| Wire     | Built into the Arduino IDE (AVR, STM32, RP2040, etc.) |

No third-party IMU library is needed — the firmware bit-bangs the ICM-42670-P
register map directly.

---

## Baud Rate

**115200** (set in both `Serial.begin(115200)` and the dashboard baud field).

---

## Flashing

1. Open `imu_velocity.ino` in the Arduino IDE (2.x recommended).
2. Select your board and port.
3. Upload. Open the Serial Monitor to confirm calibration output:

```
# ICM-42670-P at 0x68  ODR=100Hz  FS=8g
# Calibrating — keep sensor stationary...
# Cal offsets (LSB): ax=... ay=... az=...
# timestamp_ms,vx,vy,vz,speed
0,0.0000,0.0000,0.0000,0.0000
10,0.0012,...
```

Keep the sensor perfectly still during the 2-second calibration window.

---

## Opening the Dashboard

1. Open `index.html` directly from your filesystem in Chrome or Edge
   (File → Open File, or drag it to the address bar).
   — Web Serial works on `file://` in recent Chrome/Edge builds.
   — If it doesn't, serve it locally: `python -m http.server 8080`
     then navigate to `http://localhost:8080`.

2. Click **Connect**, select the Arduino's serial port, confirm 115200 baud.

3. Move the sensor — velocity traces appear on the chart.

4. Click **Download CSV** to save the full session.

---

## Drift Limitations

Velocity from double-integrating accelerometer data is **inherently drift-prone**:

- **Bias drift**: any residual DC offset after calibration accumulates
  linearly with time.
- **Noise integration**: random noise integrates into a random walk.
- **Tilt coupling**: gravity leaks into horizontal axes when the sensor tilts.

Two mitigations are applied in the firmware:

| Technique | What it does | Tuning knob |
|-----------|-------------|-------------|
| **ZUPT** (zero-velocity update) | Snaps velocity to 0 after 20 consecutive "quiet" samples | `ZUPT_WINDOW`, `ZUPT_THRESHOLD` |
| **High-pass decay** | Exponentially decays velocity each sample (`v *= 0.995`) | `HP_ALPHA` |

These help for short, distinct motions with rest periods between them.
Continuous motion (walking, driving) will still drift significantly.

For a production speedometer you would need a sensor fusion algorithm
(Madgwick/Mahony + GPS or wheel odometry), which is well beyond this demo.
