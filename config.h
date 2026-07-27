#pragma once

// ═══════════════════════════════════════════════════════════════
// 集中配置 — 所有可调参数
// ═══════════════════════════════════════════════════════════════

namespace cfg {

// 模型
constexpr const char* PARAM_PATH = "model/best.param";
constexpr const char* BIN_PATH   = "model/best.bin";
constexpr int   INPUT_SIZE       = 320;      // 模型输入尺寸
constexpr int   NUM_THREADS      = 2;        // ncnn 推理线程数

// 检测
constexpr float CONF_THRESHOLD  = 0.25f;
constexpr float NMS_THRESHOLD   = 0.45f;

// 摄像头
constexpr int   CAM_WIDTH       = 640;
constexpr int   CAM_HEIGHT      = 480;
constexpr int   CAM_FPS         = 30;

// 显示
constexpr const char* WIN_NAME  = "Steel Ball Detection";
constexpr int   WAITKEY_MS      = 10;       // imshow 后等待时间

}  // namespace cfg
