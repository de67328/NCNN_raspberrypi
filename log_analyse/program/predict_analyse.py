#!/usr/bin/env python3
"""
predict_analyse.py — 钢球位置预测可行性分析 (v2)
仅评估连续检出段内的预测，避免跨长间隙导致的虚假大误差。
"""

import numpy as np
import pandas as pd
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

DATA_DIR   = Path(__file__).resolve().parent.parent / "data"
VISUAL_DIR = Path(__file__).resolve().parent.parent / "visual"
CSV_PATH   = DATA_DIR / "detect_processed.csv"
VISUAL_DIR.mkdir(parents=True, exist_ok=True)

df = pd.read_csv(CSV_PATH)
frames = df["frame"].values.astype(int)
x_meas = df["x"].values
y_meas = df["y"].values
N = len(frames)

fps = 13.0; dt = 1.0/fps
delay_frames = max(1, int(np.round(0.100 / dt)))
print(f"Detections: {N}  fps={fps}  delay≈{delay_frames}f")

# ── 找连续检出段 (frame gap ≤ 1) ──
segments = []
seg_start = 0
for i in range(1, N):
    if frames[i] - frames[i-1] > 1:
        if i - seg_start >= 4:
            segments.append((seg_start, i))
        seg_start = i
if N - seg_start >= 4:
    segments.append((seg_start, N))
print(f"Continuous segments (≥4): {len(segments)}")

# ── 帧间统计 ──
frame_gaps = []; x_deltas = []
for s, e in segments:
    for k in range(s, e-1):
        frame_gaps.append(frames[k+1] - frames[k])
        x_deltas.append(abs(x_meas[k+1] - x_meas[k]))
frame_gaps = np.array(frame_gaps); x_deltas = np.array(x_deltas)

# ── 预测器 ──
def pred_linear(x_hist, dt, lk):
    if len(x_hist) < 2: return np.nan
    v = (x_hist[-1] - x_hist[-2]) / dt
    return x_hist[-1] + v * lk * dt

def pred_quad(x_hist, dt, lk):
    if len(x_hist) < 3: return np.nan
    t = np.array([0.0, -dt, -2*dt])
    c = np.polyfit(t, x_hist[-3:], 2)
    return np.polyval(c, lk * dt)

# ── 评估 ──
def segment_errors(predictor, lk):
    errs = []
    for s, e in segments:
        for k in range(s + 2, e - lk):
            p = predictor(x_meas[s:k+1], dt, lk)
            if not np.isnan(p):
                errs.append(x_meas[k+lk] - p)
    return np.array(errs)

print("\n─ Prediction Error within Continuous Segments ─")
for lk in [1, 2]:
    e1 = segment_errors(pred_linear, lk)
    e2 = segment_errors(pred_quad, lk)
    print(f"  Linear  {lk}f: MAE={np.mean(np.abs(e1)):.1f} px  RMSE={np.sqrt(np.mean(e1**2)):.1f}  n={len(e1)}")
    print(f"  Quad    {lk}f: MAE={np.mean(np.abs(e2)):.1f} px  RMSE={np.sqrt(np.mean(e2**2)):.1f}  n={len(e2)}")

# ── 可视化 ──
fig, axes = plt.subplots(2, 3, figsize=(16, 10))
fig.suptitle("Ball Prediction Feasibility at 13 fps", fontsize=13, fontweight="bold")

# (a) 检出点 + 连续段标色
ax = axes[0,0]
for i, (s,e) in enumerate(segments):
    ax.plot(frames[s:e], x_meas[s:e], ".", color=plt.cm.tab10(i%10), ms=2)
ax.set(xlabel="Frame", ylabel="X [px]", title=f"Detections ({len(segments)} segments)")
ax.grid(alpha=0.3)

# (b) 帧间 |Δx| 分布
ax = axes[0,1]
ax.hist(x_deltas, bins=50, color="steelblue", edgecolor="white")
for v,c,l in [(5,"green","5px"),(10,"orange","10px"),(20,"red","20px")]:
    ax.axvline(v, color=c, ls="--", label=l)
ax.set(xlabel="|Δx| consecutive [px]", ylabel="Count", title="Frame-to-Frame |Δx|")
ax.legend(fontsize=7); ax.grid(alpha=0.3)

