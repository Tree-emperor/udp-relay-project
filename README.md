# UDP Relay Project

C++ UDP 原样转发示例：发送端连续发包，转发器过滤来源 IP 后立即转发，接收端统计收包情况，并测量转发器内部耗时。

## 技术框架

- **语言**：C++11，无第三方网络框架。
- **网络**：Windows 使用 Winsock2，macOS 使用 POSIX Socket；IPv4、UDP、阻塞式单线程循环。
- **构建**：MinGW-w64 GCC、PowerShell；Windows 运行库静态链接，目标系统版本为 Windows 7（6.1）。
- **计时**：Windows 转发器使用 QPC；macOS 使用 `steady_clock`。报文中的系统时间戳只供辅助观察，不用于 Win7 微秒级转发结论。

```text
udp_sender → 127.0.0.1:9000 → udp_test → 127.0.0.1:9001 → udp_receiver
```

| 文件 | 当前实现 |
| --- | --- |
| `udp_sender.cpp` | 每次发送 200 包，发送间隔参数为 0 |
| `udp_test.cpp` | 仅接受 `127.0.0.1`，保持报文内容和长度原样转发，无额外休眠 |
| `udp_receiver.cpp` | 解析文本报文；收满 200 包后退出，已收包后静默约 2 秒也会退出 |
| `udp_common.h` | 平台适配、socket 初始化/关闭、IP 转换 |

IP、端口和包数目前在源码中配置。转发器持续运行，手动测试后按 Ctrl+C 结束。

## 组帧格式

每个 UDP 数据报承载一条 ASCII 文本，示例：

```text
seq=1/200 ts=1793328000000000000 hello UDP
```

| 部分 | 含义 |
| --- | --- |
| `seq=1/200` | 当前序号 / 本轮总包数，十进制文本 |
| `ts=...` | 发送端 `system_clock` 时间转换为 Unix 纪元纳秒数；单位为纳秒不代表实际精度达到纳秒 |
| `hello UDP` | 固定测试内容 |

字段以空格分隔，长度可变；不发送结尾的 `\0` 或换行。当前 200 包测试的载荷为 42–44 字节。应用层没有额外帧头、长度字段或校验字段，边界由 UDP 数据报提供。转发器不解析这些字段，也支持原样转发二进制及空载荷；接收端只统计符合上述文本格式的报文。

## 编译与运行

Windows 开发环境：从 [w64devkit 2.10.1](https://github.com/skeeto/w64devkit/releases/tag/v2.10.1) 获取工具链，解压到 `.tools/w64devkit/`，确保存在 `bin/g++.exe`。

```powershell
# 构建 x64 程序与启动脚本
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
# 生成 x86/x64 免安装 ZIP
powershell -NoProfile -ExecutionPolicy Bypass -File .\package.ps1
```

双击 `build/04_one_round.cmd` 自动测量 200 包，结果保存在 `build/runs/日期时间/summary.txt`。也可直接双击根目录 `selftest.cmd` 完成编译和测试。手动观察时按 `01_receiver.cmd` → `02_relay.cmd` → `03_sender.cmd` 顺序运行，勿与自动测试同时启动。

目标 Win7 无需编译器：复制并完整解压 `dist/udp_demo_win_x64.zip` 后运行相同入口。32 位系统使用 x86 包。详细操作见 [Win7 手动测试说明](Win7手动测试说明.md)。构建产物和本机工具链不提交到 Git。

macOS 可分别编译并依次运行三个程序：

```sh
for name in udp_receiver udp_test udp_sender; do
  clang++ -std=c++11 -Wall -Wextra -O2 "$name.cpp" -o "$name"
done
```

## 最终结论

2026-10-30 在 Windows 7 SP1 x64 虚拟机（2 vCPU、4 GiB）实测，预热后 10×200 个转发样本：

| 指标 | 时延 |
| --- | ---: |
| 平均 | **19.52 µs** |
| P50 / P99 | 18.2 / 78.5 µs |
| 最大 | 188.1 µs |

计时范围为 **`recvfrom` 返回 → `sendto` 返回**，不是发送端到接收端的端到端时延。典型耗时符合微秒级预期，但不保证每包小于 1 ms，后续独立复测出现过约 2.919 ms 长尾。

正式测试转发 2000 包、接收 1999 包。Win7 默认接收缓冲区实测为 8 KiB；受控积压实验中，默认缓冲区收到 189/200，改为 1 MiB 后收到 200/200，支持接收队列溢出是原少收现象的最可能原因，但不能精确追溯原来那 1 包。主程序尚未扩大缓冲区，也没有确认/重传机制。

只用转发器 `local elapsed` 或自动入口汇总评价转发时延：接收端旧的包间隔/系统时间差，以及写死的 `~165 us`，均不是可靠的 Win7 转发耗时。

原始数据见 [时延记录](时延.md)、[丢包排查](results/win7-drop-investigation/排查结论.md)。
