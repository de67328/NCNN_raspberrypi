"""训练钢珠 YOLO 检测模型。

示例：
    python scripts/train.py --data D:/YOLO/steel_ball_/data/dataset_yolov8.yaml
"""

from __future__ import annotations

import argparse
from pathlib import Path

from ultralytics import YOLO


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train a YOLO steel-ball detector")
    parser.add_argument("--data", type=Path, required=True, help="Ultralytics dataset YAML")
    parser.add_argument("--weights", default="yolov8n.pt", help="Initial .pt model")
    parser.add_argument("--imgsz", type=int, default=320)
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--batch", type=int, default=16)
    parser.add_argument("--device", default=None, help="e.g. 0, cpu; default lets Ultralytics choose")
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--project", type=Path, default=Path("runs/train"))
    parser.add_argument("--name", default="steel_ball")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    data = args.data.expanduser().resolve()
    if not data.is_file():
        raise FileNotFoundError(f"Dataset YAML not found: {data}")

    kwargs = {
        "data": str(data),
        "imgsz": args.imgsz,
        "epochs": args.epochs,
        "batch": args.batch,
        "workers": args.workers,
        "project": str(args.project),
        "name": args.name,
    }
    if args.device is not None:
        kwargs["device"] = args.device

    model = YOLO(args.weights)
    results = model.train(**kwargs)
    best = Path(results.save_dir) / "weights" / "best.pt"
    print(f"\nTraining complete. Best weights: {best.resolve()}")


if __name__ == "__main__":
    main()
