# mp_board

个人学习用的割草机器人算法板应用框架，结构参照典型的多进程嵌入式 Linux 工程。
代码从零实现，只依赖 libzmq 和 POSIX，没有第三方框架。

## 架构

```
        +-----------+        +-----------+
        |  cloud    |<------>|  network  |  MQTT 收发、状态上报
        +-----------+        +-----------+
                                   |
 +---------+   +--------+   +------+------+   +---------+
 | carrier |-->|  zhub  |-->|    tasks    |-->| monitor |  事件计数/资源采样
 | 串口收帧 |   | ZMQ中枢 |   | 状态机调度  |   | 日志聚合 |
 +---------+   +--------+   +-------------+   +---------+
      |              ^
      v              |
  共享内存 <----------+  (ModSnapshot, seqlock)

 mpm：守护进程，负责拉起上面所有进程，异常退出按退避重启
```

## 进程与端口

| 进程 | 职责 | 端口 |
|------|------|------|
| mpm | 守护与重启 | - |
| zmq_hub | 事件总线中枢 XSUB/XPUB | 27788 订阅端 / 27789 发布端 |
| carrier | 板间串口收帧、共享内存快照 | - |
| network | MQTT 上报与下行指令 | - |
| tasks | 任务状态机 | - |
| monitor | 事件计数、资源采样 | - |

## 事件 ID

| ID | 名称 | 方向 |
|----|------|------|
| 0x1001 | kEvModUpdate | carrier -> 全体 |
| 0x1002 | kEvMotorCmd | tasks/app -> carrier |
| 0x2001 | kEvTaskCmd | network/app -> tasks |
| 0x2002 | kEvTaskState | tasks -> 全体 |
| 0x3001 | kEvNetStatus | network -> 全体 |
| 0x4001 | kEvLog | 任意 -> monitor |
| 0x4002 | kEvFault | 任意 -> 全体 |

新增事件只在 event.h 里追加，不改已有值。

## 快速开始

```bash
./scripts/setup_build_env.sh
./mm -p host -j8          # 编译
./mm -p host --run        # 编译并运行 mpm
```

运行后看日志：

```bash
tail -f /tmp/mp_board/logs/mpm.log
tail -f /tmp/mp_board/logs/tasks.log
```

`config/default.conf` 里 `[serial] sim = 1` 表示没有硬件，用模拟帧驱动整条链路。
接真板子时改成 0，并把 `port` 改成实际串口。

## 交叉编译

```bash
cmake -B build/mr527 -DPLATFORM=mr527 \
      -DMP_TOOLCHAIN_PREFIX=/opt/toolchain/bin/aarch64-linux-gnu-
cmake --build build/mr527 -j8
```

目标板需要 aarch64 版本的 libzmq，放到 `third_party/zmq/mr527/{include,lib}`。

## 目录

```
libs/      公共库：日志、事件总线、共享内存、配置、串口、CRC
sdk/       板间协议：帧格式与 mod 结构体
zmq_hub/   事件总线中枢
carrier/   板间通信进程
network/   IoT 进程
tasks/     任务调度进程
monitor/   监控与日志聚合
mpm/       进程守护
scripts/   构建、打包、环境检查
config/    默认配置
```

## 后续可以接的东西

1、把 MqttStub 换成 libmosquitto 或 aws-crt-cpp，接口不用动
2、carrier 接真实串口，把 SerialSimulator 换掉
3、补 OTA：A/B 分区、增量包、进度上报
4、补日志落盘轮转策略、崩溃 dump
5、把 tasks 的状态机拆成独立任务类，方便加新任务类型
