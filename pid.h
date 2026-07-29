#pragma once

/// @file pid.h
/// @brief 独立 PID 控制器 — 不依赖任何项目头文件

namespace ctrl {

/**
 * 位置式 PID + 积分抗饱和 + 测量值微分
 *
 * 用法:
 *   PIDController pid(Kp, Ki, Kd, outMax, dt);
 *   while (...) {
 *       double output = pid.update(setpoint, measurement);
 *       // output 已经限幅在 [-outMax, outMax]
 *   }
 */
class PIDController {
public:
    /// @param Kp     比例增益
    /// @param Ki     积分增益
    /// @param Kd     微分增益
    /// @param outMax 输出限幅绝对值（>0）
    /// @param dt     控制周期 [s]
    PIDController(double Kp, double Ki, double Kd,
                  double outMax, double dt);

    /// 重置积分项和历史误差（切换目标或模式切换时调用）
    void reset();

    /// @param setpoint    目标值（与 measurement 同一量纲）
    /// @param measurement 当前测量值
    /// @return 控制量（已限幅）
    double update(double setpoint, double measurement);

    // ── 在线调参 ──
    void setGains(double Kp, double Ki, double Kd);
    void setOutMax(double outMax);
    void setDt(double dt);

    // ── 调试/监控 ──
    double integral()   const { return integral_; }
    double lastError()  const { return lastError_; }
    double lastOutput() const { return lastOutput_; }

private:
    double Kp_, Ki_, Kd_;
    double outMax_;
    double dt_;
    double integral_;
    double iLimit_;         // 积分限幅值 (outMax / Ki 上限)
    double lastError_;
    double lastMeasurement_;
    double lastOutput_;
};

} // namespace ctrl
