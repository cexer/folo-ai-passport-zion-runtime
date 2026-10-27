[English](idf-source-compat.md) · **简体中文**

# ESP-IDF 源码兼容：实验配置

## 运行资源准入

插件文件不设置字节配额，加载器不设置固定的代码/数据分配预算或节/符号数量配额。
安装受实际可用 Flash 空间限制；加载时按需求向内存分配器申请。
申请失败，设备显示“内存不足”，并释放加载器已部分申请的内存。
仍保留文件格式、整数溢出、ABI 和重定位校验。文件合法且能安装，仍可能因
RAM、连续可执行内存、Flash 映射或公共符号缺失而无法运行。
这不代表能捕获进入应用后业务代码内部的任意内存申请失败。

目标是业务源码尽量不改，通过编译链接选择运行环境。这是显式 `zion_api_t`
接口之外的新增路径，不是把现成 BIN 自动转换为插件。

当前配置支持 C 和 C++17，使用 ESP-IDF 6.0.1、LVGL 9.5.0 和匹配的宿主配置。
支持构造函数、局部静态初始化和已测试的标准库基础能力；关闭异常和 RTTI。
导入表结合显式资源所有权适配与自动生成的 SDK 精确符号，并不代表任意二进制兼容。

## 应用要改什么

保留普通 `app_main()`、LVGL、FreeRTOS、NVS 和已支持的 BSP 调用。
在原项目旁增加一个 `compat.json` 构建描述，例如：

```json
{
  "id": 2147483653,
  "key": "idf-compat",
  "name": "IDF Compat CPU",
  "sources": ["main/main.c", "../plugins/nescpu/upstream/nes6502.c"],
  "include_dirs": ["../plugins/nescpu", "../plugins/nescpu/upstream"],
  "cflags": ["-Wno-discarded-qualifiers"]
}
```

路径相对此描述文件。每个应用使用独立、稳定的高位 ID 和存储 key，不要复用示例身份。
测试应用的业务源码不需要 ZION 头文件、API 函数表、应用专属适配层或条件分支；
构建工具在外部生成入口包装。

可选的 `link_wrap` 列表对额外的配置源文件启用 GNU 链接包装。它不是自动兼容保证，
也不是使用公共服务的必需条件。例如 V-Pet 原程序把入口栈上的对象交给界面长期保存，
其配置增加了保活适配。候选源码保持不动，但这项应用专属修正会明确记入构建清单，
不能把它与通用 SDK 能力补充混为一谈。

编译器使用宿主真实 SDK/BSP/LVGL 头文件及 C 库 specs，构建 RV32IM/ILP32
可重定位 ELF，保留公共函数导入，避免把完整基础库重复链接进去；加载器将导入
解析到宿主实现。BSP 初始化调用复用宿主已持有的子系统，按键注册把应用回调接到
现有按键分发器。回调仍必须非阻塞。`app_main` 返回后退掉入口任务，
应用自己创建的任务继续存活，直到关机。

兼容导出表位于 `main/zion_idf_exports.inc`。未支持的导入在打包前明确报出名称；
不导出整片 Flash 擦除或任意符号查找。为保留应用既有协议，现已导入精确的 SDK
`esp_restart`，例如 Agent 的 USB `reboot` 指令；ZION 不占用任何按键进行重启。
这是兼容边界，不是安全沙箱：
原生代码地址空间及 NVS 命名空间仍然共享，应用必须可信。

## 构建与打包

在本仓库激活工具链，先构建宿主：

```powershell
. ../../../build-tools/Enter-ESP-IDF6.ps1
./tools/build-runtime.ps1 -DeviceTest -BuildDirectory build-layout-apps
python tools/build-idf-compat.py examples/idf-compat --host-build build-layout-apps --output build-idf-compat-device
```

生成双模式 BIN 时，再通过原有 IDF 工程构建同一份应用源码。
使用 `idf.py -C` 时，构建及配置路径使用绝对路径：

```powershell
$compatBuild = Join-Path (Get-Location) 'build-idf-compat-standalone'
New-Item -ItemType Directory -Force $compatBuild | Out-Null
idf.py -C examples/idf-compat -B $compatBuild -D "SDKCONFIG=$compatBuild/sdkconfig" build
idf.py -C examples/idf-compat -B $compatBuild merge-bin -o "$compatBuild/IDF-Compat-Standalone-full.bin"
python tools/build-idf-compat.py examples/idf-compat --host-build build-layout-apps --output build-idf-compat-device --standalone-full "$compatBuild/IDF-Compat-Standalone-full.bin"
```

产物包含 `app.elf`、`app.zion`、可选的 `app-dual.bin`、生成的 `entry.c` 和
记录源码哈希、导入符号、宿主身份的 `manifest.json`。
双模式 BIN 在完整独立固件之后追加 ZPK1 插件包及 ZIONDUAL 尾标。
当前 USB 工具能提取原生插件部分：

```powershell
python tools/zionctl.py build-idf-compat-device/app-dual.bin --port COM_PORT
```

插件绑定宿主的构建契约。生产插件必须针对生产宿主构建，不能拿 DeviceTest/SIM
插件代替。契约指纹包含 SDK 配置、C 库 specs、相关头文件和兼容层，入口包装导入
对应的精确契约符号。本地 SDK 输入过期会构建失败；宿主不匹配会在调用 `app_main`
之前拒绝加载。首版刻意采取严格绑定，并不是跨版本通用动态 ABI。

