# 控制模块更新记录

> 日期：2026-07-29（v2 简化：纯像素偏差）  
> 关联文档：`../note/note.pdf`

**v2 变更（2026-07-29）**：实测钢球滚动时管道倾角极小，theta 补偿无必要。
控制链路从「像素→cm→PID→角度→高度→脉冲」简化为「像素误差→PID→角度→脉冲」。

---

## 1. 新增文件

在 `serial/` 目录下新增 4 组文件（8 个），全部位于 `namespace ctrl` 下：

| 文件 | 行数 | 职责 |
|------|------|------|
| `pid.h` / `pid.cpp` | ~60 / ~90 | 位置式 PID + 积分抗饱和 |
| `coord_converter.h` / `coord_converter.cpp` | ~25 / ~3 | **极简**：只保留 `uCenter` 和 `pulsesPerDeg` 两个常数 + 一个转换函数 |
| `serial_motor.h` / `serial_motor.cpp` | ~80 / ~270 | Pi ↔ STM32 文本协议串口（Linux termios） |
| `control_loop.h` / `control_loop.cpp` | ~65 / ~105 | **纯像素闭环**：`update(uBallPx, dt)` 一行搞定 |

**现有文件未做任何修改。**

---

## 2. 架构总览（v2 简化版）

```
┌──────────────────────────────────────────────────────────────┐
│                       main.cpp（将来集成）                    │
│  while (true) {                                              │
│    frame  = cam.read();                                      │
│    dets   = detector.detect(frame);                          │
│    uBall  = dets[0].box.x + dets[0].box.width/2;  ← 像素    │
│    ctrl.update(uBall, dt);                        ← ★一行★  │
│    drawDetections(frame, dets);                              │
│  }                                                           │
└──────────────────────────────────────────────────────────────┘

控制链路（单行）:
  uBall [px] → e = uTarget - uBall → PID → θ° → pulses = pulsesPerDeg × θ°
     → serial_motor.moveRelative(pulses) → STM32 → X42S → 管道倾斜 → 球回位
```

### 依赖关系

```
control_loop
  ├── pid              ← 零外部依赖
  ├── coord_converter  ← header-only（仅两个 double）
  └── serial_motor     ← Linux <termios>；Windows 桩
```

---

## 3. 各模块详解

### 3.1 `pid.h` — PID 控制器

**接口**：
```cpp
PIDController pid(Kp, Ki, Kd, outMax, dt);
double output = pid.update(setpoint, measurement);
```

**特性**：
- 梯形积分（`(e_k + e_{k-1}) * dt / 2`）
- 积分限幅（`outMax / Ki`）
- 条件积分抗饱和（输出饱和且积分同向时，回退本次积分）
- 对测量值微分（避免 setpoint 跳变冲击）
- 在线调参：`setGains()`, `setOutMax()`, `setDt()`
- 调试接口：`integral()`, `lastError()`, `lastOutput()`

**默认参数**：`Kp=0.5, Ki=0.02, Kd=0.1, outMax=10°, dt=0.05s`

---

### 3.2 `coord_converter.h` — 极简转换器（header-only）

实测倾角极小，省略全部 theta 补偿和 cm 转换。只保留两个标定常数：

| 成员 | 默认值 | 含义 |
|------|--------|------|
| `uCenter` | 320 px | 目标像素列（画面中管道中心 O） |
| `pulsesPerDeg` | 80 pulse/° | 每度倾角对应脉冲数 |

**唯一方法**：
```cpp
int pulses = conv.pidOutToPulses(pidOutDeg);
// = round(pidOutDeg × pulsesPerDeg)
```

**标定 `pulsesPerDeg`**：发固定倾角指令，记录实际脉冲数，取比值。

---

### 3.3 `serial_motor.h` — 串口通信

**物理层**：Linux termios，115200-8N1，`/dev/serial0`（Pi 默认串口）

**协议层**（文本协议，每行以 `\n` 结尾）：

| Pi → STM32 | STM32 → Pi | 说明 |
|------------|-----------|------|
| `ENABLE` | `OK ENABLED` | 使能电机 |
| `DISABLE` | `OK DISABLED` | 失能 |
| `MOVE <p> <s> <a>` | `OK MOVED <posX10>` | 相对移动 p 脉冲，速度 s RPM，加速度 a |
| `STOP` | `OK STOPPED` | 停止 |
| `READPOS` | `POS <posX10>` | 读位置（×10，0.1° 单位） |
| `ZERO` | `OK ZEROED` | 设当前位置为零点 |
| `STATUS` | `STATUS <flags>` | 读状态标志位 |
| 任何通信异常 | `ERR <CODE>` | 超时/校验错/参数错 |

