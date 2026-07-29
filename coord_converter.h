#pragma once

/// @file coord_converter.h
/// @brief 极简转换 — 只做「倾角 → 脉冲」一步
///
/// 实测倾角极小，跳过 theta 补偿和像素→cm 转换。
/// PID 在像素空间工作，本类只负责角度→脉冲映射。

#include <cmath>

namespace ctrl {

class CoordConverter {
public:
    /// 目标像素列（画面中管道中心 O 对应的像素位置）
    int uCenter = 320;

    /// 每度倾角对应电机脉冲数 [pulse/°]
    /// 标定方法：发已知角度，测量脉冲数，取比值
    double pulsesPerDeg = 80.0;

    CoordConverter() = default;

    /// PID 输出的倾角指令 → 相对脉冲数
    int pidOutToPulses(double pidOutDeg) const
    {
        return static_cast<int>(std::round(pidOutDeg * pulsesPerDeg));
    }
};

} // namespace ctrl
