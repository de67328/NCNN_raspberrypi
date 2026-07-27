#include "visual.h"

#include <algorithm>
#include <cstdio>

#include <opencv2/imgproc.hpp>

static const cv::Scalar BALL_COLOR(0, 255, 0);    // 绿色 (BGR)
static const cv::Scalar CENTER_COLOR(0, 0, 255);  // 红色

// ═══════════════════════════════════════════════════════════════
void drawDetections(cv::Mat& image, const std::vector<Detection>& dets)
{
    for (const auto& d : dets) {
        cv::rectangle(image, d.box, BALL_COLOR, 2, cv::LINE_AA);

        // 圆心十字
        int cx = d.box.x + d.box.width / 2;
        int cy = d.box.y + d.box.height / 2;
        cv::drawMarker(image, cv::Point(cx, cy), CENTER_COLOR,
                       cv::MARKER_CROSS, 10, 2, cv::LINE_AA);

        // 标签
        char label[32];
        snprintf(label, sizeof(label), "ball %.2f", d.confidence);
        int baseline = 0;
        cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX,
                                      0.5, 2, &baseline);

        int ly = std::max(d.box.y, ts.height + 6);
        cv::rectangle(image,
                      cv::Point(d.box.x, ly - ts.height - 4),
                      cv::Point(d.box.x + ts.width + 4, ly + 2),
                      cv::Scalar(0, 0, 0), cv::FILLED);
        cv::putText(image, label, cv::Point(d.box.x + 2, ly),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    BALL_COLOR, 2, cv::LINE_AA);
    }
}

// ═══════════════════════════════════════════════════════════════
void drawHud(cv::Mat& image, bool detecting, float fps, int detectMs)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s  %.0ffps  det:%dms",
             detecting ? "ON" : "OFF", fps, detectMs);

    cv::putText(image, buf, cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.55,
                cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

    if (detecting) {
        // 右上角: 模型输入尺寸提示
        cv::putText(image, "YOLOv8n 320x320",
                    cv::Point(image.cols - 200, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(180, 180, 180), 1, cv::LINE_AA);
    }
}
