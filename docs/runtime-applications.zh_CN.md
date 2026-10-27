[English](runtime-applications.md) · **简体中文**

# 外部应用：当前实验

目录 `apps/zion-runtime/firmware`，分支 `feature/shared-runtime`，ESP-IDF **6.0.1**。
正式社区版仍在 `apps/zion-online/firmware`。宿主只内置最小小智，其他示例全部外部安装。
本次实验不包含提交、推送或发布。

## 生命周期与公共能力

列表直接进入一个原生应用，不重启。离开或切换应用必须关机再开机；开机按住上键可跳过默认项。
长按 OK 属于应用，不负责重启或返回列表。完整独立固件仍走原来的重启引导路径，不会变成原生插件。

C ABI 提供文字、按键、设置、电量、联网状态、16 kHz 单声道双工 PCM、字形、资源和 RGB565 绘制，
不暴露 C++ 对象或 ESP-IDF/LVGL 句柄。这张显式表不提供通用 HTTP/MQTT；
另外的源码兼容链接路径已经接通真实 SDK 的 HTTP/MQTT、FAT 与 Opus，见
[社区验证](community-link-validation.zh_CN.md)。小智仍是静态原生 C++ 组件。

交接时删除管理界面并释放蓝牙；离线应用还停止网络任务、释放队列并关闭 Wi-Fi。
8 KiB 栈的安装控制台换为 2 KiB USB 控制台，USB 仍可访问，但运行中拒绝安装；ROM USB 刷机不依赖它。
管理主任务退出后，单次 Idle hook 等任务清理完成再唤醒应用，按键直接进入应用队列。
必要的显示、输入、存储服务保留，小智保留联网。不支持热卸载。

## 三种包

| 类型 | 内容 | 验证路径 |
| --- | --- | --- |
| 仅插件 `.zion` | 256 字节 ZPK1 头、ELF、可选资源 | USB 安装到 DATA，直接进入原生应用 |
| 双模式 `.bin` | 完整固件、ZPK1、`ZIONDUAL` 尾标 | CLI 提取插件；完整 BIN 也可独立启动 |
| 仅独立 `.bin` | 普通完整固件 | 直接刷机；USB 兼容安装后引导独立应用 |

当前较窄的 USB 兼容路径提取 factory APP，拒绝私有 DATA 分区。继承的社区安装器另有完整镜像和
DATA 路径。官方小程序分发原生插件**未验证**，普通 BIN 不会自动变成共享运行时应用。

ZPK1 校验长度、受限 UTF-8 名称/键、标识、ABI/API 大小、包头 CRC32 及负载 SHA-256。
使用正式分配器的稳定 ID DATA 记录；更新先校验新空间再切换，未提交的失败写入会擦除，
不确定提交保留可能有效的数据。USB 独立 APP 使用 DIRTY 预留暂存、原始 APP 解析、SHA 身份，
暂存会回收或在开机恢复。原生默认项/删除复用已有菜单模型，内置小智不可删除。

## 真实源码负载

