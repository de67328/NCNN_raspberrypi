"""把 Ultralytics .pt 模型导出并整理为本工程使用的 NCNN 文件。"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from ultralytics import YOLO


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export YOLO weights to NCNN")
    parser.add_argument("--weights", type=Path, required=True, help="Trained best.pt")
    parser.add_argument(
        "--imgsz",
        type=int,
        nargs=2,
        metavar=("HEIGHT", "WIDTH"),
        default=(96, 320),
        help="Fixed model input in Ultralytics HEIGHT WIDTH order",
    )
    parser.add_argument("--output", type=Path, default=Path("model"))
    parser.add_argument(
        "--name",
        default="steel_ball_yolov8n_320x96_b1_fp32_best",
        help="Output basename without extension",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    weights = args.weights.expanduser().resolve()
    if not weights.is_file():
        raise FileNotFoundError(f"Weights not found: {weights}")

    exported = Path(
        YOLO(str(weights)).export(
            format="ncnn",
            imgsz=tuple(args.imgsz),
            batch=1,
            device="cpu",
        )
    )

    param = exported / "model.ncnn.param"
    binary = exported / "model.ncnn.bin"
    if not param.is_file() or not binary.is_file():
        raise RuntimeError(f"Ultralytics export did not create expected files in {exported}")

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    output_param = output / f"{args.name}.param"
    output_binary = output / f"{args.name}.bin"
    shutil.copy2(param, output_param)
    shutil.copy2(binary, output_binary)
    metadata = exported / "metadata.yaml"
    if metadata.is_file():
        shutil.copy2(metadata, output / f"{args.name}.metadata.yaml")

    print(f"\nNCNN model ready: {output}")
    print(f"  {output_param}")
    print(f"  {output_binary}")


if __name__ == "__main__":
    main()