# (c) 预测误差直方图
ax = axes[0,2]
e1 = segment_errors(pred_linear, 1)
e2 = segment_errors(pred_linear, 2)
ax.hist(e1, bins=40, alpha=0.5, color="blue", label=f"1f (σ={np.std(e1):.0f})")
ax.hist(e2, bins=40, alpha=0.4, color="red", label=f"2f (σ={np.std(e2):.0f})")
ax.axvline(0, color="k", ls="--", lw=0.8)
ax.set(xlabel="Error [px]", title="Linear Prediction Error")
ax.legend(fontsize=8); ax.grid(alpha=0.3)

# (d) 最长段详情
ax = axes[1,0]
longest = max(segments, key=lambda seg: seg[1]-seg[0])
s,e = longest
ax.plot(frames[s:e], x_meas[s:e], "b.-", ms=2, lw=0.6)
ax.plot(frames[s:e], y_meas[s:e], "r.-", ms=2, lw=0.6, alpha=0.5, label="Y")
ax.set(xlabel="Frame", title=f"Longest Segment (f{frames[s]}–{frames[e-1]})")
ax.legend(fontsize=7); ax.grid(alpha=0.3)

# (e) 预测 vs 实测（最长段, 2f ahead）
ax = axes[1,1]
s,e = longest
seg_x = x_meas[s:e]; seg_f = frames[s:e]
preds_f, preds_v, actuals_f, actuals_v = [], [], [], []
for k in range(2, len(seg_x)-2):
    p = pred_linear(seg_x[:k+1], dt, 2)
    if not np.isnan(p):
        preds_f.append(seg_f[k]); preds_v.append(p)
        actuals_f.append(seg_f[k+2]); actuals_v.append(seg_x[k+2])
ax.plot(seg_f, seg_x, "k.-", ms=2, lw=0.5, alpha=0.4, label="Measured")
ax.plot(actuals_f, actuals_v, "go", ms=3, alpha=0.6, label="Actual (+2f)")
ax.plot(preds_f, preds_v, "rx", ms=3, alpha=0.6, label="Predicted (+2f)")
ax.set(xlabel="Frame", ylabel="X [px]", title="2-Frame Ahead: Predicted vs Actual")
ax.legend(fontsize=6, markerscale=2); ax.grid(alpha=0.3)

# (f) 误差 vs 帧间变化 散点图
ax = axes[1,2]
e1f = segment_errors(pred_linear, 1)
# 对齐帧间变化
xd_aligned = []
for s,e_ in segments:
    for k in range(s+2, e_-1):
        xd_aligned.append(abs(x_meas[k+1]-x_meas[k]))
xd_aligned = np.array(xd_aligned)
n_pts = min(len(e1f), len(xd_aligned))
ax.scatter(xd_aligned[:n_pts], np.abs(e1f[:n_pts]), s=3, alpha=0.4)
m = max(xd_aligned[:n_pts].max(), np.abs(e1f[:n_pts]).max())
ax.plot([0,m],[0,m],"r--",lw=0.8,label="error=|Δx|")
ax.set(xlabel="|Δx| consecutive [px]", ylabel="|1f prediction error| [px]")
ax.legend(fontsize=7); ax.grid(alpha=0.3)

plt.tight_layout()
plt.savefig(VISUAL_DIR / "prediction_feasibility.png", dpi=150)
plt.close()
print(f"\nSaved: {VISUAL_DIR / 'prediction_feasibility.png'}")

# ── 结论 ──
pct5  = 100*np.sum(x_deltas<=5)/len(x_deltas)
pct10 = 100*np.sum(x_deltas<=10)/len(x_deltas)
consec_pct = 100*np.sum(frame_gaps==1)/len(frame_gaps)
print(f"\n{'='*55}")
print(f"FEASIBILITY: {pct10:.0f}% of frame pairs have |Δx|≤10px")
print(f"  |Δx|≤5px: {pct5:.0f}%   ≤10px: {pct10:.0f}%")
print(f"  Consecutive frames: {consec_pct:.0f}%")
e1_all = segment_errors(pred_linear, 1)
print(f"  1f pred MAE: {np.mean(np.abs(e1_all)):.1f} px  "
      f"(baseline: median |Δx|={np.median(x_deltas):.1f} px)")
if pct10 > 70 and consec_pct > 60:
    print("  → VIABLE: prediction can compensate visual latency")
elif pct10 > 40:
    print("  → MARGINAL: works for slow motion, not for fast")
else:
    print("  → DIFFICULT: increase frame rate or add IMU")
print(f"{'='*55}")
