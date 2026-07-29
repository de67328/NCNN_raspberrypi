#include "camera.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <opencv2/imgcodecs.hpp>

namespace {

constexpr std::size_t READ_CHUNK_SIZE = 64 * 1024;
constexpr std::size_t MAX_JPEG_SIZE = 2 * 1024 * 1024;
constexpr auto FIRST_FRAME_TIMEOUT = std::chrono::seconds(5);

const std::array<std::uint8_t, 2> JPEG_SOI = {0xFF, 0xD8};
const std::array<std::uint8_t, 2> JPEG_EOI = {0xFF, 0xD9};

}  // namespace

// ═══════════════════════════════════════════════════════════════
Camera::Camera(int width, int height, int fps,
               int shutterUs, float gain)
    : width_(width), height_(height), fps_(fps),
      shutterUs_(shutterUs), gain_(gain)
{
    streamBuf_.reserve(READ_CHUNK_SIZE * 2);
}

Camera::~Camera()
{
    close();
}

// ═══════════════════════════════════════════════════════════════
bool Camera::open()
{
    if (captureThread_.joinable() || childPid_ > 0)
        return true;

    int pipeFds[2] = {-1, -1};
    if (::pipe(pipeFds) != 0) {
        std::cerr << "[Camera] 创建管道失败: " << std::strerror(errno)
                  << std::endl;
        return false;
    }

    const std::string width = std::to_string(width_);
    const std::string height = std::to_string(height_);
    const std::string fps = std::to_string(fps_);

    const pid_t pid = ::fork();
    if (pid < 0) {
        std::cerr << "[Camera] 创建 rpicam-vid 进程失败: "
                  << std::strerror(errno) << std::endl;
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
        return false;
    }

    if (pid == 0) {
        ::close(pipeFds[0]);
        if (::dup2(pipeFds[1], STDOUT_FILENO) < 0)
            ::_exit(126);
        ::close(pipeFds[1]);

        const int nullFd = ::open("/dev/null", O_WRONLY);
        if (nullFd >= 0) {
            ::dup2(nullFd, STDERR_FILENO);
            ::close(nullFd);
        }

        ::execlp("rpicam-vid", "rpicam-vid",
                 "-t", "0",
                 "--width", width.c_str(),
                 "--height", height.c_str(),
                 "--codec", "mjpeg",
                 "--framerate", fps.c_str(),
                 "--shutter", std::to_string(shutterUs_).c_str(),
                 "--gain", std::to_string(gain_).c_str(),
                 "--denoise", "cdn_off",
                 "--nopreview",
                 "-o", "-",
                 static_cast<char*>(nullptr));
        ::_exit(127);
    }

    ::close(pipeFds[1]);
    fd_ = pipeFds[0];
    childPid_ = static_cast<int>(pid);
    stopRequested_.store(false);
    streamBuf_.clear();

    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        latestFrame_.release();
        producedGeneration_ = 0;
        consumedGeneration_ = 0;
        captureRunning_ = true;
    }

    captureThread_ = std::thread(&Camera::captureLoop, this);

    // 等待首帧，确认 rpicam-vid 和相机都真正可用。
    bool ready = false;
    {
        std::unique_lock<std::mutex> lock(frameMutex_);
        ready = frameReady_.wait_for(lock, FIRST_FRAME_TIMEOUT, [this] {
            return producedGeneration_ > 0 || !captureRunning_;
        });
        ready = ready && producedGeneration_ > 0;
    }

    if (!ready) {
        std::cerr << "[Camera] 5 秒内未收到画面，请检查 rpicam-vid 和 CSI 相机"
                  << std::endl;
        close();
        return false;
    }

    std::cout << "[Camera] " << width_ << "x" << height_
              << " @" << fps_ << "fps (latest-frame mode)" << std::endl;
    return true;
}

