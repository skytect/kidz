#include <Arduino.h>

#include "config.h"
#include "stm32_mux/include/config.h"
#include "util.h"

// State
struct Line line;

// Sets an LDR MUX to select a specific channel.
void selectLDRMUXChannel(LDRMUX mux, uint8_t channel) {
    switch (mux) {
    case MUX1:
        digitalWrite(PIN_LDRMUX1_S0, channel & 1);
        digitalWrite(PIN_LDRMUX1_S1, (channel >> 1) & 1);
        digitalWrite(PIN_LDRMUX1_S2, (channel >> 2) & 1);
        digitalWrite(PIN_LDRMUX1_S3, (channel >> 3) & 1);
    case MUX2:
        digitalWrite(PIN_LDRMUX2_S0, channel & 1);
        digitalWrite(PIN_LDRMUX2_S1, (channel >> 1) & 1);
        digitalWrite(PIN_LDRMUX2_S2, (channel >> 2) & 1);
        digitalWrite(PIN_LDRMUX2_S3, (channel >> 3) & 1);
    }
    delayMicroseconds(50); // TODO: Figure out how long of a delay I need
}

// Reads the value of an LDR 0-indexed counting clockwise from 000º.
uint16_t readLDR(uint8_t index) {
    const auto real_index = LDR_MAP_REVERSE[index];
    selectLDRMUXChannel(real_index < 16 ? MUX1 : MUX2, real_index % 16);
    return analogRead(real_index < 16 ? PIN_LDRMUX1_SIG : PIN_LDRMUX2_SIG);
}

// Finds the position of the line.
void findLine() {
    // Get the matches (to the line)
    uint8_t matchCount = 0;
    uint8_t matches[LDR_COUNT];
    for (uint8_t i = 0; i < LDR_COUNT; ++i) {
        if (readLDR(i) > LDR_THRESHOLDS[i]) {
            matches[matchCount] = i;
            ++matchCount;
        }
    }

    // No match (to the line) was found
    if (matchCount == 0) {
        line.bearing = UINT16_NO_LINE;
        line.size = UINT8_NO_LINE;
        return;
    }

    // Find the cluster
    float maxAngleDifference = 0;
    uint8_t clusterStart = LDR_COUNT, clusterEnd = LDR_COUNT;
    // Iterate through all combinations of matching indices and find
    // the pair with the greatest minimum difference
    for (uint8_t i = 0; i < matchCount - 1; ++i) {
        for (uint8_t j = i + 1; j < matchCount; ++j) {
            auto angleDifference =
                abs(LDR_BEARINGS[matches[i]] - LDR_BEARINGS[matches[j]]);
            angleDifference =
                angleDifference > 180 ? 360 - angleDifference : angleDifference;
            if (angleDifference > maxAngleDifference) {
                maxAngleDifference = angleDifference;
                clusterStart = matches[i];
                clusterEnd = matches[j];
            }
        }
    }

    // No cluster was found
    if (clusterStart == LDR_COUNT) {
        line.bearing = UINT16_NO_LINE;
        line.size = UINT8_NO_LINE;
        return;
    }

    // Calculate the line angle and size
    const auto clusterStartAngle = LDR_BEARINGS[clusterStart];
    const auto clusterEndAngle = LDR_BEARINGS[clusterEnd];
    const auto clusterMidpoint =
        angleMidpoint(clusterStartAngle, clusterEndAngle);
    const auto rawLineBearing = clusterMidpoint - 90.0;

    // Sets lineAngle as the midpoint of cluster ends
    line.bearing = roundf(
        (rawLineBearing > 180 ? 360 - rawLineBearing : rawLineBearing) * 100);
    // Sets lineSize as the ratio of the cluster size to 180°
    line.size = roundf(
        (smallerAngleDiff(clusterStartAngle, clusterEndAngle) / 180.0) * 100);
}

