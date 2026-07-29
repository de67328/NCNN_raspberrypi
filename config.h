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
constexpr float CONF_THRESHOLD  = 0.80f;
constexpr float NMS_THRESHOLD   = 0.45f;

// 摄像头
constexpr int   CAM_WIDTH       = 640;
constexpr int   CAM_HEIGHT      = 480;
constexpr int   CAM_FPS         = 30;

// 显示
constexpr const char* WIN_NAME  = "Steel Ball Detection";
constexpr float DISPLAY_SCALE   = 0.55f;    // 显示缩放比例 (0~1)
constexpr int   WAITKEY_MS      = 10;       // imshow 后等待时间

}  // namespace cfg
