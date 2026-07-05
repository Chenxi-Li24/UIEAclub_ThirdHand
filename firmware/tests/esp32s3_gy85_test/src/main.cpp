#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t PIN_SDA = 11;
constexpr uint8_t PIN_SCL = 10;

constexpr uint8_t ADXL345_ADDR = 0x53;
constexpr uint8_t ITG3205_ADDR = 0x68;
constexpr uint8_t HMC5883L_ADDR = 0x1E;
constexpr uint8_t QMC5883L_ADDR = 0x0D;

bool adxlOk = false;
bool itgOk = false;
bool hmcOk = false;
bool qmcOk = false;

bool devicePresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

bool readRegisters(uint8_t address, uint8_t reg, uint8_t *data, size_t length) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(address, static_cast<uint8_t>(length)) != length) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        data[i] = Wire.read();
    }
    return true;
}

uint8_t readRegister(uint8_t address, uint8_t reg) {
    uint8_t value = 0xFF;
    readRegisters(address, reg, &value, 1);
    return value;
}

void scanI2C() {
    Serial.println("I2C scan start");
    int count = 0;
    for (uint8_t address = 1; address < 127; ++address) {
        if (devicePresent(address)) {
            Serial.printf("  found: 0x%02X\n", address);
            ++count;
        }
    }
    Serial.printf("I2C device count: %d\n", count);
}

void initADXL345() {
    if (!devicePresent(ADXL345_ADDR)) {
        Serial.println("ADXL345: NOT FOUND at 0x53");
        return;
    }

    const uint8_t deviceId = readRegister(ADXL345_ADDR, 0x00);
    Serial.printf("ADXL345: DEVID=0x%02X (expected 0xE5)\n", deviceId);
    if (deviceId != 0xE5) {
        return;
    }

    writeRegister(ADXL345_ADDR, 0x2D, 0x00);
    writeRegister(ADXL345_ADDR, 0x31, 0x08);  // Full resolution, +/-2 g.
    writeRegister(ADXL345_ADDR, 0x2C, 0x0A);  // 100 Hz output data rate.
    writeRegister(ADXL345_ADDR, 0x2D, 0x08);  // Measurement mode.
    adxlOk = true;
    Serial.println("ADXL345: OK");
}

void initITG3205() {
    if (!devicePresent(ITG3205_ADDR)) {
        Serial.println("ITG3205: NOT FOUND at 0x68");
        return;
    }

    const uint8_t whoAmI = readRegister(ITG3205_ADDR, 0x00);
    Serial.printf("ITG3205: WHO_AM_I=0x%02X\n", whoAmI);

    writeRegister(ITG3205_ADDR, 0x3E, 0x80);  // Reset.
    delay(100);
    writeRegister(ITG3205_ADDR, 0x3E, 0x00);  // Internal oscillator.
    writeRegister(ITG3205_ADDR, 0x15, 0x09);  // Sample-rate divider.
    writeRegister(ITG3205_ADDR, 0x16, 0x1B);  // Full scale, 42 Hz LPF.
    itgOk = true;
    Serial.println("ITG3205: OK");
}

void initMagnetometer() {
    if (devicePresent(HMC5883L_ADDR)) {
        uint8_t id[3] = {};
        readRegisters(HMC5883L_ADDR, 0x0A, id, 3);
        Serial.printf("HMC5883L: ID=%02X %02X %02X\n", id[0], id[1], id[2]);
        writeRegister(HMC5883L_ADDR, 0x00, 0x70);  // 8 samples, 15 Hz.
        writeRegister(HMC5883L_ADDR, 0x01, 0x20);  // +/-1.3 gauss.
        writeRegister(HMC5883L_ADDR, 0x02, 0x00);  // Continuous mode.
        hmcOk = true;
        Serial.println("HMC5883L: OK");
        return;
    }

    if (devicePresent(QMC5883L_ADDR)) {
        Serial.println("Magnetometer clone detected at 0x0D (QMC5883L compatible)");
        writeRegister(QMC5883L_ADDR, 0x0A, 0x80);  // Soft reset.
        delay(20);
        writeRegister(QMC5883L_ADDR, 0x0B, 0x01);  // Set/reset period.
        writeRegister(QMC5883L_ADDR, 0x09, 0x1D);  // Continuous, 200 Hz, 8 G.
        qmcOk = true;
        Serial.println("QMC5883L: OK");
        return;
    }

    Serial.println("Magnetometer: NOT FOUND at 0x1E or 0x0D");
}

void printADXL345() {
    uint8_t data[6];
    if (!adxlOk || !readRegisters(ADXL345_ADDR, 0x32, data, sizeof(data))) {
        return;
    }

    const int16_t x = static_cast<int16_t>((data[1] << 8) | data[0]);
    const int16_t y = static_cast<int16_t>((data[3] << 8) | data[2]);
    const int16_t z = static_cast<int16_t>((data[5] << 8) | data[4]);
    Serial.printf("ACC[g]   X:%7.3f Y:%7.3f Z:%7.3f  ", x * 0.0039f, y * 0.0039f, z * 0.0039f);
}

void printITG3205() {
    uint8_t data[6];
    if (!itgOk || !readRegisters(ITG3205_ADDR, 0x1D, data, sizeof(data))) {
        return;
    }

    const int16_t x = static_cast<int16_t>((data[0] << 8) | data[1]);
    const int16_t y = static_cast<int16_t>((data[2] << 8) | data[3]);
    const int16_t z = static_cast<int16_t>((data[4] << 8) | data[5]);
    Serial.printf("GYRO[dps] X:%7.2f Y:%7.2f Z:%7.2f  ", x / 14.375f, y / 14.375f, z / 14.375f);
}

void printMagnetometer() {
    uint8_t data[6];
    if (hmcOk && readRegisters(HMC5883L_ADDR, 0x03, data, sizeof(data))) {
        const int16_t x = static_cast<int16_t>((data[0] << 8) | data[1]);
        const int16_t z = static_cast<int16_t>((data[2] << 8) | data[3]);
        const int16_t y = static_cast<int16_t>((data[4] << 8) | data[5]);
        Serial.printf("MAG[raw] X:%6d Y:%6d Z:%6d", x, y, z);
    } else if (qmcOk && readRegisters(QMC5883L_ADDR, 0x00, data, sizeof(data))) {
        const int16_t x = static_cast<int16_t>((data[1] << 8) | data[0]);
        const int16_t y = static_cast<int16_t>((data[3] << 8) | data[2]);
        const int16_t z = static_cast<int16_t>((data[5] << 8) | data[4]);
        Serial.printf("MAG[raw] X:%6d Y:%6d Z:%6d", x, y, z);
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println();
    Serial.println("ESP32-S3 GY-85 Test Start");
    Serial.println("I2C pins: SDA=GPIO11, SCL=GPIO10");

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);
    delay(100);

    scanI2C();
    initADXL345();
    initITG3205();
    initMagnetometer();

    Serial.println("Move and rotate the GY-85; sensor values should change.");
}

void loop() {
    printADXL345();
    printITG3205();
    printMagnetometer();
    Serial.println();
    delay(500);
}
