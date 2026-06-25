/*
 * imu_velocity.ino
 * Reads ICM-42670-P over I2C via the TDK ICM42670P library and streams
 * bias-corrected acceleration over serial for the browser dashboard.
 *
 * Wiring (STM32-style pinout, adjust for your board):
 *   SDA -> PB7  (or board SDA pin)
 *   SCL -> PB6  (or board SCL pin)
 *   VCC -> 3.3V
 *   GND -> GND
 *
 * Serial: 115200 baud, CSV: timestamp_ms,ax,ay,az,a_mag
 */

#include "ICM42670P.h"

/* ---------- I2C ---------- */
#define PIN_I2C_SDA PB7
#define PIN_I2C_SCL PB6

/* ---------- Config ---------- */
#define ODR_HZ    100
#define ACCEL_FSR 8        // ±8 g
#define G_MS2     9.80665f // m/s^2

// Accel sensitivity: ±8 g, 16-bit signed -> 32768/8 = 4096 LSB/g
const float LSB_PER_G = 4096.0f;

/* ---------- Calibration ---------- */
#define CAL_SAMPLES 200    // samples at startup (2 s at 100 Hz)

/* ================================================================ */

// lsb=0 -> I2C address 0x68 (AD0 low); lsb=1 -> 0x69 (AD0 high)
ICM42670 IMU(Wire, 0);

// Calibration offsets (raw LSB)
float cal_ax = 0, cal_ay = 0, cal_az = 0;

unsigned long last_tx_ms = 0;
const unsigned long TX_PERIOD_MS = 10;  // transmit at 100 Hz

/* -------- Setup -------- */

void setup() {
    Serial.begin(115200);
    Serial.println("# Power on");
    delay(3000);  // let USB-CDC enumerate and serial monitor connect

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

    if (IMU.begin() != 0) {
        Serial.println("# ERROR: ICM-42670-P not found");
        while (1) {}
    }

    IMU.startAccel(ODR_HZ, ACCEL_FSR);
    delay(50);  // stabilise after power-on

    Serial.println("# ICM-42670-P  ODR=100Hz  FS=8g");
    Serial.println("# Calibrating — keep sensor stationary...");

    // Average CAL_SAMPLES readings to get static bias offsets
    double sum_ax = 0, sum_ay = 0, sum_az = 0;
    inv_imu_sensor_event_t evt;
    for (int i = 0; i < CAL_SAMPLES; i++) {
        IMU.getDataFromRegisters(evt);
        sum_ax += evt.accel[0];
        sum_ay += evt.accel[1];
        sum_az += evt.accel[2];
        delay(1000 / ODR_HZ);
    }
    cal_ax = (float)(sum_ax / CAL_SAMPLES);
    cal_ay = (float)(sum_ay / CAL_SAMPLES);
    // Preserve gravity on Z: offset only the bias above/below 1 g
    cal_az = (float)(sum_az / CAL_SAMPLES) - LSB_PER_G;

    Serial.print("# Cal offsets (LSB): ax=");
    Serial.print(cal_ax, 1);
    Serial.print(" ay="); Serial.print(cal_ay, 1);
    Serial.print(" az="); Serial.println(cal_az, 1);
    Serial.println("# timestamp_ms,ax,ay,az,a_mag");

    last_tx_ms = millis();
}

/* -------- Main loop -------- */

void loop() {
    inv_imu_sensor_event_t evt;
    IMU.getDataFromRegisters(evt);

    // Convert to m/s^2, subtract bias (Z keeps gravity removed)
    float ax = ((float)evt.accel[0] - cal_ax) / LSB_PER_G * G_MS2;
    float ay = ((float)evt.accel[1] - cal_ay) / LSB_PER_G * G_MS2;
    float az = ((float)evt.accel[2] - cal_az) / LSB_PER_G * G_MS2;

    float a_mag = sqrtf(ax*ax + ay*ay + az*az);

    unsigned long now_ms = millis();
    if ((now_ms - last_tx_ms) >= TX_PERIOD_MS) {
        last_tx_ms = now_ms;
        Serial.print(now_ms);
        Serial.print(',');
        Serial.print(ax, 4);
        Serial.print(',');
        Serial.print(ay, 4);
        Serial.print(',');
        Serial.print(az, 4);
        Serial.print(',');
        Serial.println(a_mag, 4);
    }

    delayMicroseconds(1000000 / ODR_HZ - 200);
}
