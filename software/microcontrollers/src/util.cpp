#include "util.h"

#include <EEPROM.h>
#include <Wire.h>

// Scans the I2C bus for devices.
void scanI2C(Stream &serial, TwoWire wire) {
    // Adapted from
    // https://github.com/stm32duino/Arduino_Core_STM32/blob/main/libraries/Wire/examples/i2c_scanner/i2c_scanner.ino
    byte error, address;
    uint8_t deviceCount = 0;

    serial.println("Scanning...");

    for (address = 1; address < 127; address++) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.

        wire.beginTransmission(address);
        error = wire.endTransmission();

        if (error == 0) {
            serial.printf("I2C device found at address 0x%02x\n", address);
            ++deviceCount;
        } else if (error == 4) {
            serial.printf("Unknown error at address 0x%02x\n", address);
        }
    }

    if (deviceCount == 0)
        serial.println("No I2C devices found\n");
    else
        serial.printf("Found %d I2C devices\n", deviceCount);
}

// Wipes the EEPROM.
void wipeEEPROM() {
    Serial.println("Wiping EEPROM...");
    for (int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0);
        Serial.printf("%4d / %d\n", i, EEPROM.length());
    }
    Serial.println("Done");
}

uint32_t _time = 0;
uint32_t _min_delta = UINT32_MAX;
uint32_t _max_delta = 0;
uint64_t _sum_delta = 0;
uint16_t _iter = 0;

// Get loop time in µs.
uint32_t printLoopTime(uint16_t sampleCount) {
    const auto now = micros();
    const auto delta = micros() - _time;
    _time = now;
    _min_delta = min(_min_delta, delta);
    _max_delta = max(_max_delta, delta);
    _sum_delta += delta;
    ++_iter;

    if (_iter == sampleCount) {
        Serial.printf(
            "Loop time (µs): mean=%5u (min=%5u, max=%5u), samples=%4u\n",
            (uint32_t)roundf((float)_sum_delta / (float)_iter), _min_delta,
            _max_delta, _iter);
        _time = micros(); // Update time as Serial.print() took some time
        _min_delta = UINT32_MAX;
        _max_delta = 0;
        _sum_delta = 0;
        _iter = 0;
    }

    return delta;
}
