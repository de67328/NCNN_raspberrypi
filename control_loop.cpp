#include "control_loop.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace ctrl {

// ═══════════════════════════════════════════════════════════
ControlLoop::ControlLoop()
    : pid_(/*Kp*/0.08, /*Ki*/0.005, /*Kd*/0.02,
           /*outMax*/8.0,   // 最大倾角 ±8°
           /*dt*/0.05)      // 默认 20Hz
    , conv_()               // uCenter=320, pulsesPerDeg=80
    , motor_()
{}

// ═══════════════════════════════════════════════════════════
bool ControlLoop::init(const std::string& motorPort)
{
    if (!motor_.open(motorPort)) {
        state_ = LoopState::ERROR;
        return false;
    }
    if (!motor_.enable()) {
        state_ = LoopState::ERROR;
        return false;
    }

    // 取当前位置作为零点参照
    int posX10 = 0;
    motor_.readPosition(posX10);
    motorPulses_ = posX10;

    targetUPx_ = conv_.uCenter;
    state_ = LoopState::RUNNING;
    std::cout << "[ControlLoop] pixel mode, target=" << targetUPx_
              << " px" << std::endl;
    return true;
}

void ControlLoop::shutdown()
{
    if (state_ != LoopState::IDLE) {
        motor_.stop();
        motor_.disable();
        motor_.close();
    }
    state_ = LoopState::IDLE;
}

// ═══════════════════════════════════════════════════════════
double ControlLoop::update(double uBallPx, double dt)
{
    if (state_ != LoopState::RUNNING && state_ != LoopState::LOST)
        return 0.0;

    if (dt > 0.0)
        pid_.setDt(dt);

    constexpr double kInvalid = -1.0;

    if (uBallPx > kInvalid) {
        // ── 钢球检出 ──
        lostFrames_ = 0;

        // 像素误差 → PID → 倾角指令 [°]
        double errorPx = static_cast<double>(targetUPx_) - uBallPx;
        pidOutputDeg_ = pid_.update(0.0, -errorPx);
        // 注：PID.setpoint=0，measurement=-error，效果等价于 setpoint=target

        // 倾角 → 脉冲
        int deltaPulses = conv_.pidOutToPulses(pidOutputDeg_);

        // 死区 + 发送
        if (std::abs(deltaPulses) > kDeadZonePulses) {
            int posAfter = 0;
            if (motor_.moveRelative(deltaPulses, 200, 10, posAfter)) {
                motorPulses_ = posAfter;
            }
        }

        if (state_ == LoopState::LOST) {
            std::cout << "[ControlLoop] ball found, resumed" << std::endl;
            state_ = LoopState::RUNNING;
        }
        return errorPx;

    } else {
        // ── 钢球丢失 ──
        ++lostFrames_;
        if (lostFrames_ >= kMaxLostFrames && state_ == LoopState::RUNNING) {
            std::cerr << "[ControlLoop] ball lost " << lostFrames_
                      << " frames" << std::endl;
            state_ = LoopState::LOST;
            motor_.stop();
        }
        return 0.0;
    }
}

// ═══════════════════════════════════════════════════════════
void ControlLoop::setTargetPixel(int uPx)
{
    targetUPx_ = uPx;
    pid_.reset();
    std::cout << "[ControlLoop] target pixel = " << uPx << std::endl;
}

void ControlLoop::emergencyStop()
{
    motor_.stop();
    motor_.disable();
    state_ = LoopState::IDLE;
    std::cout << "[ControlLoop] EMERGENCY STOP" << std::endl;
}

void ControlLoop::resetPID()
{
    pid_.reset();
}

} // namespace ctrl
