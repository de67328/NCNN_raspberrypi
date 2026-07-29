#include "pid.h"
#include <algorithm>
#include <cmath>

namespace ctrl {

PIDController::PIDController(double Kp, double Ki, double Kd,
                             double outMax, double dt)
    : Kp_(Kp), Ki_(Ki), Kd_(Kd)
    , outMax_(outMax)
    , dt_(dt)
    , integral_(0.0)
    , iLimit_(0.0)
    , lastError_(0.0)
    , lastMeasurement_(0.0)
    , lastOutput_(0.0)
{
    // 积分限幅：防止积分饱和到远超 outMax 的程度
    iLimit_ = (Ki_ > 1e-9) ? (outMax_ / Ki_) : 0.0;
}

void PIDController::reset()
{
    integral_         = 0.0;
    lastError_        = 0.0;
    lastMeasurement_  = 0.0;
    lastOutput_       = 0.0;
}

double PIDController::update(double setpoint, double measurement)
{
    // 误差
    double error = setpoint - measurement;

    // ── P ──
    double pOut = Kp_ * error;

    // ── I（梯形积分 + 抗饱和）──
    if (Ki_ > 1e-9 && iLimit_ > 1e-9) {
        integral_ += Ki_ * (error + lastError_) * 0.5 * dt_;
        integral_ = std::clamp(integral_, -iLimit_, iLimit_);
    }
    double iOut = integral_;

    // ── D（对测量值微分，避免 setpoint 跳变冲击）──
    double dOut = 0.0;
    if (Kd_ > 1e-9 && dt_ > 1e-9) {
        dOut = -Kd_ * (measurement - lastMeasurement_) / dt_;
    }

    // ── 合成 ──
    double output = pOut + iOut + dOut;
    output = std::clamp(output, -outMax_, outMax_);

    // 条件积分：输出饱和时不累加（进一步抗饱和）
    if (std::abs(output) >= outMax_) {
        // 如果积分方向与输出方向相同，回退本次积分
        if ((error > 0 && integral_ > 0) || (error < 0 && integral_ < 0)) {
            integral_ -= Ki_ * (error + lastError_) * 0.5 * dt_;
            integral_ = std::clamp(integral_, -iLimit_, iLimit_);
        }
    }

    lastError_       = error;
    lastMeasurement_ = measurement;
    lastOutput_      = output;
    return output;
}

void PIDController::setGains(double Kp, double Ki, double Kd)
{
    Kp_ = Kp; Ki_ = Ki; Kd_ = Kd;
    iLimit_ = (Ki_ > 1e-9) ? (outMax_ / Ki_) : 0.0;
    integral_ = std::clamp(integral_, -iLimit_, iLimit_);
}

void PIDController::setOutMax(double outMax)
{
    outMax_ = outMax;
    iLimit_ = (Ki_ > 1e-9) ? (outMax_ / Ki_) : 0.0;
    integral_ = std::clamp(integral_, -iLimit_, iLimit_);
}

void PIDController::setDt(double dt)
{
    dt_ = dt;
}

} // namespace ctrl
