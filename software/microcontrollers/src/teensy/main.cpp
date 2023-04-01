#include <Arduino.h>
#include <PacketSerial.h>

#include "counter.h"
#include "pid.h"
#include "shared_config.h"
#include "teensy/include/config.h"
#include "teensy/include/movement.h"
#include "teensy/include/sensors.h"

// Serial
PacketSerial muxSerial;
PacketSerial tofSerial;
PacketSerial imuSerial;
PacketSerial coralSerial;

// IO
auto sensors = Sensors(muxSerial, tofSerial, imuSerial, coralSerial);
auto movement = Movement();

// Counters
auto debugPrintCounter = Counter();

void performCalibration() {
#ifdef CALIBRATE_IMU
    // CALIBRATE IMU
    // Set STM32 IMU to calibration mode
    const bool calibrating = true;
    byte buf[sizeof(IMURXPayload)];
    memcpy(buf, &calibrating, sizeof(calibrating));
    // Send 100 times to ensure it gets through
    for (int i = 0; i < 100; i++) imuSerial.send(buf, sizeof(buf));

    movement.speed = 100;
    auto rotateCounter = Counter();
    auto speedCounter = Counter();
    while (1) {
        // Redirect IMU Serial to monitor
        while (imuSerial.available() > 0) Serial.write(imuSerial.read());

        // Drive motors to create magnetometer noise
        if (speedCounter.millisElapsed(100)) {
            movement.speed = -movement.speed;
        }
        if (rotateCounter.millisElapsed(1)) {
            if (movement.angle == 18000)
                movement.angle = -17000;
            else
                movement.angle += 100;
        }
        movement.drive();
    }
#endif
#ifdef CALIBRATE_MUX
    // CALIBRATE MUX
    // Set STM32 MUX to calibration mode
    const bool calibrating = true;
    byte buf[sizeof(MUXRXPayload)];
    memcpy(buf, &calibrating, sizeof(calibrating));
    // Send 100 times to ensure it gets through
    for (int i = 0; i < 100; i++) muxSerial.send(buf, sizeof(buf));

    while (1) {
        // Redirect MUX Serial to monitor
        while (MUX_SERIAL.available() > 0) Serial.write(MUX_SERIAL.read());
    }
#endif
#ifdef CALIBRATE_ROBOT_ANGLE_CONTROLLER
    auto circleCounter = Counter();
    auto rotateCounter = Counter();
    auto controller = PIDController(0,         // Target angle
                                    -256, 256, // Output limits
                                    KP_ROBOT_ANGLE, KI_ROBOT_ANGLE,
                                    KD_ROBOT_ANGLE,    // Gains
                                    MIN_DT_ROBOT_ANGLE // min dt
    );
    while (10) {
        // Read all sensor values
        sensors.read();

        // Keep straight
        if (sensors.robot.newData) {
            movement.angularVelocity = controller.advance(sensors.robot.angle);

            // Print controller info
            controller.debugPrint("Heading");
        }

        // Spin in a circle
        movement.speed = 80;
        if (circleCounter.millisElapsed(4)) {
            if (movement.angle == 18000)
                movement.angle = -17000;
            else
                movement.angle += 100;
        }
        if (rotateCounter.millisElapsed(50)) {
            if (controller.setpoint == 18000)
                controller.setpoint = -17000;
            else
                controller.setpoint += 100;
        }

        // Actuate outputs
        movement.drive();
    }
#endif
}

void performDebug() {
#ifdef DEBUG_MUX
    // Redirect MUX Serial to monitor
    while (MUX_SERIAL.available() > 0) Serial.write(MUX_SERIAL.read());
#endif
#ifdef DEBUG_TOF
    // Redirect TOF Serial to monitor
    while (TOF_SERIAL.available() > 0) Serial.write(TOF_SERIAL.read());
#endif
#ifdef DEBUG_IMU
    // Redirect IMU Serial to monitor
    while (IMU_SERIAL.available() > 0) Serial.write(IMU_SERIAL.read());
#endif
#ifdef DEBUG_CORAL
    // Redirect Coral Serial to monitor
    while (CORAL_SERIAL.available() > 0) Serial.write(CORAL_SERIAL.read());
#endif

#ifdef DEBUG_TEENSY
    if (debugPrintCounter.millisElapsed(100)) {
        // Print debug data
        Serial.printf("Line ");
        if (sensors.line.exists())
            Serial.printf("%4d.%02dº ", (int)sensors.line.angleBisector,
                          abs((int)(sensors.line.angleBisector * 100) % 100));
        else
            Serial.printf("         ");
        Serial.printf("%01d.%02d | ", (int)sensors.line.depth,
                      abs((int)(sensors.line.depth * 100) % 100));
        if (sensors.robot.established())
            Serial.printf("Robot Angle %4d.%02dº | ", (int)sensors.robot.angle,
                          abs((int)(sensors.robot.angle * 100) % 100));
        else
            Serial.printf("Robot Angle          | ");
        Serial.print("Bounds ");
        if (sensors.bounds.front.established())
            Serial.printf("F: %3d.%1d cm ", (int)sensors.bounds.front.value,
                          abs((int)(sensors.bounds.front.value * 10) % 10));
        else
            Serial.printf("F:          ");
        if (sensors.bounds.back.established())
            Serial.printf("B: %3d.%1d cm ", (int)sensors.bounds.back.value,
                          abs((int)(sensors.bounds.back.value * 10) % 10));
        else
            Serial.printf("B:          ");
        if (sensors.bounds.left.established())
            Serial.printf("L: %3d.%1d cm ", (int)sensors.bounds.left.value,
                          abs((int)(sensors.bounds.left.value * 10) % 10));
        else
            Serial.printf("L:          ");
        if (sensors.bounds.right.established())
            Serial.printf("R: %3d.%1d cm | ", (int)sensors.bounds.right.value,
                          abs((int)(sensors.bounds.right.value * 10) % 10));
        else
            Serial.printf("R:          | ");
        if (sensors.ball.exists())
            Serial.printf("Ball %4d.%02dº %4d.%02d cm | ",
                          (int)sensors.ball.angle,
                          abs((int)(sensors.ball.angle * 100) % 100),
                          (int)sensors.ball.distance,
                          abs((int)(sensors.ball.distance * 100) % 100));
        else
            Serial.printf("Ball                     | ");
        Serial.printf("Has Ball: %d | ", sensors.hasBall);
        Serial.printf("Other Robot: 0x%02X | ", sensors.otherRobot.testByte);
        Serial.println();
    }

    // // Print loop time
    // printLoopTime();

    // // Test motors
    // movement.angle = 0;
    // movement.speed = 0;
    // movement.angularVelocity = 200;

    // // Line Track
    // TODO
#endif
}

