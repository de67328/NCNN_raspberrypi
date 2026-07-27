# YOLOv8 → NCNN → Raspberry Pi 钢珠检测

本工程包含完整的三段流程：

1. 在 PC 上用 Ultralytics 训练单类别 `ball` 检测模型；
2. 把 `best.pt` 直接导出为 NCNN 的 `.param + .bin`；
3. 在 64 位 Raspberry Pi OS 上用 C++、NCNN、OpenCV 和 CSI 摄像头实时推理。

当前已有模型 `yolov8n_320_best.pt` 的规格为：输入 `1×3×320×320`，输出
`1×5×2100`，类别为 `{0: ball}`，模型本身不包含 NMS。

## 1. 数据集

目录应符合 Ultralytics YOLO 格式：

```text
steel_ball_dataset/
├── images/
│   ├── train/
│   └── val/
└── labels/
    ├── train/
    └── val/
```

每张图对应一个同名 `.txt`，每行格式为：

```text
class_id center_x center_y width height
```

后四项均为相对图像宽高的 `0~1` 数值。复制
`dataset.example.yaml` 并修改其中的 `path`。

## 2. PC 训练

建议在 Python 虚拟环境中执行：

```bash
python -m venv .venv
# Windows: .venv\Scripts\activate
# Linux:   source .venv/bin/activate
python -m pip install -r requirements-train.txt

python scripts/train.py \
  --data D:/YOLO/steel_ball_/data/dataset_yolov8.yaml \
  --weights yolov8n.pt \
  --imgsz 320 \
  --epochs 100 \
  --batch 16 \
  --device 0
```

最优权重默认位于：

```text
runs/train/steel_ball/weights/best.pt
```

没有 NVIDIA GPU 时去掉 `--device 0` 或改为 `--device cpu`。

## 3. 导出 NCNN

不要把 ONNX 文件改后缀。直接从训练得到的 `.pt` 导出：

```bash
python scripts/export_ncnn.py \
  --weights yolov8n_320_best.pt \
  --imgsz 320
```

Ultralytics 会调用 PNNX，并把最终文件整理到：

```text
model/best.param
model/best.bin
model/metadata.yaml
```

第一次导出若提示缺少 `ncnn` 或 `pnnx`，按提示安装后重新运行：

```bash
python -m pip install ncnn pnnx
```

可以先在 PC 上用 Ultralytics 对导出目录做精度回归：

```bash
yolo val model=yolov8n_320_best_ncnn_model \
  data=/path/to/dataset_yolov8.yaml imgsz=320
```

当前仓库模型在 237 张真实验证图（2039 个实例）上的实测结果：

| 模型 | Precision | Recall | mAP50 | mAP50-95 |
|---|---:|---:|---:|---:|
| PyTorch | 0.9349 | 0.9086 | 0.9464 | 0.5015 |
| NCNN | 0.9337 | 0.9111 | 0.9471 | 0.5013 |

两种后端的指标基本一致，说明本次转换没有明显精度损失。

## 4. Raspberry Pi 安装依赖

推荐 Raspberry Pi 4/5 和 64 位 Raspberry Pi OS：

```bash
sudo apt update
sudo apt install -y build-essential git cmake libprotobuf-dev \
  protobuf-compiler libomp-dev libopencv-dev
```

编译并安装 NCNN（CPU 推理，关闭 Vulkan）：

```bash
git clone --depth 1 https://github.com/Tencent/ncnn.git
cd ncnn
git submodule update --init
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DNCNN_VULKAN=OFF \
  -DNCNN_BUILD_EXAMPLES=OFF \
  -DNCNN_BUILD_TOOLS=OFF
cmake --build build -j"$(nproc)"
sudo cmake --install build
sudo ldconfig
```

确认相机可用：

```bash
rpicam-hello -t 3000
```

## 5. 复制、编译、运行

把整个工程（尤其是 `model/best.param` 和 `model/best.bin`）复制到树莓派：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cd build
./ball_detect
```

程序从 `build/model/` 加载模型，按 `q` 或 `Esc` 退出。每次构建都会用
`model/best.param` 和 `model/best.bin` 同步 `build/model/`，避免误用旧权重；
缺少模型时 CMake 会直接报错。

可在 `config.h` 中调整输入尺寸、置信度、NMS 阈值、NCNN 线程数及相机参数。
`INPUT_SIZE` 必须和导出时的 `--imgsz` 一致。

## 关键实现说明

- 预处理使用保持宽高比的 letterbox，填充值为 114；
- 后处理按 letterbox 的缩放和边距将框映射回原图；
- 自动读取 NCNN 模型的输入/输出 blob 名称，兼容 PNNX 常见的 `in0/out0`；
- YOLOv8 原始输出不带 NMS，本工程在 C++ 端按类别执行 NMS。
- 相机由受控的 `rpicam-vid` 子进程和后台采集线程驱动，只保留最新帧，
  避免推理较慢时顺序处理积压画面；
- MJPEG 流解析支持 `FF D8`/`FF D9` 跨读取块，损坏单帧会跳过而不会终止；
- 相机启动会等待首帧确认，运行中 EOF/进程退出会让主程序明确报错退出，
  不会陷入 CPU 空转。

训练和 NCNN 导出参数可参考
[Ultralytics 官方训练文档](https://docs.ultralytics.com/modes/train/) 与
[Ultralytics NCNN 文档](https://docs.ultralytics.com/integrations/ncnn/)；
NCNN 的树莓派构建方式见
[Tencent NCNN 官方构建文档](https://github.com/Tencent/ncnn/wiki/how-to-build)。
