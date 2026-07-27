#include "detector.h"
#include "config.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

// ═══════════════════════════════════════════════════════════════
Detector::Detector(const std::string& paramPath, const std::string& binPath)
{
    net_.opt.num_threads = cfg::NUM_THREADS;

    if (net_.load_param(paramPath.c_str()) != 0)
        throw std::runtime_error("load_param: " + paramPath);
    if (net_.load_model(binPath.c_str()) != 0)
        throw std::runtime_error("load_model: " + binPath);

    std::cout << "[Detector] loaded (" << cfg::NUM_THREADS << " threads)"
              << std::endl;
}

// ═══════════════════════════════════════════════════════════════
ncnn::Mat Detector::preprocess(const cv::Mat& bgr, int& origH, int& origW)
{
    origH = bgr.rows;
    origW = bgr.cols;

    ncnn::Mat in = ncnn::Mat::from_pixels_resize(
        bgr.data, ncnn::Mat::PIXEL_BGR2RGB,
        bgr.cols, bgr.rows, inputSize, inputSize);

    const float norm[3] = { 1.f / 255.f, 1.f / 255.f, 1.f / 255.f };
    in.substract_mean_normalize(nullptr, norm);
    return in;
}

// ═══════════════════════════════════════════════════════════════
std::vector<Detection> Detector::detect(const cv::Mat& bgr)
{
    int origH = 0, origW = 0;
    ncnn::Mat in = preprocess(bgr, origH, origW);

    ncnn::Extractor ex = net_.create_extractor();
    ex.input("images", in);

    ncnn::Mat out;
    ex.extract("output0", out);

    return postprocess(out, origH, origW);
}

// ═══════════════════════════════════════════════════════════════
// 三种 ncnn 输出格式自动适配: 通道分离 / 行分离 / 打包
// ═══════════════════════════════════════════════════════════════
std::vector<Detection> Detector::postprocess(
    const ncnn::Mat& output, int origH, int origW)
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

        float x1 = cx - bw * 0.5f, y1 = cy - bh * 0.5f;
        float x2 = cx + bw * 0.5f, y2 = cy + bh * 0.5f;
        x1 = std::max(0.f, std::min(x1, 1.f));
        y1 = std::max(0.f, std::min(y1, 1.f));
        x2 = std::max(0.f, std::min(x2, 1.f));
        y2 = std::max(0.f, std::min(y2, 1.f));

        cand.push_back({bestCls, bestScore,
                        cv::Rect((int)(x1 * origW), (int)(y1 * origH),
                                 (int)((x2 - x1) * origW),
                                 (int)((y2 - y1) * origH))});
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
