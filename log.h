#pragma once

#include <string>
#include <vector>
#include "detector.h"

// ═══════════════════════════════════════════════════════════════
// 检测日志 — 将每帧识别结果写入 txt 文件
// ═══════════════════════════════════════════════════════════════

namespace dlog {

/// 初始化日志文件（程序启动时调用一次），返回是否成功
bool init(const std::string& filepath);

/// 记录一帧的检测结果（无检测时写空行标记）
void writeFrame(int frameIdx, const std::vector<Detection>& detections);

/// 关闭日志文件（程序退出时调用）
void close();

}  // namespace dlog
