[English](shared-runtime.md) · **简体中文**

# ZION 公共运行时与原生 ELF 实验

当前同步与分区说明：[runtime 0.5.1](online-sync-051.zh_CN.md)。

以下是 0.2.0 内置示例的历史说明。当前外部包实现和真机验证范围请先阅读
[runtime-applications.zh_CN.md](runtime-applications.zh_CN.md)。

本地目录 `folo-ai-passport-zion-runtime`，分支 `feature/shared-runtime`。
继承 `zion-with-xiaozhi` 的工作区快照（Git 基点
`f0e2e28d4a04a287d48bb3d9ace06b1f93c60661`，包含其未提交修改）。
原实验目录、正式项目均保留。下方初始实验发生在 GitHub 发布之前，当前状态以上方说明为准。

## 运行方式

ZION 统一持有屏幕/LVGL 任务、ADC 按键、I2C、电池、Wi-Fi 配网和连接、整数配置、
16kHz 单声道全双工 PCM。玩法调用版本化 C 服务表，不公开 LVGL/ESP-IDF 句柄或 STL ABI。
最小小智的板级、显示、音频适配层复用这些服务，不再另行初始化屏幕、按键、配网管理器及 codec。

列表直接进入一个玩法，不重启。离开玩法通过关机再开机；上键＋开机绕过默认。
**长按 OK 不重启、不返回 ZION**，按键归玩法自身使用。不做热卸载，也不在同次开机中启动第二个玩法。
原生玩法运行期间拒绝安装器请求；配网热点开启时需要先关闭热点再进入玩法。

小智仍是静态注册的原生组件。其 C++ 框架、协议和 Opus 尚未完全改造成独立 C SDK 插件；
下面两款 ELF 示例则真实经过独立文件加载。

## 独立模块

`tools/build-runtime-plugins.py` 单独构建 RV32IM、ILP32、小端 `ET_REL` 文件，
不把 ESP-IDF、BSP、LVGL、网络库链接进插件。
`main/zion_elf.c` 检查节、符号、重定位边界及 ABI/服务表大小，分配代码和数据，
完成重定位并解析少量 libc/运算辅助函数。入口参数为宿主服务表。
代码、数据分别使用 C3 正确的指令/数据别名，模拟器执行真实 guest RAM 中的插件指令；
宿主没有把插件入口符号静态链接为自己的函数。

加载器是有意受限的实现，不等同 Linux `.so`：不支持 TLS、C++ 静态初始化/异常展开、
任意重定位类型、PIC 或压缩指令模块，需使用配套编译参数。
实验配置显式设置 `CONFIG_ESP_SYSTEM_MEMPROT=n`，因为 C3 默认指令/数据分隔禁止可执行堆分配。
它是共享地址空间的可信原生代码平台，不提供进程隔离。没有改 eFuse，也没有刷写真机。

为便于复现，独立 ELF 与媒体样例作为**只读文件数据**打包，运行时真实加载和重定位；
插件代码不是静态链接进宿主代码段。产物也单独提供 ELF 文件。
**尚未实现**从文件系统、USB 或小程序安装新 ELF 包，以及可扩展插件目录。
旧社区 BIN 保留原完整固件路径，不能不改源码自动变成 ELF 应用；
经官方小程序分发插件的方案尚未验证。

## 复杂社区候选

选择图形压力较高的[社区玩法 9：电子相册](https://ai-passport.folotoy.cn/plays/9/)，
源码 [PhoenixZHC/folotoy_gallery](https://github.com/PhoenixZHC/folotoy_gallery)，
提交 `00cbde9d2c9f7998f1690e44c7dcc8ed21cbb63f`。
原版 V1.4.0 已用 IDF 5.5.3 构建，APP 为 1,033,392 字节。
这是具有代表性的复杂压力候选，未对全社区项目做“最复杂”排名证明。

插件移植了原 FGA1 解析器和动画调度器。原项目在手机浏览器转换 GIF；
设备解析 FGA1，不是在设备上直接解 GIF/MOV。
本次为解析器增加流式输出，和原连续缓存路径做逐字节对照。
ZION 按需分配五块各 30KiB 像素块，避免申请一块连续 150KiB；解码、呈现期间持有 LVGL 锁。
截图全部 240×320 像素与独立解码结果对照。

独立文件实测：计数器 3,008 字节，相册核心 10,356 字节，FGA1 媒体样例 158,140 字节。
媒体为自行生成的几何测试图，含全帧及增量帧，不使用他人照片。
运行时像素仍需 150KiB RAM，不能把小安装包理解成不占内存。
小模块**不代表完整相册移植**：网页上传、FAT 存储、音乐、录音及完整设置菜单未纳入模块。
来源及适配范围记录在 `examples/plugins/gallery/source.json`。

## 构建与验证

本分支使用最小小智要求的 **ESP-IDF 6.0.1**：

```powershell
. ..\built-tools\Enter-ESP-IDF6.ps1
.\tools\build-runtime.ps1 -BuildDirectory build-runtime-delivery
.\tools\build-runtime.ps1 -Simulator
python -m pip install --no-user -r tools/requirements-runtime-test.txt
python tools/test-runtime-host.py --cc <host-C-compiler>
python tools/test-runtime-sim.py --sim ..\PassportSim\passportsim-1.0.3-windows-x64\passportsim.exe
```

交付验证使用新构建目录。正式脚本构建、合并并检查 ESP 校验和/SHA、ELF 绑定、
分区 MD5/边界、NVS 全擦除状态、APP/bootloader/分区表 factory 一致及嵌入插件身份。
带版本号产物在 `release/`，匹配的调试资料和模块在 `dist/zion-runtime/<hash12>/`。
完整 BIN 从 `0x0` 启动，APP-only 不可写到该地址。
factory 仍为 `0x260000`，cardid `0x356000`、store `0x35A000`、otadata `0x7FE000`。
旧 2MiB 更新器不兼容。本次未刷真机。

模拟专用构建关闭模拟器不支持的 SDK6 无线/PCM，并显式使用小智显示/输入测试入口。
ELF 重定位和执行、FGA1 解码、动画、绘图、按键、NVS 和关开机均运行真实 guest 代码。
已测直接进入、关开机后配置保留、默认启动、上键绕过、长按 OK 不重启、动画暂停/继续和三幅逐像素验证的帧。
按 PassportSim 定义，`run` 为 PASS_WITH_CAVEATS（CLI 退出码 10），不是硬件验收通过。
列表持续刷新也可能无法满足 UI-settled 标记；测试用真实串口和画面断言。

专项 Host tests 覆盖畸形 ELF/ABI 拒绝与失败释放、单次开机仅一个应用、按键过滤、
连续/流式解码一致性与分块边界、字形覆盖，以及原菜单/策略/安装器协议。
继承的完整门禁仍首先被导入小智文档缺少双语配对阻断，未宣称通过。

精确固件身份、大小与结果在 `diagnostics/runtime-final-report.json`；
模拟器回执、脱敏串口及截图在 `diagnostics/runtime-sim/`。
Device tests：**NOT RUN**。
待验证：真机 C3 可执行内存及 RAM 余量、Wi-Fi/BLE/TLS、激活、录音/TTS、动态字形、真实小程序安装。
旧 `builtin-xiaozhi` 文档仅代表历史基线实验。
