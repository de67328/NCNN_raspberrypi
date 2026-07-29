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
    int   inputWidth    = 320;
    int   inputHeight   = 96;

    // ROI 遮罩: 只识别 [roiTop, roiBottom) 横条带内的目标
    // 设为 0 和图像高度 = 全图（关闭 ROI）
    int   roiTop       = 0;       // ROI 上边沿 y (原始图像坐标)
    int   roiBottom    = 0;       // ROI 下边沿 y；0=自动取图像高度
    int   roiFillB     = 0;       // ROI 外填充色 B
    int   roiFillG     = 0;
    int   roiFillR     = 0;

private:
    ncnn::Mat preprocess(const cv::Mat& bgr,
                         float& scaleX, float& scaleY);
    std::vector<Detection> postprocess(const ncnn::Mat& output,
                                       int origH, int origW,
                                       float scaleX, float scaleY);
    static std::vector<int> nms(const std::vector<Detection>& dets,
                                float iouThresh);

    ncnn::Net net_;
    std::string inputBlobName_;
    std::string outputBlobName_;
};
