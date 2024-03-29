#ifndef TEENSY_SENSORS_H
#define TEENSY_SENSORS_H

#include <PacketSerial.h>
#include <cmath>
#include <cstdint>
#include <deque>

#include "config.h"
#include "shared_config.h"
#include "vector.h"

struct Goals {
    bool newData = false;
    Vector offensive, defensive;
};

class Sensors {
  public:
    Sensors(PacketSerial &muxSerial, PacketSerial &tofSerial,
            PacketSerial &imuSerial, PacketSerial &coralSerial)
        : _muxSerial(muxSerial), _tofSerial(tofSerial), _imuSerial(imuSerial),
          _coralSerial(coralSerial){};

    void init();
    void waitForSubprocessorInit();

    void onMuxPacket(const byte *buffer, size_t size);
    void onTofPacket(const byte *buffer, size_t size);
    void onImuPacket(const byte *buffer, size_t size);
    void onCoralPacket(const byte *buffer, size_t size);

    void read();
    void markAsRead();

  private:
    // Write-possible private variables for sensor output
    struct {
        struct {
            bool newData = false;
            float value = NAN; // -179.99º to 180.00º

            bool established() const { return !std::isnan(value); }
        } angle;
        struct {
            bool newData = false;
            Vector value;

            bool exists() const { return value.exists(); }
        } position;
    } _robot;
    BluetoothPayload _otherRobot;
    struct {
        bool newData = false;
        float angleBisector = NAN; // -179.99º to 180.00º
        float depth = 0;           // 0.00 (inside edge) to 1.00 (outside edge)

        bool exists() const { return !std::isnan(angleBisector); }
    } _line;
    struct {
        struct {
            bool newData = false;
            float value = NAN; // 0.0 to TOF_MAX_DISTANCE

            bool valid() const { return !std::isnan(value); }
        } front, back, left, right;

        // Bounds in all four directions are established
        bool established() const {
            return !std::isnan(front.value) && !std::isnan(back.value) &&
                   !std::isnan(left.value) && !std::isnan(right.value);
        }
    } _bounds;
    struct {
        bool newData = false;
        Vector value;
    } _ball;
    Goals _goals;
    bool _hasBall = false; // Assume the robot does not have the ball initially

  public:
    // Read-only public interface to sensor output
    const decltype(_robot) &robot = _robot;
    const decltype(_otherRobot) &otherRobot = _otherRobot;
    const decltype(_line) &line = _line;
    const decltype(_bounds) &bounds = _bounds;
    const decltype(_ball) &ball = _ball;
    const decltype(_goals) &goals = _goals;
    const decltype(_hasBall) &hasBall = _hasBall;

  private:
    void _updateRobotPosition();

    // Serial managers to receive packets
    PacketSerial &_muxSerial;
    PacketSerial &_tofSerial;
    PacketSerial &_imuSerial;
    PacketSerial &_coralSerial;

    // Init flags
    bool _muxInit = false;
    bool _tofInit = false;
    bool _imuInit = false;
    bool _coralInit = false;

    // Internal state (robot angle)
    float _robotAngleOffset;

    // Internal state (line)
    bool _isInside = true;   // Which side of the line is the robot on?
    uint8_t switchCount = 0; // Has the angle jumped consistently enough to
                             // consider the robot to have "switched sides"?
    std::deque<float> _lineAngleBisectorHistory; // Past angles for comparison
};

#endif
