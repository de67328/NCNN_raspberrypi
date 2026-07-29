#pragma once

/// @file coord_converter.h
/// @brief 坐标转换器 — 集中管理所有物理量、标定常数和换算
///
/// 引用 config.h 获取相机参数宏，避免硬编码。
/// 所有公式对应 note.tex §简化工程模型。

#include "config.h"

#include <cmath>

namespace ctrl {

class CoordConverter {
public:
    // ═══════════════════════════════════════════════════════
    // 标定常数（可在线修改，用于现场调参/标定）
    // ═══════════════════════════════════════════════════════

    /// 目标像素列 — 画面中管道中心 O 对应位置 [px]
    int uCenter = cfg::U_CENTER;

    /// 像素→物理转换系数 [cm/px]
    /// 标定：测量两刻度线间像素数，alphaX = 0.1 * N_ticks / Δu
    double alphaX = cfg::ALPHA_X;

    /// 每度倾角对应电机脉冲数 [pulse/°]
    /// 标定：电机走已知角度，记录脉冲数
    double pulsesPerDeg = cfg::PULSES_PER_DEG;

    // ═══════════════════════════════════════════════════════
    // 物理常数（原则上不变，但允许在线微调）
    // ═══════════════════════════════════════════════════════

    double pipeLen = cfg::PIPE_LEN;     // [cm] 摆杆长度
    double hingeH  = cfg::HINGE_H;      // [cm] 铰链高度
    double gravity = cfg::GRAVITY;      // [cm/s²]
    double beta    = cfg::BETA;         // 有效质量系数 = 5/7

    // ═══════════════════════════════════════════════════════
    // 相机参数（只读，来自 config.h）
    // ═══════════════════════════════════════════════════════

    static constexpr int camWidth()  { return cfg::CAM_WIDTH; }
    static constexpr int camHeight() { return cfg::CAM_HEIGHT; }
    static constexpr int camFps()    { return cfg::CAM_FPS; }

    // ═══════════════════════════════════════════════════════
    // 核心换算
    // ═══════════════════════════════════════════════════════

    /// 对象增益 K = α·β·g / L  [px/s²/cm]
    double computePlantGain() const
    {
        return alphaX * beta * gravity / pipeLen;
    }

    /// PID 输出倾角 [°] → 脉冲数
    int pidOutToPulses(double pidOutDeg) const
    {
        return static_cast<int>(std::round(pidOutDeg * pulsesPerDeg));
    }

    /// PID 输出倾角 [°] → 高度变化 Δh [cm]
    double pidOutToDeltaH(double pidOutDeg) const
    {
        return pipeLen * std::tan(pidOutDeg * M_PI / 180.0);
    }

    /// 脉冲累积 → 当前高度 [cm]
    double pulsesToHeight(int totalPulses) const
    {
        double hPerDeg  = pidOutToDeltaH(1.0);
        double hPerPulse = hPerDeg / pulsesPerDeg;
        return totalPulses * hPerPulse;
    }

    /// 脉冲累积 → 当前倾角 [°]
    double pulsesToThetaDeg(int totalPulses) const
    {
        return static_cast<double>(totalPulses) / pulsesPerDeg;
    }

    /// 像素列 → 物理位置 [cm]（以 O 为原点，右为正）
    double pixelToCm(double uPx) const
    {
        return (uPx - static_cast<double>(uCenter)) * alphaX;
    }

    /// 物理位置 [cm] → 像素列
    double cmToPixel(double xCm) const
    {
        return xCm / alphaX + static_cast<double>(uCenter);
    }

    // ═══════════════════════════════════════════════════════
    // PID 参数理论计算（期望极点法）
    // ═══════════════════════════════════════════════════════

    /// 由期望极点反算 PID 增益（物理空间，输出高度 h [cm]）
    void computePidGains(double omegaN, double zeta, double gamma,
                         double& Kp, double& Ki, double& Kd) const
    {
        double K = computePlantGain();
        Kd = omegaN * (2.0 * zeta + gamma) / K;
        Kp = omegaN * omegaN * (1.0 + 2.0 * zeta * gamma) / K;
        Ki = gamma * omegaN * omegaN * omegaN / K;
    }

    /// 转换为像素空间 PID 增益（输出倾角 [°]）
    void computePixPidGains(double omegaN, double zeta, double gamma,
                            double& KpPx, double& KiPx, double& KdPx) const
    {
        double KpCm, KiCm, KdCm;
        computePidGains(omegaN, zeta, gamma, KpCm, KiCm, KdCm);
        double cmToDeg = 180.0 / (M_PI * pipeLen);
        KpPx = KpCm * cmToDeg;
        KiPx = KiCm * cmToDeg;
        KdPx = KdCm * cmToDeg;
    }

    /// 验证稳定性: Kp·Kd > Ki/K
    bool checkStability(double Kp, double Ki, double Kd) const
    {
        double K = computePlantGain();
        return (Kp * Kd > Ki / K) && (Kd > 0.0) && (Ki > 0.0);
    }

    /// 打印全部参数（调试用）
    void dump() const;
};

} // namespace ctrl
