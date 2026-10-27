**简体中文** | [English](README.md)

# ZION 多玩法小程序管理器

**公共运行时 / ELF 实验：**使用 ESP-IDF **6.0.1** 和 `tools/build-runtime.ps1`。只内置小智，其他示例均外部安装。请先读[当前包格式、服务和验证](docs/runtime-applications.zh_CN.md)，包含 ADPCM/DSP 和 Nofrendo CPU 负载。原生入口不重启，退出需要关机再开机，长按 OK 不重启。下方是继承的独立固件基线说明，不是当前插件分支的构建指令或布局。

面向 ESP32-C3 AI Passport 的正式多固件启动器，派生自 alexwwang/meta-pass，
并兼容 FoloToy 官方小程序安装协议。默认构建使用完整 Zion 产品界面，不再是
先前的研究探针。

- 四行滚动玩法卡片、标题后置默认标记、电量进度及空间统计。
- 持久化默认启动，支持启动窗口按键取消；软件返回或崩溃后安全回到启动器。
- 玩法管理：运行一次、设置/取消默认、确认删除玩法及关联资源。
- 设置：Wi-Fi 状态、带密码的临时配网热点、本地网页配置。
- 使用官方玩法社区的小程序，经 BLE 下发请求，由设备下载、校验、重定位安装。
- 精简复古绘制界面及中文字体，仅官方小程序码使用图片。
- 安装失败或取消后回收 APP 和独占资源；提交状态不确定时保留已证实有效的安装。

开源地址：[cexer/folo-ai-passport-zion](https://github.com/cexer/folo-ai-passport-zion)。
保留上游 MIT 许可及版权声明。

阅读[使用说明](docs/zion-user-guide.zh_CN.md)、[架构与兼容性](docs/zion.zh_CN.md)
及[验证说明](docs/development/engineering/build-and-test.zh_CN.md)。激活 ESP-IDF
5.5.3 后，Windows 可运行 `tools/build-zion.ps1`；固件、匹配 ELF/MAP 和清单
输出到 `dist/`。刷写是独立的显式操作，使用 `tools/flash_zion.py`。交付前运行
`./tools/validate.sh`。Linux/macOS 可运行 `tools/build-firmware.sh`，再执行
`python3 tools/package_zion.py --build-dir build`。
构建产物、下载依赖、本地设备配置及 `dist/` 均不进入 Git。

硬件：ESP32-C3、8 MiB Flash、无 PSRAM。产品采用 2 MiB factory 布局。
从旧版 1.44 MiB factory 迁移需要全新安装；相同布局的分段升级保留玩法和设置。
`0x356000` 的身份分区受保护。Zion 替换 592，不同时保留两个独立启动器。

构建、主机测试和真机验收分别报告。按键注入仅用于测试构建，正式构建关闭。
包身份由 `zion-product.json` 定义：MNAM 保存完整中文名称，ESP 描述字段使用
满足 31 字节限制的紧凑名称。历史网页部署流程为手动触发，提交源码不会自动部署服务。
