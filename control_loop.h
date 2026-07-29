#pragma once

/// @file control_loop.h
/// @brief 纯像素偏差闭环 — 极简版
///
/// 控制链路（单行）：
///   像素误差 e_px = uTarget - uBall
///     → PID 输出倾角 θ_cmd [°]
///     → pulses = pulsesPerDeg × θ_cmd
///     → motor.moveRelative(pulses)
///
/// 不依赖 detector.h / camera.h / coord_converter 的复杂转换。

#include "pid.h"
#include "coord_converter.h"
#include "serial_motor.h"

#include <string>

namespace ctrl {

// ═══════════════════════════════════════════════════════════
enum class LoopState {
    IDLE,       // 未初始化
    RUNNING,    // 正常闭环
    LOST,       // 钢球连续丢失
    ERROR       // 电机通信故障
};

// ═══════════════════════════════════════════════════════════
class ControlLoop {
public:
    ControlLoop();
    ~ControlLoop() = default;

    // ── 初始化 ──
    bool init(const std::string& motorPort);
    void shutdown();

    // ── 每帧调用 ──
    /// @param uBallPx  检测到的钢球像素列（-1 表示未检出）
    /// @param dt       距上帧时间 [s]
    /// @return 当前像素误差 e_px（用于 HUD 显示）
    double update(double uBallPx, double dt);

    // ── 目标 ──
    void setTargetPixel(int uPx);
    int  targetPixel() const { return targetUPx_; }

    // ── 状态 ──
    LoopState state()     const { return state_; }
    bool   isRunning()    const { return state_ == LoopState::RUNNING; }
    double pidOutputDeg() const { return pidOutputDeg_; }
    int    lostFrames()   const { return lostFrames_; }

    // ── 参数访问 ──
    PIDController&  pid()       { return pid_; }
    CoordConverter& converter() { return conv_; }

    // ── 紧急操作 ──
    void emergencyStop();
    void resetPID();

private:
    PIDController  pid_;
    CoordConverter conv_;
    SerialMotor    motor_;

    LoopState state_ = LoopState::IDLE;
    int   targetUPx_    = 320;
    double pidOutputDeg_ = 0.0;
    int   motorPulses_  = 0;
    int   lostFrames_   = 0;

    static constexpr int kMaxLostFrames = 10;
    static constexpr int kDeadZonePulses = 5;
};

} // namespace ctrl
