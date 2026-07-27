#pragma once

#include <opencv2/core.hpp>
#include <ncnn/net.h>
#include <vector>
#include <string>

// ═══════════════════════════════════════════════════════════════
// 检测结果
// ═══════════════════════════════════════════════════════════════
struct Detection {
    int      cls_id;
    float    confidence;
    cv::Rect box;
};

// ═══════════════════════════════════════════════════════════════
// YOLOv8n NCNN 检测器
// ═══════════════════════════════════════════════════════════════
class Detector {
public:
    Detector(const std::string& paramPath, const std::string& binPath);

    /// 同步推理一张 BGR 图像
    std::vector<Detection> detect(const cv::Mat& bgr);

    // 参数
    float confThreshold = 0.25f;
    float nmsThreshold  = 0.45f;
    int   inputSize     = 320;

private:
    ncnn::Mat preprocess(const cv::Mat& bgr, float& scale,
                         int& padLeft, int& padTop);
    std::vector<Detection> postprocess(const ncnn::Mat& output,
                                       int origH, int origW,
                                       float scale, int padLeft, int padTop);
    static std::vector<int> nms(const std::vector<Detection>& dets,
                                float iouThresh);

    ncnn::Net net_;
    std::string inputBlobName_;
    std::string outputBlobName_;
};