// ------------------------------ MAIN CODE START ------------------------------
void setup() {
    // Turn on the debug LED
    pinMode(PIN_LED_DEBUG, OUTPUT);
    digitalWriteFast(PIN_LED_DEBUG, HIGH);

    // Initialise monitor serial
    Serial.begin(MONITOR_BAUD_RATE);
    Serial.println("Initialising...");

    // Initialise motors and sensors and wait for completion
    movement.init();
    sensors.init();
#ifndef DEBUG_MUX
    muxSerial.setPacketHandler(
        [](const byte *buf, size_t size) { sensors.onMuxPacket(buf, size); });
#endif
#ifndef DEBUG_TOF
    tofSerial.setPacketHandler(
        [](const byte *buf, size_t size) { sensors.onTofPacket(buf, size); });
#endif
#ifndef DEBUG_IMU
    imuSerial.setPacketHandler(
        [](const byte *buf, size_t size) { sensors.onImuPacket(buf, size); });
#endif
#ifndef DEBUG_CORAL
    coralSerial.setPacketHandler(
        [](const byte *buf, size_t size) { sensors.onCoralPacket(buf, size); });
#endif
#ifndef DONT_WAIT_FOR_SUBPROCESSOR_INIT
    sensors.waitForSubprocessorInit();
#endif

    // Turn off the debug LED
    Serial.println("Initialisation complete");
    digitalWriteFast(PIN_LED_DEBUG, LOW);

#ifdef CALIBRATE
    // Runs the corresponding calibration if flag is defined
    performCalibration(); // this would probably be blocking
#endif
}

void loop() {
    // Read all sensor values
    sensors.read();

    // Keep straight
    if (sensors.robot.newData)
        movement.updateHeadingController(sensors.robot.angle);

    // Move about the balls
    if (sensors.ball.exists()) {
        // We can't just move straight at the ball. We need to move behind
        // it in a curve. Here, we calculate the angle offset we need to
        // achieve this.
        const auto angleOffset =
            // The angle offset is directly related to the angle of the
            // ball, but we constrain it to 90º because the robot should
            // never move in a range greater than 90º away from the ball, as
            // it would be moving away from the ball.
            constrain(sensors.ball.angle, -9000, 9000) *
            // The angle offset undergoes exponential decay. As the ball
            // gets closer to the robot, the robot moves more directly at
            // the ball.
            fmin(BALL_MOVEMENT_A *
                     pow(exp(1), BALL_MOVEMENT_B * sensors.ball.distance),
                 1.0);
        // Then, we pack it into instructions for our drive function.
        movement.angle = sensors.ball.angle + angleOffset;
        // movement.speed = hasBall ? 280 : 240;
        movement.velocity = 100;
    } else {
        // If we don't see the ball, we don't move.
        // TODO: Move to the center of the field
        movement.angle = 0;
        movement.velocity = 0;
    }

    // Slow down near the wall
    // TODO

    // Avoid the lines
    if (sensors.line.newData && sensors.line.exists()) {
        if (sensors.line.depth < 0.3) {
            // Stop inside the line to prevent jerking
            movement.stop = true;
            // TODO: Line track towards the ball
        } else {
            // Move away from the line
            movement.angle = sensors.line.angleBisector;
            movement.velocity = sensors.line.depth * 255;
        }
    }

#ifdef DEBUG
    // Runs any debug code if the corresponding flag is defined
    performDebug();
#endif

    // Actuate outputs
    movement.drive();

    // Write to bluetooth
    auto bluetoothOutboundPayload = BluetoothPayload::create(0x01);
    byte buf[sizeof(TOFRXPayload)];
    memcpy(buf, &bluetoothOutboundPayload, sizeof(bluetoothOutboundPayload));
    tofSerial.send(buf, sizeof(buf));

    // Mark all sensor data as old
    sensors.markAsRead();
}
// ------------------------------- MAIN CODE END -------------------------------
