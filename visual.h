#pragma once

#include <opencv2/core.hpp>
#include <vector>
#include "detector.h"

// ═══════════════════════════════════════════════════════════════
// 可视化 — 画框 + HUD 文字叠加
// ═══════════════════════════════════════════════════════════════

/// 在原图上画检测结果（绿框 + 红圆心十字 + 置信度标签）
void drawDetections(cv::Mat& image, const std::vector<Detection>& detections);

/// 叠加左上角 HUD（状态 + FPS + 耗时）
void drawHud(cv::Mat& image, bool detecting,
             float fps, int detectMs);
