#include <Arduino.h>
#include <PacketSerial.h>
#include <VL53L1X.h>
#include <Wire.h>
#include <array>

#include "shared_config.h"
#include "stm32_tof/include/config.h"

// State
BoundsData bounds;
BluetoothPayload bluetoothInboundPayload;

// Serial managers
PacketSerial teensySerial;

// Time-Of-Flight sensors
std::array<VL53L1X, TOF_COUNT> tofs;

// ------------------------------ MAIN CODE START ------------------------------
void onTeensyPacket(const byte *buf, size_t size) {
    TOFRXPayload payload;
    // Don't continue if the payload is invalid
    if (size != sizeof(payload)) return;
    memcpy(&payload, buf, sizeof(payload));

    // Send bluetooth outbound payload to HC05
    // TODO: Send data from tofRxPayload.bluetoothOutboundPayload
}

void setup() {
    // Turn on the debug LED
    pinMode(PIN_LED_DEBUG, OUTPUT);
    digitalWrite(PIN_LED_DEBUG, HIGH);

    // Initialise pins
    pinMode(PIN_RING, OUTPUT);
    pinMode(PIN_LEFT_EYE, OUTPUT);
    pinMode(PIN_RIGHT_EYE, OUTPUT);

    Wire.setSDA(PIN_SDA_TOF);
    Wire.setSCL(PIN_SCL_TOF);

    // Initialise serial
    TEENSY_SERIAL.begin(TEENSY_TOF_BAUD_RATE);
    BLUETOOTH_SERIAL.begin(BLUETOOTH_BAUD_RATE);
    while (!BLUETOOTH_SERIAL) delay(10);
#ifdef DEBUG
    DEBUG_SERIAL.begin(DEBUG_BAUD_RATE);
    while (!DEBUG_SERIAL) delay(10);
#endif
    teensySerial.setStream(&TEENSY_SERIAL);
    teensySerial.setPacketHandler(&onTeensyPacket);

    // Initialise I2C
    Wire.begin();
    Wire.setClock(400000); // Use 400 kHz I2C

    // Disable all TOFs
    for (uint8_t i = 0; i < TOF_COUNT; ++i) {
        // Drive the XSHUT low to disable the sensor
        pinMode(TOF_CONFIGS[i].xshutPin, OUTPUT);
        digitalWrite(TOF_CONFIGS[i].xshutPin, LOW);
    }
    // Enable and initialise each sensor, one by one
    for (uint8_t i = 0; i < TOF_COUNT; ++i) {
        // Allow XSHUT to be pulled HIGH (don't drive it, not level shifted)
        pinMode(TOF_CONFIGS[i].xshutPin, INPUT);
        delay(10);

        tofs[i].setTimeout(500);
        if (!tofs[i].init()) { // NOTE: This fails if the sensor is already
                               // initialised
#ifdef DEBUG
            DEBUG_SERIAL.printf("Failed to detect and initialize sensor %d", i);
#endif
            while (1) {}
        }

        tofs[i].setAddress(I2C_ADDRESS_TOF_START + i);
        tofs[i].setDistanceMode(TOF_CONFIGS[i].distanceMode);
        tofs[i].setMeasurementTimingBudget(TOF_CONFIGS[i].measurementPeriod);
        tofs[i].startContinuous(TOF_CONFIGS[i].intermeasurementPeriod);
        tofs[i].setROICenter(199); // 199 is the optical center
        tofs[i].setROISize(4, 4);  // FOV ranges from 4 to 16
    }

    // Turn off the debug LED
    digitalWrite(PIN_LED_DEBUG, LOW);
}

void loop() {
    // Read TOF ranges
    for (uint8_t i = 0; i < TOF_COUNT; i++) {
        tofs[i].read();
        if (tofs[i].ranging_data.range_status == VL53L1X::RangeValid ||
            tofs[i].ranging_data.range_status == VL53L1X::SignalFail)
            // If the range is valid we use it
            // We consider the range valid even if the signal value is below the
            // minimum defined threshold to let us to use smaller timing budgets
            bounds.set(i, tofs[i].ranging_data.range_mm);
        else if (tofs[i].ranging_data.range_status == VL53L1X::None)
            // If there is no update, we set the new value as NO_BOUNDS
            bounds.set(i);
    }

    // Read bluetooth inbound payload from HC05
    // TODO: Read data into &bluetoothInboundPayload

    // Send data over serial to Teensy
    byte buf[sizeof(TOFTXPayload)];
    memcpy(buf, &bounds, sizeof(bounds));
    memcpy(buf + sizeof(bounds), &bluetoothInboundPayload,
           sizeof(bluetoothInboundPayload));
    teensySerial.send(buf, sizeof(buf));

    bounds.markAsOld();

    // ------------------------------ START DEBUG ------------------------------

    // // Scan for I2C devices
    // pinMode(PIN_XSHUT_FRONT, INPUT);
    // pinMode(PIN_XSHUT_BACK, INPUT);
    // pinMode(PIN_XSHUT_LEFT, INPUT);
    // pinMode(PIN_XSHUT_RIGHT, INPUT);
    // scanI2C(TEENSY_SERIAL, Wire);
    // delay(5000);

    // // Print payload
    // TEENSY_SERIAL.printf("F: %4d B: %4d L: %4d R: %4d\n", bounds.front,
    //                      bounds.back, bounds.left, bounds.right);

    // // Print loop time
    // printLoopTime();

    // ------------------------------- END DEBUG -------------------------------
}
// ------------------------------- MAIN CODE END -------------------------------