void Camera::close()
{
    stopRequested_.store(true);

    // 先结束生产者，使阻塞的 read() 因管道关闭而返回。
    if (childPid_ > 0) {
        const pid_t pid = static_cast<pid_t>(childPid_);
        ::kill(pid, SIGTERM);

        bool reaped = false;
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < deadline) {
            int status = 0;
            const pid_t result = ::waitpid(pid, &status, WNOHANG);
            if (result == pid || (result < 0 && errno == ECHILD)) {
                reaped = true;
                break;
            }
            if (result < 0 && errno != EINTR)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!reaped) {
            ::kill(pid, SIGKILL);
            int status = 0;
            while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
        }
        childPid_ = -1;
    }

    if (captureThread_.joinable())
        captureThread_.join();

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }

    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        captureRunning_ = false;
        latestFrame_.release();
    }
    frameReady_.notify_all();
}

// ═══════════════════════════════════════════════════════════════
// 从字节流中提取完整 JPEG。查找不到 SOI 时保留末尾 0xFF，
// 因而 FF | D8 和 FF | D9 跨 read() 边界也能正确拼接。
// ═══════════════════════════════════════════════════════════════
bool Camera::decodeNextFrame(cv::Mat& frame)
{
    std::array<std::uint8_t, READ_CHUNK_SIZE> chunk{};

    while (!stopRequested_.load()) {
        auto soi = std::search(streamBuf_.begin(), streamBuf_.end(),
                               JPEG_SOI.begin(), JPEG_SOI.end());

        if (soi == streamBuf_.end()) {
            const bool keepTrailingFF =
                !streamBuf_.empty() && streamBuf_.back() == 0xFF;
            streamBuf_.clear();
            if (keepTrailingFF)
                streamBuf_.push_back(0xFF);
        } else if (soi != streamBuf_.begin()) {
            streamBuf_.erase(streamBuf_.begin(), soi);
        }

        if (streamBuf_.size() >= 2) {
            auto eoi = std::search(streamBuf_.begin() + 2, streamBuf_.end(),
                                   JPEG_EOI.begin(), JPEG_EOI.end());
            if (eoi != streamBuf_.end()) {
                const std::size_t jpegSize =
                    static_cast<std::size_t>(eoi - streamBuf_.begin()) + 2;
                cv::Mat encoded(1, static_cast<int>(jpegSize), CV_8UC1,
                                streamBuf_.data());
                frame = cv::imdecode(encoded, cv::IMREAD_COLOR);
                streamBuf_.erase(streamBuf_.begin(),
                                 streamBuf_.begin() + jpegSize);
                if (!frame.empty())
                    return true;

                // 单帧损坏不等同于相机死亡，继续寻找下一张完整 JPEG。
                continue;
            }
        }

        if (streamBuf_.size() >= MAX_JPEG_SIZE) {
            std::cerr << "[Camera] JPEG 帧超过 2 MiB，丢弃损坏数据"
                      << std::endl;
            streamBuf_.clear();
        }

        const ssize_t bytesRead = ::read(fd_, chunk.data(), chunk.size());
        if (bytesRead > 0) {
            streamBuf_.insert(streamBuf_.end(), chunk.begin(),
                              chunk.begin() + bytesRead);
            continue;
        }
        if (bytesRead < 0 && errno == EINTR)
            continue;
        return false;
    }

    return false;
}

void Camera::captureLoop()
{
    cv::Mat frame;
    while (!stopRequested_.load() && decodeNextFrame(frame)) {
        {
            std::lock_guard<std::mutex> lock(frameMutex_);
            latestFrame_ = frame;
            ++producedGeneration_;
        }
        frameReady_.notify_one();
    }

    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        captureRunning_ = false;
    }
    frameReady_.notify_all();
}

bool Camera::read(cv::Mat& frame)
{
    std::unique_lock<std::mutex> lock(frameMutex_);
    frameReady_.wait(lock, [this] {
        return producedGeneration_ > consumedGeneration_ || !captureRunning_;
    });

    if (producedGeneration_ <= consumedGeneration_)
        return false;

    latestFrame_.copyTo(frame);
    consumedGeneration_ = producedGeneration_;
    return !frame.empty();
}
