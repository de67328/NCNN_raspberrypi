#pragma once

// ═══════════════════════════════════════════════════════════════
// 集中配置 — 所有可调参数
// ═══════════════════════════════════════════════════════════════

namespace cfg {

// 模型
constexpr const char* PARAM_PATH =
    "model/steel_ball_yolov8n_320x96_b1_fp32_best.param";
constexpr const char* BIN_PATH =
    "model/steel_ball_yolov8n_320x96_b1_fp32_best.bin";
constexpr int   INPUT_WIDTH      = 320;
constexpr int   INPUT_HEIGHT     = 96;
constexpr int   NUM_THREADS      = 3;        // ncnn 推理线程数 (Pi4B:4核,留1核)

// 检测
constexpr float CONF_THRESHOLD  = 0.5f;
constexpr float NMS_THRESHOLD   = 0.45f;

// ROI — 只识别横条带状区域，其余填充纯色（减少误检，不改变推理量）
//       设为 0 和 CAM_HEIGHT 即关闭 ROI（全图识别）
constexpr int   ROI_TOP         = 200;       // ROI 上边沿 y (px)，0=最上方
constexpr int   ROI_BOTTOM      = 250;       // ROI 下边沿 y (px)，CAM_HEIGHT=最下方
constexpr int   ROI_FILL_B      = 0;         // ROI 外填充色 (BGR)
constexpr int   ROI_FILL_G      = 0;
constexpr int   ROI_FILL_R      = 0;

// 摄像头
constexpr int   CAM_WIDTH       = 640;
constexpr int   CAM_HEIGHT      = 480;
constexpr int   CAM_FPS         = 60;        // OV5647: 640x480 最高 62.5fps
constexpr int   SHUTTER_US      = 8000;      // 快门 (微秒), 2000=1/500s
constexpr float GAIN            = 10.0f;      // 模拟增益 (1~16)
constexpr bool  FLIP_H          = true;     // 左右镜像翻转
constexpr bool  FLIP_V          = true;     // 上下翻转


// 显示
constexpr const char* WIN_NAME  = "Steel Ball Detection";
constexpr float DISPLAY_SCALE   = 1.0f;    // 显示缩放比例 (0~1)
constexpr int   WAITKEY_MS      = 10;       // imshow 后等待时间

// 日志
constexpr const char* LOG_PATH   = "detect_log.txt";  // 检测坐标输出文件


// ═══════════════════════════════════════════════════════════════
// 控制 — 串口 & 电机
// ═══════════════════════════════════════════════════════════════
constexpr const char* SERIAL_PORT  = "/dev/serial0";  // Pi ↔ STM32
constexpr int   SERIAL_BAUD        = 115200;
constexpr int   MOTOR_DEFAULT_SPEED = 200;    // RPM
constexpr int   MOTOR_DEFAULT_ACCEL = 10;     // 加速度档位
constexpr int   MOTOR_DEADZONE_PULSES = 5;    // 死区脉冲数

// ═══════════════════════════════════════════════════════════════
// 标定参数 (CoordConverter)
// ═══════════════════════════════════════════════════════════════
constexpr int   U_CENTER        = CAM_WIDTH / 2;   // 目标像素列 (默认画面中央)
constexpr double ALPHA_X        = 0.05;   // [cm/px] 像素→物理转换系数
constexpr double PULSES_PER_DEG = 80.0;   // [pulse/°] 每度倾角对应脉冲

// ═══════════════════════════════════════════════════════════════
// 物理参数
// ═══════════════════════════════════════════════════════════════
constexpr double PIPE_LEN       = 25.0;   // [cm] 摆杆长度
constexpr double HINGE_H        = 5.0;    // [cm] 左端铰链高度
constexpr double GRAVITY        = 981.0;  // [cm/s²]
constexpr double BETA           = 5.0 / 7.0; // 球体纯滚动有效质量系数 (I=2/5 m r²)

// ═══════════════════════════════════════════════════════════════
// PID 默认参数 (像素空间)
// ═══════════════════════════════════════════════════════════════
constexpr double PID_KP         = 0.08;   // [°/px]  比例增益
constexpr double PID_KI         = 0.005;  // [°/px/s] 积分增益
constexpr double PID_KD         = 0.02;   // [°·s/px] 微分增益
constexpr double PID_OUT_MAX    = 8.0;    // [°]      最大倾角输出
constexpr double CONTROL_HZ     = 20.0;   // [Hz]     控制频率
constexpr double CONTROL_DT     = 1.0 / CONTROL_HZ;  // [s] 控制周期

// ═══════════════════════════════════════════════════════════════
// 控制逻辑
// ═══════════════════════════════════════════════════════════════
constexpr int   MAX_LOST_FRAMES = 10;     // 连续丢帧阈值
constexpr double DET_CONFIDENCE_MIN = 0.5; // 最低置信度

}  // namespace cfg
