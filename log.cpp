#include "log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>

namespace dlog {

static std::ofstream gFile;

// ═══════════════════════════════════════════════════════════════
bool init(const std::string& filepath)
{
    gFile.open(filepath, std::ios::out | std::ios::trunc);
    if (!gFile.is_open()) {
        std::cerr << "[Log] 无法创建日志文件: " << filepath << std::endl;
        return false;
    }

    // 写表头
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&t));

    gFile << "# YOLOv8n NCNN Detection Log\n";
    gFile << "# started: " << timeBuf << "\n";
    gFile << "# format: frame,  x_center,  y_center,  "
             "width,  height,  confidence,  cls_id\n";
    gFile << "# (empty fields = no detection in that frame)\n";
    gFile << "#\n";

    std::cout << "[Log] " << filepath << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════
void writeFrame(int frameIdx, const std::vector<Detection>& detections)
{
    if (!gFile.is_open()) return;

    if (detections.empty()) {
        // 无检测：写 frame 编号后用空字段占位
        gFile << frameIdx << ",\n";
        return;
    }

    for (const auto& d : detections) {
        const int cx = d.box.x + d.box.width  / 2;
        const int cy = d.box.y + d.box.height / 2;
        gFile << frameIdx << ",  "
              << cx << ",  " << cy << ",  "
              << d.box.width << ",  " << d.box.height << ",  "
              << d.confidence << ",  " << d.cls_id << "\n";
    }
    gFile.flush();  // 即时写入，避免崩溃丢失
}

// ═══════════════════════════════════════════════════════════════
void close()
{
    if (gFile.is_open()) {
        gFile << "# end of log\n";
        gFile.close();
        std::cout << "[Log] closed" << std::endl;
    }
}

}  // namespace dlog
