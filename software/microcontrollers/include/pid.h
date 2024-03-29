#ifndef PID_H
#define PID_H

#include <Arduino.h>

class PIDController {
  public:
    PIDController(const float setpoint, const float min, const float max,
                  const float kp, const float ki, const float kd,
                  const uint32_t minDt = 0, const float maxi = infinityf(),
                  const float maxSetpointChange = infinityf());

    // Update controller
    float advance(const float input, const float scaler = 1.0,
                  const bool isAngle = false);
    void reset();

    // Update parameters
    void updateSetpoint(const float value);
    void updateLimits(const float min, const float max);
    void updateGains(const float kp, const float ki, const float kd);

    void debugPrint(const char *name = nullptr, Stream &serial = Serial);

    float currentSetpoint() const { return _setpoint; }

  private:
    // Parameters
    float _targetSetpoint;
    float _setpoint;
    float _min;
    float _max;
    float _kp;
    float _ki;
    float _kd;
    uint32_t _minDt;
    float _maxi;
    float _maxSetpointChange;
    // Internal values
    float _integral = 0.0F;
    float _lastError = 0.0F;
    uint32_t _lastTime = 0;
    float _lastOutput = 0.0F;
    bool _justStarted = true;
    // For debugging
    float _lastInput = 0.0F;
    float _lastP = 0.0F;
    float _lastI = 0.0F;
    float _lastD = 0.0F;
    uint32_t _lastDt = 0.0F;
};

#endif
