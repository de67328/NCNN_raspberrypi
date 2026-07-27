#pragma once

#include <opencv2/core.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

// ═══════════════════════════════════════════════════════════════
// CSI 摄像头采集 — 受控 rpicam-vid 子进程 + MJPEG 最新帧缓存
// ═══════════════════════════════════════════════════════════════
class Camera {
public:
    Camera(int width, int height, int fps);
    ~Camera();

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    bool open();
    void close();

    /// 阻塞等待一张新 BGR 帧；采集进程退出或读取失败时返回 false。
    /// 后台只保留最新帧，因此推理较慢时不会顺序处理积压的旧帧。
    bool read(cv::Mat& frame);

private:
    bool decodeNextFrame(cv::Mat& frame);
    void captureLoop();

    int  width_, height_, fps_;
    int   fd_   = -1;
    int   childPid_ = -1;

    std::vector<uint8_t> streamBuf_;
    std::thread captureThread_;
    std::atomic<bool> stopRequested_{false};

    std::mutex frameMutex_;
    std::condition_variable frameReady_;
    cv::Mat latestFrame_;
    std::uint64_t producedGeneration_ = 0;
    std::uint64_t consumedGeneration_ = 0;
    bool captureRunning_ = false;
};