| 示例 | 来源与移植范围 |
| --- | --- |
| 画廊 | [PhoenixZHC/folotoy_gallery](https://github.com/PhoenixZHC/folotoy_gallery)，`00cbde9d2c9f7998f1690e44c7dcc8ed21cbb63f`：FGA1 完整帧/差分帧及调度；不是完整网页/FAT/音乐应用 |
| 声音算法 | [Liutupi/nabo-sound-lab](https://github.com/Liutupi/nabo-sound-lab)，`e16007ec78a73ac28b9ea3c0b7a3f6a19f30139b`：ADPCM、12 种效果、PCM 和设置；无美术、完整菜单、ESP-NOW |
| NES CPU | [oyf518/esp32-nes-card](https://github.com/oyf518/esp32-nes-card)，`1dba1b9ed026d03d58a7c2d61dfc1b508175f4c9`：未改 Nofrendo CPU，自制算术/分支/栈/IRQ/NMI 负载；不是完整 PPU/APU/BLE 游戏移植 |

各示例保留原始声明、许可证和来源。未包含第三方游戏 ROM 或受限的 NABO 美术、语音素材。
声音算法只把初始化 libm 查表预计算，12 种效果的全部输出 PCM 与未改上游 C 实现逐字节对照。
CPU 在 200 次分片后的基准为：819,539 周期，摘要 1,738,864,307，PC 32,800，IRQ 19，NMI 29。

ELF 限定为 RV32IM/ILP32 `ET_REL`；校验后从 Flash 只读映射文件，重定位后的代码/数据在 RAM 中。
文件上限 256 KiB，代码预算 64 KiB，数据预算 256 KiB，实际堆和连续块限制更严格。
一次实测中 O2 CPU 模块需 48,196 字节连续代码空间，但最大块仅 43,008 字节，加载失败；
Os 版本代码 35,228 字节，通过相同基准。独立 CPU 核心仍用 O2。分配历史会影响布局，不能把这些数值当固定预算。

配置使用 `CONFIG_ESP_SYSTEM_MEMPROT=n`；可信原生代码共享地址空间，没有进程隔离，未改 eFuse。
拒绝 RVC/PIC、TLS、通用 C++ 初始化/异常展开及不支持的重定位。
另见[画廊内存对照](runtime-memory.zh_CN.md)。

## 复现和结果口径

在本仓库激活 `../../../build-tools/Enter-ESP-IDF6.ps1`，然后执行：

```powershell
.\tools\build-runtime.ps1 -DeviceTest -BuildDirectory build-layout-apps
python tools/build-runtime-plugins.py
python tools/build-soundlab-plugin.py
python tools/build-nescpu-plugin.py --opt Os
python tools/test-runtime-host.py --cc PATH_TO_HOST_CLANG
python tools/test-soundlab-reference.py --cc PATH_TO_HOST_CLANG
```

`-Simulator` 为明确关闭无线/PCM 的模拟配置，跳过 PCM 不算硬件音频通过。
`-DeviceTest` 保留硬件并增加串口按键；生产打包不使用这两个开关，并使用新目录。
旧缓存位于 `../build-history`，不能复用其绝对路径。factory 位于 `0x10000`，大小 `0x260000`；
身份仍在 `0x356000`，store 为 `0x35A000`，OTA 元数据为 `0x7FE000`。
旧版 2 MiB 更新器不适用于本实验布局。

包在 `release/test-apps/`，真机记录在 `diagnostics/soundlab/`、`diagnostics/nescpu/`。
汇总矩阵区分构建、主机和设备结果。串口/静音测试不证明实体手感、摄像头观察到的画面、音质、
云端/网络并发或完整原应用移植。继承的全量门禁仍有导入文档双语检查失败，不能标为 PASS。
0.2.0 内置示例和旧 `runtime-final-report.json` 仅为历史证据。

## 0.3.0 验证检查点

新增的 0.4.0 源码兼容构建配置及产物，见 [ESP-IDF 源码兼容说明](idf-source-compat.zh_CN.md)。

权威汇总为
[`runtime-external-app-matrix.json`](../diagnostics/runtime-external-app-matrix.json)，
其中将安装包 SHA-256 与逐项测试记录关联。

| 场景 | 纯插件包 | 双模式包 | 纯独立包 |
| --- | --- | --- | --- |
| SoundLab | 21,720 字节；C3 原生加载通过 | 993,304 字节；原生加载和直接刷机通过 | 971,568 字节；直接刷机和 ZION 旧固件安装路径通过 |
| NES CPU | 73,992 字节；C3 原生加载通过 | 1,013,752 字节；原生加载和直接刷机通过 | 939,744 字节；直接刷机和 ZION 旧固件安装路径通过 |

干净构建的非测试宿主产物为
`release/ZION-Runtime-0.3.0-runtime-9754f23fb304.bin`（2,375,728 字节），
已验证匹配的 ELF/调试文件。宿主没有内置这两个测试应用。
真机自动化使用 DeviceTest 配置，而非此非测试产物；模拟器配置明确关闭无线和 PCM。

新增生命周期测试在 PassportSim 通过：默认自动进入、按住上键开机跳过默认、
取消删除、删除默认插件和目录持久化。持续重绘导致模拟器 `until_ui_settled`
开机标记超时，但执行、参考结果和状态断言通过，无 HLE tripwire。
真机菜单还验证了默认项设置、取消/确认删除、内置应用删除保护和重新安装。
删除 CPU 插件回收 77,824 字节 Flash，重新安装后空闲空间恢复原值。
真机最终停在列表，包含六个条目，无默认项；测试后已关闭串口。

删除确认页现已显示原生插件名称，诊断默认项也包含原生插件。
完整损坏载荷因 SHA-256 不匹配被拒绝，旧目录及空闲空间不变。
记录位于 `diagnostics/native-management/`、`diagnostics/external-apps-sim/`
和 `diagnostics/package-rejection/`。

Build：PASS。Host tests：PASS。Device tests：在上述范围内 PASS。
全仓库门禁仍因继承的导入文档检查失败而为 FAIL，见
`../diagnostics/runtime-complete-gate-final.log`。
这是本地实验分支，本检查点不代表已经提交、推送、发布或支持原应用全部功能。
