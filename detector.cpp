#include "detector.h"
#include "config.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include <ncnn/mat.h>
#include <opencv2/imgproc.hpp>

// ═══════════════════════════════════════════════════════════════
Detector::Detector(const std::string& paramPath, const std::string& binPath)
{
    net_.opt.num_threads = cfg::NUM_THREADS;

    if (net_.load_param(paramPath.c_str()) != 0)
        throw std::runtime_error("load_param: " + paramPath);
    if (net_.load_model(binPath.c_str()) != 0)
        throw std::runtime_error("load_model: " + binPath);

    const auto inputNames = net_.input_names();
    const auto outputNames = net_.output_names();
    if (inputNames.empty() || outputNames.empty())
        throw std::runtime_error("NCNN model has no input or output blob");

    inputBlobName_ = inputNames.front();
    outputBlobName_ = outputNames.front();

    std::cout << "[Detector] loaded (" << cfg::NUM_THREADS << " threads)"
              << ", input=" << inputBlobName_
              << ", output=" << outputBlobName_
              << std::endl;
}

// ═══════════════════════════════════════════════════════════════
ncnn::Mat Detector::preprocess(const cv::Mat& bgr,
                               float& scaleX, float& scaleY)
{
    if (inputWidth <= 0 || inputHeight <= 0)
        throw std::runtime_error("invalid model input size");

    // 与 320x96 训练数据保持一致：直接非等比缩放，不做 letterbox，
    // 因此横纵方向必须分别记录缩放比例。
    scaleX = static_cast<float>(inputWidth) / bgr.cols;
    scaleY = static_cast<float>(inputHeight) / bgr.rows;

    ncnn::Mat in = ncnn::Mat::from_pixels_resize(
        bgr.data, ncnn::Mat::PIXEL_BGR2RGB,
        bgr.cols, bgr.rows, inputWidth, inputHeight);

    const float norm[3] = { 1.f / 255.f, 1.f / 255.f, 1.f / 255.f };
    in.substract_mean_normalize(nullptr, norm);
    return in;
}

// ═══════════════════════════════════════════════════════════════
std::vector<Detection> Detector::detect(const cv::Mat& bgr)
{
    if (bgr.empty())
        return {};

    // ── ROI 遮罩：ROI 外区域填充纯色，不改图像尺寸 ──
    cv::Mat work;
    const int bottom = (roiBottom > 0) ? roiBottom : bgr.rows;
    const bool useRoi = (roiTop > 0 || bottom < bgr.rows);
    if (useRoi) {
        work = bgr.clone();
        const cv::Scalar fill(roiFillB, roiFillG, roiFillR);
        if (roiTop > 0)
            cv::rectangle(work, cv::Rect(0, 0, bgr.cols, roiTop), fill, cv::FILLED);
        if (bottom < bgr.rows)
            cv::rectangle(work, cv::Rect(0, bottom, bgr.cols, bgr.rows - bottom), fill, cv::FILLED);
    } else {
        work = bgr;  // 浅拷贝，无额外开销
    }

    float scaleX = 1.f, scaleY = 1.f;
    ncnn::Mat in = preprocess(work, scaleX, scaleY);

    ncnn::Extractor ex = net_.create_extractor();
    if (ex.input(inputBlobName_.c_str(), in) != 0)
        throw std::runtime_error("NCNN failed to set input blob: " + inputBlobName_);

    ncnn::Mat out;
    if (ex.extract(outputBlobName_.c_str(), out) != 0)
        throw std::runtime_error("NCNN failed to extract output blob: " + outputBlobName_);

    return postprocess(out, bgr.rows, bgr.cols, scaleX, scaleY);
}

