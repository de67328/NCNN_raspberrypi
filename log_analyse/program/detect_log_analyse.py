#!/usr/bin/env python3
"""
detect_log_analyse.py — 检测日志处理
1. 解析 detect_log.txt
2. 验证 y 坐标稳定性（钢珠纵坐标应基本不变）
3. 提取主钢球轨迹
4. 导出 CSV
"""

import re
import numpy as np
import pandas as pd
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ── 路径 ──
DATA_DIR  = Path(__file__).resolve().parent.parent / "data"
VISUAL_DIR = Path(__file__).resolve().parent.parent / "visual"
LOG_PATH  = DATA_DIR / "detect_log.txt"
CSV_PATH  = DATA_DIR / "detect_processed.csv"

VISUAL_DIR.mkdir(parents=True, exist_ok=True)

# ═══════════════════════════════════════════════════════════
# 1. 解析日志
# ═══════════════════════════════════════════════════════════
def parse_log(path: Path) -> pd.DataFrame:
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split(",")]
            if len(parts) < 7:
                # 空帧（无检出）
                frame = int(parts[0])
                rows.append([frame, np.nan, np.nan, np.nan, np.nan, np.nan, np.nan])
            else:
                frame, x, y, w, h, conf, cls = parts
                rows.append([
                    int(frame),
                    float(x), float(y),
                    float(w), float(h),
                    float(conf), int(cls)
                ])
    df = pd.DataFrame(rows, columns=["frame","x","y","w","h","conf","cls"])
    return df

print("Parsing log...")
df = parse_log(LOG_PATH)
print(f"  Total rows: {len(df)}")
print(f"  Frames:     {df['frame'].min()} – {df['frame'].max()}")
print(f"  Detections: {df['x'].notna().sum()}")

# ═══════════════════════════════════════════════════════════
# 2. 按帧聚合（同一帧多检出→取置信度最高的）
# ═══════════════════════════════════════════════════════════
df_valid = df.dropna(subset=["x"]).copy()
# 每帧选置信度最高的检测
df_best = df_valid.loc[df_valid.groupby("frame")["conf"].idxmax()].copy()
df_best = df_best.sort_values("frame").reset_index(drop=True)

print(f"\n  Best-per-frame detections: {len(df_best)}")
print(f"  x range: {df_best['x'].min():.0f} – {df_best['x'].max():.0f} px")
print(f"  y range: {df_best['y'].min():.0f} – {df_best['y'].max():.0f} px")

# ═══════════════════════════════════════════════════════════
# 3. Y 坐标稳定性验证
# ═══════════════════════════════════════════════════════════
y_mean = df_best["y"].mean()
y_std  = df_best["y"].std()
print(f"\n  Y-coordinate stats: mean={y_mean:.1f} px, std={y_std:.1f} px")
print(f"  Y variation: {df_best['y'].max() - df_best['y'].min():.0f} px")

# ── 图1: Y坐标时序 ──
fig, axes = plt.subplots(2, 2, figsize=(14, 10))
fig.suptitle("Detection Log Analysis", fontsize=14, fontweight="bold")

ax = axes[0, 0]
ax.scatter(df_best["frame"], df_best["y"], s=2, alpha=0.6, c="steelblue")
ax.axhline(y_mean, color="red", linestyle="--", label=f"mean={y_mean:.1f}")
ax.fill_between([df_best["frame"].min(), df_best["frame"].max()],
                y_mean - 2*y_std, y_mean + 2*y_std, alpha=0.15, color="red")
ax.set_xlabel("Frame")
ax.set_ylabel("Y [px]")
ax.set_title("Ball Y-coordinate Over Time")
ax.legend(fontsize=8)
ax.grid(True, alpha=0.3)

# ── 图2: Y坐标分布直方图 ──
ax = axes[0, 1]
ax.hist(df_best["y"], bins=40, color="steelblue", edgecolor="white", alpha=0.8)
ax.axvline(y_mean, color="red", linestyle="--")
ax.set_xlabel("Y [px]")
ax.set_ylabel("Count")
ax.set_title(f"Y Distribution (std={y_std:.1f} px)")
ax.grid(True, alpha=0.3)

# ── 图3: X坐标时序（主轨迹）──
ax = axes[1, 0]
ax.scatter(df_best["frame"], df_best["x"], s=3, c="darkgreen", alpha=0.7)
ax.set_xlabel("Frame")
ax.set_ylabel("X [px]")
ax.set_title("Ball X-coordinate Over Time (trajectory)")
ax.grid(True, alpha=0.3)

# ── 图4: 置信度时序 ──
ax = axes[1, 1]
ax.scatter(df_best["frame"], df_best["conf"], s=2, c="orange", alpha=0.6)
ax.set_xlabel("Frame")
ax.set_ylabel("Confidence")
ax.set_title("Detection Confidence")
ax.set_ylim(0, 1.05)
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(VISUAL_DIR / "detect_overview.png", dpi=150)
plt.close()
print(f"\n  Saved: {VISUAL_DIR / 'detect_overview.png'}")

# ═══════════════════════════════════════════════════════════
# 4. 导出 CSV
# ═══════════════════════════════════════════════════════════
# 导出每个有效检测帧的完整信息
cols_out = ["frame", "x", "y", "w", "h", "conf"]
df_best[cols_out].to_csv(CSV_PATH, index=False, float_format="%.2f")
print(f"  Saved: {CSV_PATH}  ({len(df_best)} rows)")

# 同时导出一个插值后的均匀时间序列（用于预测分析）
# 填充缺失帧
all_frames = pd.DataFrame({"frame": range(df["frame"].min(), df["frame"].max() + 1)})
df_interp = all_frames.merge(df_best[["frame", "x", "y", "conf"]], on="frame", how="left")
# 线性插值 x, y
df_interp["x"] = df_interp["x"].interpolate(method="linear", limit=5)
df_interp["y"] = df_interp["y"].interpolate(method="linear", limit=5)
df_interp["detected"] = df_interp["conf"].notna().astype(int)
df_interp["conf"] = df_interp["conf"].fillna(0)

interp_path = DATA_DIR / "detect_interpolated.csv"
df_interp.to_csv(interp_path, index=False, float_format="%.2f")
print(f"  Saved: {interp_path}  ({len(df_interp)} rows, with interpolation)")

print("\nDone.")
