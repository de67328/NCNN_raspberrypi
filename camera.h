#pragma once

#include <opencv2/core.hpp>
#include <cstdio>
#include <vector>

// ═══════════════════════════════════════════════════════════════
// CSI 摄像头采集 — popen rpicam-vid MJPEG → imdecode
// ═══════════════════════════════════════════════════════════════
class Camera {
public:
    Camera(int width, int height, int fps);
    ~Camera();

    bool open();
    void close();

    /// 阻塞读取一帧 BGR，失败返回 false
    bool read(cv::Mat& frame);

private:
    int  width_, height_, fps_;
    FILE* pipe_ = nullptr;
    int   fd_   = -1;

    std::vector<uint8_t> buf_;
    size_t bufPos_ = 0;
    size_t bufLen_ = 0;
};