生产宿主使用全新构建目录，不带 `-DeviceTest`、`-Simulator`。SIM 明确关闭无线/PCM。
官方小程序能否分发此扩展双模式包、社区能否审核通过，尚未验证。

## 证据和剩余范围

版本化生产宿主为 `release/ZION-Runtime-0.4.0-runtime-afa71a9888b0.bin`
（2,378,304 字节），匹配调试文件位于 `dist/zion-runtime/afa71a9888b0`。
宿主没有内置这个测试应用。对应示例包位于 `release/idf-compat-0.4.0/`：
插件 76,540 字节，双模式 BIN 776,028 字节，独立 BIN 699,472 字节。
与其他宿主配置混用前，先核对该目录的 `manifest.json`。
非测试生产组合已构建校验，真机自动化使用对应的 DeviceTest 组合。

`examples/idf-compat/main/main.c` 是普通 IDF 兼容测试应用，同一文件同时用于
独立构建和插件构建，没有做运行环境适配式源码改写。
第三方 Nofrendo `nes6502.c` 与候选仓库逐字节一致，SHA-256 为
`e8f3fe711aa8d5674df230d06a63a574d4f6bd2764f88c1b3a860cf289106ae0`。
这验证了真实第三方 CPU 核心下的机制，不代表已经完成原 NES 应用完整 UI、PPU、
APU 和蓝牙的移植。

[汇总验证报告](../diagnostics/idf-compat/report.json) 关联安装包身份及各配置结果。
真机及模拟器记录位于 `diagnostics/idf-compat/`，覆盖 6502 参考结果、真实 LVGL 对象、
`app_main` 返回后的 FreeRTOS 任务/队列、按键、NVS 提交/持久化、双模式包提取
及错误契约拒绝。模拟器运行 caveat、持续重绘引起的开机稳定标记超时保留在记录中，
不代表无线或音质验证。主机测试对真实兼容 ELF 做重定位，并验证错误契约拒绝后无
分配泄漏；真实编译器负向测试验证不支持的 `esp_flash_erase_chip` 导入被拒绝，
且不会留下旧安装包。

另一次更新安装的真机 panic 已用匹配的 ELF
`264cf4f6ebc1c287b765bab8c0b4df4237b66b737ea3cb88a6b22bf9a03ccfe2`
解码：列表在物化重建分区缓存时，仍从 NVS 读取默认项，访问了已失效的分区缓存指针。
默认身份现改为初始化时加载、原子缓存，设置提交成功后更新缓存；之后连续三次安装更新
没有重启，完整故障记录保留于 `install-device.log`。更早一次未等到任务就绪日志的超时，
没有由这份调用栈独立证明原因。

仍然一轮开机只运行一个应用。列表进入应用不重启；退出或切换应用需要关机再开机，
按住上键开机跳过默认。没有增加长按 OK 重启或热卸载。
完整第三方应用兼容与未支持的驱动/资源生命周期仍是后续工作。
下述后续 C/C++ 验收更新了早期测试应用的覆盖范围。

## 当前真机验收

未经修改的上游 UNO C/C++ 源码已在 ESP32-C3 启动，打开音频编解码器，
初始化 Wi-Fi/ESP-NOW、建立房间，并响应上/下/OK/长按操作。
另一个 C/C++ 探针验证了全局构造函数、局部静态初始化、字符串、互斥锁、
C 链接与 FreeRTOS 任务/队列。验证聚焦主流程和底层能力，未覆盖全部 UNO
规则，也未验证第二台实物设备无线对战。

特意填充的 UNO 测试包（ELF 300,000 字节，总计 1,100,256 字节）安装后，
同样通过主流程。另一个内存测试包申请超过整机容量的可执行内存，实际申请失败，
加载器释放部分分配并报告 `elf:out-of-memory`。
这些人工放大的尺寸仅用于回归，不是原版 UNO 的体积。
## 可选按键配置及独立固件 OTA 语义

应用使用不同 ADC 分类表时，可在外部配置中把 `button_config_header` 指向其原始
`bsp_pins.h`。生成的入口读取 `BSP_BTN_MV_TABLE`，在 `app_main` 前配置共享 BSP，
业务 C/C++ 文件不变。支持板上三键及一个可区分的并联电压，GPIO 和 ADC 所有权
仍属于 BSP。拒绝重叠、非法电压区间；初始化失败时尝试恢复原配置。
重启后恢复管理器默认三档。同名接口不代表任意改造过的 BSP 都能自动互换。

独立固件 OTA 不是插件更新。原生插件导入的 OTA 查询不返回下一槽位；
begin/write/end/abort、启动分区选择和回滚确认返回 `ESP_ERR_NOT_SUPPORTED`。
插件更新应通过 ZION 包管理；单独构建的独立模式仍链接实际 ESP-IDF OTA 实现。
合并固件可使用 factory 或纯 OTA 分区布局，但分区表必须只有一个明确、完整校验的 APP。

DeviceTest 提供设备任务定时的原始按键脉冲，按住时间不包含 USB 确认耗时。
它属于回调注入，不代表物理触点或 ADC 噪声实测。
