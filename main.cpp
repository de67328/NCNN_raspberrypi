/**
 * main.cpp — 低延迟钢珠检测 (C++17)
 *
 * 管线: 后台采集最新帧 → 检测 → 画框 → 显示
 * 用法: ./ball_detect
 */

#include "config.h"
#include "detector.h"
#include "camera.h"
#include "visual.h"

#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

// SIGINT 标志 — Ctrl+C 优雅退出
static std::sig_atomic_t gExitFlag = 0;
static void onSignal(int) { gExitFlag = 1; }

// ═══════════════════════════════════════════════════════════════
int main()
{
    std::cout << "========================================\n";
    std::cout << "  YOLOv8n NCNN Steel Ball Detection\n";
    std::cout << "  Latest-frame / Low-latency mode\n";
    std::cout << "========================================\n";

    try {
        Detector detector(cfg::PARAM_PATH, cfg::BIN_PATH);
        detector.confThreshold = cfg::CONF_THRESHOLD;
        detector.nmsThreshold  = cfg::NMS_THRESHOLD;
        detector.inputWidth    = cfg::INPUT_WIDTH;
        detector.inputHeight   = cfg::INPUT_HEIGHT;

        Camera cam(cfg::CAM_WIDTH, cfg::CAM_HEIGHT, cfg::CAM_FPS);
        if (!cam.open()) return 1;

        cv::namedWindow(cfg::WIN_NAME, cv::WINDOW_AUTOSIZE);

        // 注册信号处理
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);

        cv::Mat frame;
        int frameCnt  = 0;
        int detectMs  = 0;
        float dispFps = 0.f;
        auto lastSec  = std::chrono::steady_clock::now();

        std::cout << "Running...  (display window: " << cfg::WIN_NAME
                  << ")" << std::endl;

        bool cameraFailed = false;
        while (true) {
            // ── 1. 取帧 ──
            if (!cam.read(frame) || frame.empty()) {
                std::cerr << "[Camera] 采集已停止，程序退出" << std::endl;
                cameraFailed = true;
                break;
            }

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
            if (cfg::DISPLAY_SCALE > 0.f && cfg::DISPLAY_SCALE < 1.f) {
                cv::Mat display;
                cv::resize(frame, display, cv::Size(), cfg::DISPLAY_SCALE, cfg::DISPLAY_SCALE);
                cv::imshow(cfg::WIN_NAME, display);
            } else {
                cv::imshow(cfg::WIN_NAME, frame);
            }

            // 多重退出: q/ESC 按键 / Ctrl+C / 窗口关闭
            int key = cv::waitKey(cfg::WAITKEY_MS) & 0xFF;
            int key2 = cv::pollKey();  // 补充抓取 (OpenCV≥4.5)
            if (key2 >= 0) key = key2;

            bool windowClosed = (cv::getWindowProperty(cfg::WIN_NAME, cv::WND_PROP_VISIBLE) < 1.0);
            if (gExitFlag || key == 'q' || key == 27 || windowClosed)
                break;
        }

        cv::destroyWindow(cfg::WIN_NAME);
        cam.close();
        std::cout << "Exit." << std::endl;
        return cameraFailed ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << std::endl;
        return 1;
    }
}
