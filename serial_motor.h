#pragma once

/// @file serial_motor.h
/// @brief Pi ↔ STM32 串口通信 — Linux termios，文本协议
///
/// 协议格式（每行以 \n 结尾）：
///   Pi → STM32:  "CMD [ARGS...]\n"
///   STM32 → Pi:  "OK <RESULT>\n"  或  "ERR <CODE>\n"
///
/// 命令列表:
///   ENABLE          使能电机（上电锁轴）
///   DISABLE         失能电机（自由）
///   MOVE <p> <s> <a>  相对移动 p 脉冲，速度 s RPM，加速度 a
///   STOP            停止
///   READPOS         读当前位置
///   ZERO            将当前位置标为零点
///   STATUS          读状态标志

#include <cstdint>
#include <string>

namespace ctrl {

class SerialMotor {
public:
    SerialMotor();
    ~SerialMotor();

    // 禁止拷贝
    SerialMotor(const SerialMotor&) = delete;
    SerialMotor& operator=(const SerialMotor&) = delete;

    /// 打开串口
    /// @param portPath  如 "/dev/serial0", "/dev/ttyAMA2"
    /// @param baudRate  波特率（默认 115200）
    /// @return 成功返回 true
    bool open(const std::string& portPath, int baudRate = 115200);

    /// 关闭串口
    void close();

    /// 串口是否已打开
    bool isOpen() const { return fd_ >= 0; }

    // ═══════════════════════════════════════════════════
    // 控制命令
    // ═══════════════════════════════════════════════════

    bool enable();
    bool disable();
    bool stop();

    /// 相对移动
    /// @param pulses  脉冲数（正=右端上升，负=右端下降）
    /// @param speed   RPM
    /// @param accel   加速度档位 (1~255)
    /// @param posX10  输出：移动后的位置×10（从应答中解析）
    /// @return 成功返回 true
    bool moveRelative(int pulses, int speed, int accel, int& posX10);

    // ═══════════════════════════════════════════════════
    // 查询命令
    // ═══════════════════════════════════════════════════

    /// 读取当前位置（×10，单位 0.1°）
    /// @param posX10  输出：位置×10
    /// @return 成功返回 true
    bool readPosition(int& posX10);

    /// 读取状态标志位
    /// @param flags  输出：状态字节
    /// @return 成功返回 true
    bool readStatus(uint8_t& flags);

    /// 设当前位置为零点
    bool zero();

    // ═══════════════════════════════════════════════════
    // 便捷方法
    // ═══════════════════════════════════════════════════

    /// 读位置并转为角度 [°]
    bool readPositionDeg(double& deg);

    /// 最近一次通信是否成功
    bool lastOk() const { return lastOk_; }

    /// 最近一次错误信息
    const std::string& lastError() const { return lastError_; }

private:
    // ── 底层 ──
    std::string sendAndWait(const std::string& cmd, int timeoutMs = 200);
    bool configurePort(int baudRate);

    int  fd_;               // 串口文件描述符
    bool lastOk_;
    std::string lastError_;

    // 位置追踪
    int  trackedPosX10_;    // 从 MOVE/READPOS 应答中跟踪的位置
};

} // namespace ctrl