// ═══════════════════════════════════════════════════════════════
// 三种 ncnn 输出格式自动适配: 通道分离 / 行分离 / 打包
// ═══════════════════════════════════════════════════════════════
std::vector<Detection> Detector::postprocess(
    const ncnn::Mat& output, int origH, int origW,
    float scaleX, float scaleY)
{
    int nc = output.c, nh = output.h, nw = output.w;

    int strideDim = 0, numAnchors = 0;

    if (nc >= 5 && nh == 1)        { strideDim = nc; numAnchors = nw; }
    else if (nc == 1 && nh >= 5)   { strideDim = nh; numAnchors = nw; }
    else if (nc == 1 && nh == 1) {
        for (int s : {5, 6, 7, 14, 84, 85})
            if (nw % s == 0) { strideDim = s; numAnchors = nw / s; break; }
    }
    if (strideDim == 0) return {};

    static bool logged = false;
    if (!logged) {
        const char* fmt = (nc >= 5) ? "channel" : (nh >= 5) ? "row" : "packed";
        std::cout << "[Detector] " << fmt << " format"
                  << " stride=" << strideDim
                  << " anchors=" << numAnchors
                  << " classes=" << (strideDim - 4) << std::endl;
        logged = true;
    }

    std::vector<Detection> cand;

    for (int i = 0; i < numAnchors; ++i) {
        float cx, cy, bw, bh, bestScore = 0.f;
        int   bestCls = 0;

        if (nc >= 5) {            // 通道分离
            cx = output.channel(0)[i]; cy = output.channel(1)[i];
            bw = output.channel(2)[i]; bh = output.channel(3)[i];
            for (int k = 0; k < strideDim - 4; ++k)
            { float s = output.channel(4 + k)[i]; if (s > bestScore) { bestScore = s; bestCls = k; } }
        } else if (nh >= 5) {     // 行分离
            cx = output.row(0)[i]; cy = output.row(1)[i];
            bw = output.row(2)[i]; bh = output.row(3)[i];
            for (int k = 0; k < strideDim - 4; ++k)
            { float s = output.row(4 + k)[i]; if (s > bestScore) { bestScore = s; bestCls = k; } }
        } else {                  // 打包
            int base = i * strideDim;
            cx = output.channel(0)[base];
            cy = output.channel(0)[base + 1];
            bw = output.channel(0)[base + 2];
            bh = output.channel(0)[base + 3];
            for (int k = 0; k < strideDim - 4; ++k)
            { float s = output.channel(0)[base + 4 + k]; if (s > bestScore) { bestScore = s; bestCls = k; } }
        }

        if (bestScore < confThreshold) continue;

        // YOLOv8 输出是 320x96 拉伸图上的像素坐标。横纵方向
        // 使用各自的比例还原到原始摄像头画面。
        float x1 = (cx - bw * 0.5f) / scaleX;
        float y1 = (cy - bh * 0.5f) / scaleY;
        float x2 = (cx + bw * 0.5f) / scaleX;
        float y2 = (cy + bh * 0.5f) / scaleY;
        x1 = std::clamp(x1, 0.f, static_cast<float>(origW));
        y1 = std::clamp(y1, 0.f, static_cast<float>(origH));
        x2 = std::clamp(x2, 0.f, static_cast<float>(origW));
        y2 = std::clamp(y2, 0.f, static_cast<float>(origH));

        const int boxW = std::max(0, static_cast<int>(std::round(x2 - x1)));
        const int boxH = std::max(0, static_cast<int>(std::round(y2 - y1)));
        if (boxW == 0 || boxH == 0) continue;

        cand.push_back({bestCls, bestScore,
                        cv::Rect(static_cast<int>(std::round(x1)),
                                 static_cast<int>(std::round(y1)),
                                 boxW, boxH)});
    }

    std::vector<int> keep = nms(cand, nmsThreshold);
    std::vector<Detection> result;
    for (int idx : keep) result.push_back(cand[idx]);
    return result;
}

// ═══════════════════════════════════════════════════════════════
std::vector<int> Detector::nms(const std::vector<Detection>& dets,
                                float iouThresh)
{
    std::vector<int> idx(dets.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        return dets[a].confidence > dets[b].confidence; });

    std::vector<int> keep;
    while (!idx.empty()) {
        int cur = idx[0]; keep.push_back(cur);
        if (idx.size() == 1) break;
        const cv::Rect& A = dets[cur].box;
        std::vector<int> rem;
        for (size_t i = 1; i < idx.size(); ++i) {
            const cv::Rect& B = dets[idx[i]].box;
            if (dets[cur].cls_id != dets[idx[i]].cls_id) {
                rem.push_back(idx[i]);
                continue;
            }
            int ix1 = std::max(A.x, B.x), iy1 = std::max(A.y, B.y);
            int ix2 = std::min(A.x + A.width,  B.x + B.width);
            int iy2 = std::min(A.y + A.height, B.y + B.height);
            int iw = std::max(0, ix2 - ix1), ih = std::max(0, iy2 - iy1);
            float inter = (float)(iw * ih);
            float iou = inter / (A.area() + B.area() - inter + 1e-6f);
            if (iou <= iouThresh) rem.push_back(idx[i]);
        }
        idx = rem;
    }
    return keep;
}
