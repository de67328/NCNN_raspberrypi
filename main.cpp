/**
 * main.cpp — 串行同步钢珠检测 (C++17)
 *
 * 最简管线:  取帧 → 检测 → 画框 → 显示  (单线程，无 CLI)
 * 用法: ./ball_detect
 */

#include "config.h"
#include "detector.h"
#include "camera.h"
#include "visual.h"

#include <chrono>
#include <cstdlib>
#include <iostream>

#include <opencv2/highgui.hpp>

// ═══════════════════════════════════════════════════════════════
int main()
{
    std::cout << "========================================\n";
    std::cout << "  YOLOv8n NCNN Steel Ball Detection\n";
    std::cout << "  Serial / Sync mode\n";
    std::cout << "========================================\n";

    Detector detector(cfg::PARAM_PATH, cfg::BIN_PATH);
    detector.confThreshold = cfg::CONF_THRESHOLD;
    detector.nmsThreshold  = cfg::NMS_THRESHOLD;
    detector.inputSize     = cfg::INPUT_SIZE;

    Camera cam(cfg::CAM_WIDTH, cfg::CAM_HEIGHT, cfg::CAM_FPS);
    if (!cam.open()) return 1;

    cv::namedWindow(cfg::WIN_NAME, cv::WINDOW_AUTOSIZE);

    cv::Mat frame;
    int frameCnt  = 0;
    int detectMs  = 0;
    float dispFps = 0.f;
    auto lastSec  = std::chrono::steady_clock::now();

    std::cout << "Running...  (display window: " << cfg::WIN_NAME
              << ")" << std::endl;

    while (true) {
        // ── 1. 取帧 ──
        if (!cam.read(frame) || frame.empty())
            continue;

        // ── 2. 检测 ──
        auto t0 = std::chrono::steady_clock::now();
        auto detections = detector.detect(frame);
        auto t1 = std::chrono::steady_clock::now();
        detectMs = (int)std::chrono::duration<float, std::milli>(
            t1 - t0).count();

        // ── 3. 画框 ──
        drawDetections(frame, detections);

        // ── 4. HUD ──
        ++frameCnt;
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastSec).count();
        if (dt >= 1.f) {
            dispFps = frameCnt / dt;
            frameCnt = 0;
            lastSec = now;
        }
        drawHud(frame, true, dispFps, detectMs);

        // ── 5. 显示 ──
        cv::imshow(cfg::WIN_NAME, frame);

        int key = cv::waitKey(cfg::WAITKEY_MS) & 0xFF;
        if (key == 'q' || key == 27)
            break;
    }

    cv::destroyWindow(cfg::WIN_NAME);
    cam.close();
    system("pkill -f 'rpicam-vid.*-o -' 2>/dev/null");
    std::cout << "Exit." << std::endl;
    return 0;
}