**关键方法**：
```cpp
motor.open("/dev/serial0");
motor.enable();
motor.moveRelative(+400, 200, 10, posAfter);  // 400脉冲, 200RPM, 加速度10
motor.readPosition(posX10);                    // 读取位置
motor.stop();
```

**平台兼容**：`#ifdef _WIN32` 下编译为桩函数（返回 false + 错误信息），方便在 PC 上编译测试。

---

### 3.4 `control_loop.h` — 纯像素闭环（极简 API）

**核心接口——一行调用**：
```cpp
ControlLoop ctrl;
ctrl.init("/dev/serial0");

// 每帧只调这一个方法：
double errorPx = ctrl.update(uBallPx, dt);
// uBallPx: 钢球像素列（-1 表示未检出）
// dt:      距上帧秒数
// 返回:    当前像素误差（HUD 用）
```

**内部逻辑**：
```
uBallPx → e = uTarget - uBall → PID(0, -e) → θ° → pulses = θ° × pulsesPerDeg
  → motor.moveRelative(pulses)
```

**目标设定**：
```cpp
ctrl.setTargetPixel(320);   // 球控制在画面中央
ctrl.setTargetPixel(400);   // 球控制在偏右（对应管道右侧某位置）
```

**默认 PID 参数**（像素空间，需实测调优）：
| 参数 | 默认值 | 含义 |
|------|--------|------|
| Kp | 0.08 °/px | 每像素偏差输出 0.08° 倾角 |
| Ki | 0.005 °/px/s | 积分消除静差 |
| Kd | 0.02 °·s/px | 阻尼抑制超调 |
| outMax | 8° | 最大倾角限制 |

**钢球丢失处理**：连续 10 帧未检出 → 停电机 → 检出后自动恢复。

---

## 4. STM32 端需要实现的协议

STM32 端（待开发）需新增 `USART2` 接收 Pi 的命令，并翻译为 X42S 二进制协议：

```
USART2 中断接收文本行 → 解析命令 → 调用 EmmMotor_xxx() → USART2 回传应答文本
```

命令—EmmMotor 映射：
| Pi 命令 | STM32 调用 | 应答 |
|---------|-----------|------|
| `ENABLE` | `EmmMotor_Enable(1, 1)` | `OK ENABLED` |
| `DISABLE` | `EmmMotor_Enable(1, 0)` | `OK DISABLED` |
| `MOVE p s a` | `EmmMotor_MoveRelative(1, p, s, a)` → 再读位置 | `OK MOVED <pos>` |
| `STOP` | `EmmMotor_Stop(1)` | `OK STOPPED` |
| `READPOS` | `EmmMotor_ReadPositionX10(1, &pos)` | `POS <pos>` |
| `ZERO` | 记录当前 `pos` 作为偏移量 | `OK ZEROED` |
| `STATUS` | `EmmMotor_ReadStatusFlags(1, &flags)` | `STATUS <flags>` |

---

## 5. 标定流程（仅 2 个参数）

| 参数 | 方法 | 步骤 |
|------|------|------|
| `uCenter` | `conv_.uCenter = v` | 管道水平时，画面中管道中心 O 对应的像素列 |
| `pulsesPerDeg` | `conv_.pulsesPerDeg = v` | 电机走已知角度，记录脉冲数，$v = N / \theta$ |

---

## 6. 后续集成步骤（将来）

1. **STM32 端**：实现 §4 的协议转发程序
2. **接线**：Pi GPIO14/15 (UART) ↔ STM32 USART2 (PA2/PA3)
3. **CMakeLists.txt**：添加 4 组新 `.cpp` 到编译
4. **main.cpp**：循环中加 2 行：
   ```cpp
   double uBall = dets[0].box.x + dets[0].box.width / 2.0;
   ctrl.update(uBall, dt);
   ```
5. **config.h**：加 `SERIAL_PORT`、PID 参数、`PULSES_PER_DEG`
6. **标定**：`uCenter`（像素）+ `pulsesPerDeg`（电机）
7. **调参**：从 Kp 开始，再加 Kd，最后加 Ki

---

## 7. 文件树

```
serial/
├── pid.h                    ← 新增
├── pid.cpp                  ← 新增
├── coord_converter.h        ← 新增
├── coord_converter.cpp      ← 新增
├── serial_motor.h           ← 新增
├── serial_motor.cpp         ← 新增
├── control_loop.h           ← 新增
├── control_loop.cpp         ← 新增
│
├── config.h                 （未改动）
├── detector.h / .cpp        （未改动）
├── camera.h / .cpp          （未改动）
├── visual.h / .cpp          （未改动）
├── main.cpp                 （未改动）
├── CMakeLists.txt           （未改动）
├── model/                   （未改动）
├── scripts/                 （未改动）
└── README.md                （未改动）
```