// CALIBRATE: Determines threshold values for the photodiodes.
void printLDRThresholds() {
    // min = green (field), max = white (line)
    uint16_t min[LDR_COUNT], max[LDR_COUNT];
    for (int i = 0; i < LDR_COUNT; ++i) {
        min[i] = 0xFFFF;
        max[i] = 0x0000;
    }

    const auto endTime = millis() + LDR_CALIBRATION_DURATION;
    while (millis() < endTime) {
        for (uint8_t i = 0; i < LDR_COUNT; ++i) {
            const auto value = readLDR(i);
            if (value < min[i]) min[i] = value;
            if (value > max[i]) max[i] = value;
        }
    }

    // Print the thresholds (averages of min and max)
    TEENSY_SERIAL.print("Thresholds: {");
    for (uint8_t i = 0; i < LDR_COUNT; ++i)
        TEENSY_SERIAL.printf("%d, ", (min[i] + max[i]) >> 1);
    TEENSY_SERIAL.print("}\n");
}

// DEBUG: Prints detected line data.
void printLDR() {
    findLine();

    uint16_t values[LDR_COUNT];
    for (uint8_t i = 0; i < LDR_COUNT; ++i) values[i] = readLDR(i);

    if (line.bearing != UINT16_NO_LINE) {
        TEENSY_SERIAL.printf("%03d.%02dº %01d.%02d |", line.bearing / 100,
                             line.bearing % 100, line.size / 100,
                             line.size % 100);
    } else {
        TEENSY_SERIAL.printf("             |");
    }
    for (uint8_t i = 0; i < LDR_COUNT >> 1; ++i)
        TEENSY_SERIAL.printf("%s", values[i] > LDR_THRESHOLDS[i] ? "1" : " ");
    TEENSY_SERIAL.printf("|");
    for (uint8_t i = LDR_COUNT >> 1; i < LDR_COUNT; ++i)
        TEENSY_SERIAL.printf("%s", values[i] > LDR_THRESHOLDS[i] ? "1" : " ");
    TEENSY_SERIAL.printf("|\n");
}

// ------------------------------ MAIN CODE START ------------------------------
void setup() {
    // Turn on the debug LED
    pinMode(PIN_LED_DEBUG, OUTPUT);
    digitalWrite(PIN_LED_DEBUG, HIGH);

    // Initialise pins
    pinMode(PIN_LDRMUX1_S0, OUTPUT);
    pinMode(PIN_LDRMUX1_S1, OUTPUT);
    pinMode(PIN_LDRMUX1_S2, OUTPUT);
    pinMode(PIN_LDRMUX1_S3, OUTPUT);
    pinMode(PIN_LDRMUX2_S0, OUTPUT);
    pinMode(PIN_LDRMUX2_S1, OUTPUT);
    pinMode(PIN_LDRMUX2_S2, OUTPUT);
    pinMode(PIN_LDRMUX2_S3, OUTPUT);
    pinMode(PIN_LDRMUX1_SIG, INPUT);
    pinMode(PIN_LDRMUX2_SIG, INPUT);

    analogReadResolution(12);

    // Initialise serial
    TEENSY_SERIAL.begin(TEENSY_MUX_BAUD_RATE);
#if DEBUG
    DEBUG_SERIAL.begin(DEBUG_BAUD_RATE);
#endif
    while (!TEENSY_SERIAL) delay(10);
#if DEBUG
    while (!DEBUG_SERIAL) delay(10);
#endif

    // Turn off the debug LED
    digitalWrite(PIN_LED_DEBUG, LOW);
}

void loop() {
    // Find the line
    findLine();

    // Send the line data over serial to Teensy
    uint8_t buf[sizeof(MUXTXPayload)];
    memcpy(buf, &line, sizeof(line));
    sendPacket(TEENSY_SERIAL, buf, MUX_TX_PACKET_SIZE, MUX_TX_SYNC_START_BYTE,
               MUX_TX_SYNC_END_BYTE);

    // TODO: Program a calibration mode
    // printLDRThresholds();
}
// ------------------------------- MAIN CODE END -------------------------------
