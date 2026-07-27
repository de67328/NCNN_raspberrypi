#include "camera.h"

#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <opencv2/imgcodecs.hpp>

// ═══════════════════════════════════════════════════════════════
Camera::Camera(int width, int height, int fps)
    : width_(width), height_(height), fps_(fps),
      buf_(2 * 1024 * 1024) {}

Camera::~Camera() { close(); }

// ═══════════════════════════════════════════════════════════════
bool Camera::open()
{
    std::string cmd = "rpicam-vid -t 0 --width "
        + std::to_string(width_)
        + " --height " + std::to_string(height_)
        + " --codec mjpeg --framerate " + std::to_string(fps_)
        + " --denoise cdn_off --nopreview -o - 2>/dev/null";

    pipe_ = popen(cmd.c_str(), "r");
    if (!pipe_) {
        std::cerr << "[Camera] rpicam-vid 启动失败" << std::endl;
        return false;
    }
    fd_ = fileno(pipe_);
    setvbuf(pipe_, nullptr, _IOFBF, 256 * 1024);
    std::cout << "[Camera] " << width_ << "x" << height_
              << " @" << fps_ << "fps" << std::endl;
    return true;
}

void Camera::close()
{
    if (pipe_) { pclose(pipe_); pipe_ = nullptr; fd_ = -1; }
}

// ═══════════════════════════════════════════════════════════════
// 零拷贝 MJPEG 解析: 在缓冲区指针上找 SOI/EOI → imdecode
// ═══════════════════════════════════════════════════════════════
bool Camera::read(cv::Mat& frame)
{
    if (!pipe_) return false;

    while (true) {
        if (bufPos_ >= bufLen_) {
            bufLen_ = fread(buf_.data(), 1, buf_.size(), pipe_);
            bufPos_ = 0;
            if (bufLen_ == 0) return false;
        }

        // 找 SOI (FF D8)
        size_t soi = bufPos_;
        while (soi < bufLen_ - 1 &&
               !(buf_[soi] == 0xFF && buf_[soi + 1] == 0xD8))
            ++soi;
        if (soi >= bufLen_ - 1) { bufPos_ = bufLen_; continue; }

        // 找 EOI (FF D9)
        size_t eoi = soi + 2;
        while (eoi < bufLen_ - 1 &&
               !(buf_[eoi] == 0xFF && buf_[eoi + 1] == 0xD9))
            ++eoi;

        if (eoi >= bufLen_ - 1) {
            // 跨边界 → 挪尾部到开头，续读
            size_t tail = bufLen_ - soi;
            memmove(buf_.data(), buf_.data() + soi, tail);
            bufLen_ = tail + fread(buf_.data() + tail, 1,
                                   buf_.size() - tail, pipe_);
            bufPos_ = 0;
            if (bufLen_ <= tail) return false;
            continue;
        }

        // 零拷贝解码
        cv::Mat raw(1, (int)(eoi - soi + 2), CV_8UC1, buf_.data() + soi);
        frame = cv::imdecode(raw, cv::IMREAD_COLOR);
        bufPos_ = eoi + 2;
        return !frame.empty();
    }
}
