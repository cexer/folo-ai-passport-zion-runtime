[English](community-link-validation.md) · **简体中文**

# 社区应用的编译链接接管验证

**已按用户要求暂停。**恢复前先读[2026-10-28 交接](community-validation-paused.zh_CN.md)。

## 新设备检查点（2026-10-28）

Passport OS（438）保持 `b70879b` 业务代码不变，在新 COM7 C3/8MB 设备通过
插件启动、上/下/短按 OK/长按 OK、秒表、运行中俄罗斯方块与设置主流程，
并人工检查实际 LCD 截图。本例需要通用元数据、暂存区和内存块优化，
连续无基座改动计数仍为 **0/3**。独立模式此前使用相同 APP 字节、保留身份区的
重排分区布局通过主流程。混合 BIN 提取插件后也在 COM7 通过相同流程，
这是 USB 客户端路径，不代表小程序下载实测。验证范围见
`diagnostics/community/passport-os/report.json`；不代表十个子应用全部验收，
也不代表原始合并镜像已经刷入。

## 无需改基座连续通过检查点（2026-10-27）

当前计数 **0/3**。以下新案例均暴露过基座导出缺口；模拟器通过不计入真机验收。
优化背景、触发应用及代码位置统一维护在[优化记录](runtime-optimization-log.zh_CN.md)。

- [Snoopy Passport](https://github.com/Juicebro-star/snoopy-passport)，提交
  `6d9200e8112a61b16bf093d8c1f773f980394166`：保留原六个 C 编译单元，真机通过
  50 状态/200 帧应用的喂食、五轮小游戏、图鉴、状态和手动校时主流程。补九个导出。
  产物在 `dist/community-link-validation/19a41bf1b51a/`；独立/混合路径的测试范围及
  一次独立版串口回执丢失，均保留在报告中。
- [Connect Four](https://github.com/Shinku-Chen/ai-passport/tree/feature/connect-four)：
  保留原应用及私有 BLE 传输代码，宿主 `5f67f4cdc172` 真机通过两种先手 AI 落子、
  双人落子、横屏、按键、BLE 扫描启停。产物在
  `dist/community-link-validation/connect-four-validated/5f67f4cdc172/`。
  混合包真机路径仍待补测。COM5 断开可能与闲置休眠或移动 USB 接线有关，用户明确表示不确定，
  暂不归因。仅用于 DeviceTest 的深睡转复位保护现已随最新宿主刷入。
- [Liquid Glass UI](https://github.com/EnglandLobster/ai-passport-liquid-glass-ui)，提交
  `6e9bbd9d402fe9a1a9c58cbb4009210cd0a01ef1`：保留原 25 个编译单元和 153,600 字节壁纸，
  三种包均构建成功，补 44 个 SDK 导出。另一份无射频模拟器宿主已执行八场景、焦点/操作、
  退出重进；真机待 USB 唤醒。产物在
  `dist/community-link-validation/liquid-glass-pending/fe2972af1225/`，其中模拟器截图明确标注，
  宿主固件为真机目标版本。
- “去远方”构建前排除：精灵块本身 6,937,968 字节，超过最大安装池 4,841,472 字节，
  没有删资源来凑通过。

最新真机目标 APP 为 2,677,856 字节，自身分区 2,678,784 字节（`0x28e000`）；
APP、bootloader 配置及实际分区表一致。身份区及较大的第二安装池地址不变。
对比 `5f67f4cdc172`，最新 ELF 静态 DRAM BSS 增加 16 字节，data 及 SRAM 别名边界不变；
这是 ELF 测量，不是新真机堆数据。没有重跑无关旧应用的业务流程。

Build：PASS（对应 DeviceTest/独立/模拟器构建）。Host tests：PASS（23 组）。
Device tests：Snoopy/四子棋的上述范围 PASS，最新 Liquid Glass 真机 NOT RUN。
完整 gate 因原有导入小智文档的双语检查 FAIL；不是正式发布或完整产品验收。
当前状态及待执行操作：`diagnostics/community/clean-streak/state.json`。

## Octo Radio 内存优化后续（2026-10-27）

下文原有的 MP3 失败已由保持业务源码不变的后续测试解决，详见
[运行时内存](runtime-memory.zh_CN.md)。只读重定位缓存、分配顺序调整、帧工作区
按需分配和目录精简均为通用宿主/加载器改动。插件 RAM 数据从 29,732 降至
22,276 字节。两条插件路径均通过 25 秒真实 I2S 持续计数观察，窗口内无新增申请失败，
并通过切台、暂停/恢复、动画和设置。混合包独立路径也通过，未变化的独立包保留之前
的通过记录。原应用的播放缓存会在启动时从 8 KiB 回退到 6 KiB，未改业务源码。

归档：`dist/community-link-validation/60cd2e31d2ab/`，宿主 APP 2,654,256 字节。
这是静音本地 DeviceTest 测试包，不是生产固件。后续仅补数学导出，不再重复完整电台回归。

## 太空弹球：图片、物理计算与合成音效

[源码](https://github.com/YeatsLiao/ai-passport-pinball)，版本
`60e331f17f89adf52fe4dedb0aa1e63daec28f4e`。7 个原始 C 单元和 258,072 字节图片包
均未修改，候选仓库 Git 干净；暂未定位到官方社区玩法编号。公共宿主增加真实的
`cosf`、`expf`、`fabsf`、`sqrtf` 提供者，自动导出达到 536 项。

四条路径通过：独立包、从 `0x0` 直接刷写混合包、混合包作为插件安装、纯插件。
真实 LCD 截图验证设置调整、图鉴翻页、蓄力发球、运动画面、暂停菜单选择和恢复。
冷重启后保存的设置面板像素一致。真实音频到达 I2S，但 codec 强制静音且 PCM 归零，
不声称验证实际音质。

原游戏轮询 ADC，且在标题开始的 0.3 个游戏秒内忽略输入。脚本先等待页面稳定并验证
菜单选中项，再使用 500ms 按下样本和 5 秒暂停长按。早期短脉冲/过早输入以及 USB
记录被拆行的尝试保留为测试工具问题；旧 BSP 的原生 ADC 配置禁用其不具备的回调边沿
命令。没有为通过测试而重写游戏逻辑、渲染或物理规则。

归档：`dist/community-link-validation/11d86f83df99/`。宿主 APP 2,655,360 字节，
独立包 1,095,936，插件 346,704，混合包 1,442,656。插件超过 256 KiB，但代码和只读资源
保留在 Flash，不会整包复制到 RAM。加载代码 29,516 字节，RAM 数据 21,508，
直接借用只读数据 261,479。独立版约剩 185 KiB 堆、最大块 112 KiB；插件约剩 122 KiB、
最大块 32--36 KiB。两边 SDK/LVGL 分配器配置不同，不能将差值直接当作框架开销。

Build：PASS。Host tests：PASS（20 组）。Device tests：在上述主流程与持久化范围内 PASS。
完整门禁仍因未改动的小智双语文档规范检查失败。物理触点/短按手感、长期运行、音质、
完整计分规则仍未验证。精确数据见 `diagnostics/community/pinball/report.json`。

## V-Pet：真实 BLE 广播与被动扫描

[源码](https://github.com/zqj372-ops/ai-passport-vpet)，版本
`d53b1da151a3d3893be6ba679436036261267958`。13 个原始 C 单元未修改。
这是明确带适配的应用案例，不声称原版固件通过，也不把业务缺陷算作公共 SDK 能力缺失。

原生入口把栈上 model 的地址传给 `vpet_app_start` 长期保存后返回，存在生命周期缺陷。
额外的构建配置源文件用 GNU `--wrap=vpet_app_start` 保留该对象。主机测试采用候选的
真实模型类型，验证独立存储及参数、返回值转发。

原生 LVGL 私有池还会在孵化时耗尽：匹配探针 ELF
`f974b41e1728455deb619830ae8cc87c122a43a2a520730494b58560eee892e8`
记录申请 88 字节、空闲和最大块均为 48、最高使用 21,272、可用池 21,320。
原生测试配置将私有池从 24 调到 64 KiB；插件本来就使用宿主 C 库分配器。
两项修正均不修改候选业务源码，也不属于通用 RAM 精简收益。

公共宿主启用 NimBLE 观察者角色，并补充 8 项导出：广播状态/停止、扫描/取消、标签
文本、时钟以及定时器暂停/恢复。自动导出达到 544 项。APP 2,657,232 字节，仍可使用
原来的 `0x289000` factory 分区。IRAM、DRAM dummy/data/BSS 的地址和大小与此前弹球
宿主完全一致；按用户要求，没有在这些补充后重复跑旧玩法。

独立版和纯插件主流程通过训练、状态、按键及真实双向 BLE 射频验证。
Windows 发送固定的 17 字节测试战斗卡，ESP 收到并进入对战；Windows 同时收到 ESP
的 VPET 厂商广播。这是真实无线收发，没有注入 GAP 事件。重启后训练、对战计数区域
像素与存档前一致。不要求已经结算的战斗画面继续变化，也不穷举战斗规则。

两条混合包路径实际完成刷写/提取安装、启动、按键和 LCD 冒烟，不重复完整业务流程。
原生轮询式 stdio 曾在音频日志期间丢失 ACK，测试自有 USB 现采用与宿主一致的缓冲
控制台；应用自有 USB 仍由应用管理。

归档：`dist/community-link-validation/26b2b3920f2d/`。宿主 APP 2,657,232 字节，
独立包 1,167,968，插件 134,412，混合包 1,302,396。插件代码 29,192 字节、RAM 数据
732、直接借用只读数据 45,539、重定位只读缓存 3,012。BLE 活动后，独立版剩余堆
82,788、最大块 73,728；插件剩余堆 103,640、最大块 34,816。SDK 和原生私有池配置
不同，不能直接用差值计算框架开销。

Build：PASS。Host tests：20 组核心检查和保活适配检查 PASS。
Device tests：在上述适配和范围内 PASS；原版原生失败记录保留为 FAIL。
完整门禁仍因原有小智文档检查失败。双实体 ESP 对战一致性、全部进化/长期养成、
物理触点和音质未验证。证据见 `diagnostics/community/vpet/report.json`；匹配的
内存池失败 ELF/MAP 保存在 `diagnostics/community/vpet/pool-probe/artifacts/`。

## 容量预筛：侨批问答

[社区玩法 753](https://ai-passport.folotoy.cn/plays/753/)，
[源码](https://github.com/liangdabiao/ai-passport-qiaopi)，版本
`5871a0ecbc4d1d2af230a1b00dc812e16bd9dd18`。仅音频 blob 就有 5,298,976 字节，
超过当前最大连续安装区的 4,841,472 字节，尚未计入 ELF 和字库开销。
在保留完整资源的条件下预筛排除；没有据此声称真机失败或 RAM 不足。

## 此前的音频与打印应用结果

两个上游仓库的 Git 状态均干净。录音笔的 8 个 C++ 单元、打印管家的 15 个 C
单元均未修改业务源码。最终混合 BIN 已按精确文件哈希直接刷入独立运行，
其插件部分也通过 ZION 安装运行；另外单独安装并运行了仅插件包。

| 应用 | 插件文件 | 混合 BIN | 主流程结果 |
| --- | --- | --- | --- |
| 录音笔 | 132,776 字节 | 1,789,272 字节 | PASS：录音、保存、解码播放、音量按键、HTTP 上传 |
| 拓竹打印管家 | 709,920 字节 | 2,966,512 字节 | PASS：TLS/MQTT 遥测、页面/菜单、APSTA、配网与扫描 HTTP |

删除录音笔回收 2,334,720 字节，包含私有 2 MiB FAT 区及代码缓存；重新安装后
可用空间精确恢复。安装信息也已计入这些资源，而不只显示文件字节数。

Build：PASS；Host tests：PASS；Device tests：在下述范围内 PASS。
完整的继承检查仍被小智导入文档的中英文规范问题阻断，未修改这些无关检查。
设备已停留在 ZION 列表，安装了这两个应用，没有默认玩法。

已验证的本地测试包位于 `dist/community-link-validation/25d993e47029/`，
包含 `manifest.json`、`test-report.json`、匹配的 ELF/MAP 及保留设备身份区的
分段刷写计划。`host/*-0x0.bin` 截止 `0x270000`；另外的 store 与 OTA 写入
列在清单中。此测试配置包含 USB 测试控制和本地 MQTT/TLS 替身重定向，
不是社区发布包。已检查归档二进制不包含提供的 Wi-Fi 凭据。

实验使用完整的上游业务源码，不修改函数实现、不替换应用 UI。混合包前半部分
独立运行原应用；ZION 尾包包含按当前宿主 SDK 编译的 ELF。本轮 USB 测试由电脑端
`zionctl` 提取尾包再传输，不安装前半部分重复的独立固件。本轮 USB 验证
不代表已验证官方小程序识别此尾包。

| 应用 | 源码 | 覆盖能力 |
| --- | --- | --- |
| [28 号录音笔](https://ai-passport.folotoy.cn/plays/28/) | [Acolevia/ai-passport-recorder](https://github.com/Acolevia/ai-passport-recorder) | C++17、Opus、麦克风/I2S、FAT/磨损均衡、后台 HTTP 上传 |
| [1209 号随身拓竹打印管家](https://ai-passport.folotoy.cn/plays/1209/) | [ericwubeta/ai-passport-bambu-monitor](https://github.com/ericwubeta/ai-passport-bambu-monitor) | C、大字体/图片、开机动画、Wi-Fi/APSTA、TLS/MQTT、HTTP 配网页 |

28 号社区作者是 Acolevia，页面当前没有填写 GitHub 链接。测试使用作者的源码
仓库，不声称最新源码提交与社区当前下载的 BIN 字节一致。

## 构建层改动

配置位于 `examples/link-apps/recorder` 和 `bambu-monitor`，列出原版全部源文件、
头文件路径、编译定义、栈与存储需求。候选应用的业务源码不包含 ZION 头文件，
也不显式调用 ZION SDK；`app_main`、构造函数及公共导入包装由工具另外生成。

录音笔的独立分区表和插件存储申请都使用 2 MiB 录音空间。这是构建/布局配置，
小于上游的 6.38 MiB，以便在 8 MiB Flash 上与 ZION 共存。原来的 `recordings`
分区名、`/rec` 路径、文件格式和业务存储代码均保留。

新增公共能力：

- 接通 HTTP、TLS/MQTT、FAT、Opus、C++ 容器及库辅助函数的精确 SDK 导出。
- 使用 Picolibc 官方支持的 errno 访问函数编译模式，访问宿主当前任务的真实
  线程独立 errno；尚不提供应用自定义 TLS、异常与 RTTI。
- 不被重定位修改的只读节直接借用 Flash 映射；含指针、需要重定位的只读节
  仍复制到可写内存。
- 按实际指令 Flash 地址重定位代码，写入插件占用空间尾部的预留区，回读校验、
  同步后执行，再释放临时 RAM 缓冲。旧包仍可使用 RAM 代码方式。
- 可执行代码按节申请重定位缓冲，不要求整份代码占一块连续 RAM。
  指令与数据映射使用分开的 MMU 页，进入应用前检查地址确实可执行。
- 插件可声明私有 FAT 存储，安装时分配，更新时保留，删除时一并回收；通过标准
  分区查找接口暴露当前应用自己的存储。
- 启用 LVGL 默认主题以支持普通应用样式，并按打印机 BSP 需求使用 10 KiB
  LVGL 栈。验证用 USB 按键任务使用 4 KiB；进入配网实测消耗约 2.3 KiB，
  超出了原来的 2 KiB。

不设置文件/代码/数据字节配额。实际 Flash 布局、MMU 映射和 RAM 申请仍须成功，
包括重定位时的临时缓冲。代码缓存及所需的 MMU 页对齐空隙会增加 Flash 占用，
不会重复安装独立固件里的完整 SDK。

## 实机验收范围

记录、精确哈希与 LCD 刷屏截图保存在 `diagnostics/community/`。测试覆盖主要
页面、上/下/OK/长按和底层能力主流程，不穷举业务规则。通过真实按键回调路径
注入输入；没有测量物理开关接触/电气行为，也没有测量声音品质。

录音笔使用真实采集、Opus 编码、文件保存、解码播放，以及未经修改的 HTTP
上传器和服务器。打印机使用真实 Wi-Fi、TLS 与 MQTT，由局域网替身发送模拟
遥测；不代表验证了实物打印机、拓竹账号、短信登录或拓竹云可用性。

`tests/standalone-driver` 仅通过构建链接增加测试控制；
`tests/bambu-network-driver` 把 MQTT 传输指向本地 TLS 替身。两者是验证组件，
不是产品功能。Wi-Fi 凭据另外写入设备 NVS，不放入固件包或源码。
电脑没有无线网卡，因此手机连接设备热点的体验仍未验证；HTTP 配网页通过
设备的 STA 局域网 IP 实际访问。

## 重现插件与混合包构建

激活 ESP-IDF 6.0.1，先构建匹配的运行时，再运行：

```powershell
python tools/build-idf-compat.py examples/link-apps/recorder --host-build build-layout-apps --output build-link-recorder --standalone-full build-recorder-standalone/recorder-full.bin
python tools/build-idf-compat.py examples/link-apps/bambu-monitor --host-build build-layout-apps --output build-link-bambu --standalone-full build-bambu-standalone/bambu-full.bin
```

每个输出包含 `app.zion`、`app-dual.bin`、`app.elf` 和源码/宿主清单。
独立工程使用各自支持的 SDK：录音笔 6.0.1，打印机 5.5.3。
原生插件绑定精确的宿主契约，契约改变后需要重建。每次开机只运行一个应用；
退出/切换通过关机再开机，上键按住开机可跳过默认玩法。

## 逐款验证：iPhone Agent 的 HID/USB 场景

本轮候选为[社区 1251](https://ai-passport.folotoy.cn/plays/1251/)，源码来自
[lewangdev/ai-passport-iphone-agent](https://github.com/lewangdev/ai-passport-iphone-agent)，
提交 `1ac4c87f23915b057bb79544952fefdf44cfaede`。11 个原始 C 编译单元（含中文字体）
保持不变，Git 状态干净。仅增加构建描述 `examples/link-apps/iphone-agent/compat.json`。

| 产物 | 字节数 |
| --- | ---: |
| ZION DeviceTest 主程序 | 2,474,416 |
| 独立合并固件 | 2,324,672 |
| 仅插件包 | 822,364 |
| 独立 + 插件混合 BIN | 3,147,052 |

测试宿主比此前增加 31,408 字节，仍包含内置最小小智、USB 测试控制及此前的本地 MQTT
重定向，不是正式发布版本。ELF SHA-256 为
`70ca4547ea385cdf8cf2089f13ee5d6dec49484f43cbd90604647a8d2b33d41a`，契约为
`c4be2e434eda1fbf4b52afb48d31ae32`。此前音频/打印应用归档保留旧契约，不能直接
当作适配当前宿主的包，需要重新构建。

加载器从 Flash 映射 758,337 字节只读数据，复制 2,592 字节数据，16,388 字节代码通过
20,480 字节指令缓存执行。安装起点 `0x360000` 的实际占用为 872,448 字节，含对齐与缓存。
文件大小不等于运行 RAM 或完整安装占用。

在不改应用的前提下补齐了两项公共能力：

- 包特征位 `8`（`bluetooth: true`）保留可重新初始化的 BLE 内存。ZION 停止并反初始化
  自身蓝牙栈，但跳过不可逆的 `esp_bt_mem_release`；不需要 BLE 的应用仍释放该内存。
  启用 HID、绑定持久化并新增 47 个精确 SDK 导入，生成的公共符号注册表增至 356 个。
- 特征位 `16`（`usb: true`）把 USB 读取权交给应用，ZION 控制台退出，避免抢读数据。
  初始化复用已有驱动；测试指令通过同一个读取通道分流，正式应用仍使用自己的原始协议。

独立与插件分别使用 SDK 5.5.3 和 6.0.1。独立分区配置保护 `0x356000` 的 `cardid`。
Wi-Fi 凭据单独写入 NVS，不进入固件产物。

真机主流程包括 USB hello/ping、暂停/恢复、上下切屏、长按上键清除绑定、长按 OK 不重启，
并取得真实 LCD 截图。应用通过真实 Wi-Fi/TCP 连接本地只发送 ping 的服务。
真实 Windows 蓝牙适配器发现 HID UUID `0x1812`，完成连接、配对，设备输出
`link encrypted, HID ready`。中性 `scroll 0 0` 指令经真实 HID 提供方返回 `@ok`，
连接中 OK 暂停/恢复也正常。未发送点击、位移或文字。Windows 会保留 HID GATT 访问权；
iPhone、AirPlay 和 AI 代理完整闭环尚未验证。

独立与插件模式在 Windows 配对期间都曾输出上游 `adv start failed: 6`，随后加密成功，
断开后恢复广播。保留此基线警告，不宣称日志完全无错误。可用堆随网络/蓝牙活动变化，
记录值还包含 SDK/配置差异，不能全部归因为 ZION 本身。

证据位于 `diagnostics/community/iphone-agent/`。早期脚本曾因 `paused` 日志早于按键注入
ACK 而超时，现已兼容两种顺序。LCD 缺行时重新抓取完整帧，不拼接残缺截图。

纯独立复测还记录了一次 Windows 配对失败、一次扫描未发现目标；设备重启后配对、加密、
中性 HID 报告及暂停/恢复均成功。证据不足以把初次失败归因于 ZION 或 Windows，保留全部
尝试记录。最终恢复到 ZION 列表，安装此应用且不设置默认玩法。对应测试归档为
`dist/community-link-validation/70ca4547ea38/`。
最终安装地址 `0x270000` 也已单独加载并通过同样主流程，其安装占用与此前
`0x360000` 的布局相同。
## Octo Radio：音频和网络边界验证中

候选：[社区玩法 1562](https://ai-passport.folotoy.cn/plays/1562/)、
[上游仓库](https://github.com/lison-xulicong/octo-radio)，版本
`70e071426d018bf56cf17e379acab93002330c5b`。保留全部 19 个原始 C/C++ 编译单元、
完整章鱼动画及内嵌配网页面；仅增加外部编译链接配置和测试设施。

独立版已经通过电台选择、MP3 解码、抖动缓冲、I2S 初始化、动画、短按键及长按设置
主流程。插件可以加载，界面和 Wi-Fi 初始化正常，但播放**尚未通过**：解码器申请
8 KiB 内存时，剩余最大连续块不足。关闭受支持的 Wi-Fi IRAM 优化、使用六个静态
接收缓冲区后，列表空闲堆由约 47 KiB 增至约 55 KiB，但混合包插件模式解码时虽然
总空闲约 15 KiB，最大连续块仍为 7,680 字节。音频验收仍然失败，这不是包大小限制。
这些 Wi-Fi 设置以吞吐量换取内存；尚未对最大下载吞吐量进行基准测试。

这个候选暴露出的基座问题包括：

- ELF 可执行段逐段暂存、写入 Flash 后释放；实测峰值暂存块 11,572 字节，
  不再将 119,076 字节代码整体保留在 RAM。
- 从已校验 Flash 按需读取段头，1,193 个段的映射表由 52,492 降至 14,316 字节。
  没有新增任意包大小限制。
- 元数据提交仅申请所需工作区 13,376 字节，避免一次占用全部 23,624 字节挤占
  LVGL 内存；保留 NOR 故障恢复和持久化顺序测试。
- USB 使用带缓冲驱动及有超时的整条确认消息发送。失败记录显示设备收到
  515,328 字节，而电脑只收到 514,304 字节的确认；修复后首轮完整传输
  1,449,488 字节成功。
- 支持没有注册回调、而是自行轮询 ADC 按键的应用。

实验基座 factory 现为 2.5 MiB（`0x10000..0x290000`），身份区仍为
`0x356000..0x35a000`；这不是正式 2 MiB 更新器的升级路径。
大混合包尾部跨越身份区时，独立模式测试仅写入已核验的原生分段，不整体写入尾部。

用户休息期间，DeviceTest 和独立版测试组件强制音量为零并静音，实际 I2S 发送零 PCM；
解码输入与应用缓冲区不变。主机测试覆盖强制静音、部分写入和错误传递。
正常发布版不链接该拦截器；没有使用麦克风或仪器测量实际声压。
独立包和混合包独立启动的真机日志都包含 `CODEC_MUTED volume=0 mute=1`、
`I2S_ZERO_OUTPUT bytes=1152`，同时完成了实际 22,050 Hz 单声道 MP3 解码和缓冲预填充。
两条独立启动路径均在静音配置下通过按键、设置及动画主流程。

证据位于 `diagnostics/community/octo-radio/`。目录与 MP3 源使用本地可控测试端，
HTTP 解析、MP3 解码和 I2S 在真机执行。公网电台可用性与主观音质未验证。
纯插件重测通过短按、长按、切台、暂停恢复指令、菜单及动画，MP3 播放仍然失败。
首次菜单截图因解码失败日志穿插，320 行中缺了 8 行；保留失败记录，暂停播放重试后
重新抓取整帧，没有补造缺失像素。

准确测试包位于 `dist/community-link-validation/0cdcdf426178/`。
宿主 APP 为 2,611,632 字节，独立合并 BIN 为 2,804,528 字节，插件为 1,449,488 字节，
混合 BIN 为 4,254,032 字节。全部 16 个白名单文件已读回核验哈希，并检查不含测试
Wi-Fi 的私密值。宿主和独立镜像都故意保持静音，不是正常发布构建。
报告明确保留插件音频失败结果；设备已返回 ZION 列表。

## 阅读应用：HTTP 配网、HTTPS 和分页持久化

[社区玩法 1427](https://ai-passport.folotoy.cn/plays/1427/)、
[上游源码](https://github.com/RadyWu/ai-passport/tree/feature/bathroom-short-read)，
版本 `f2142e3bbab18a1831e190b7a22c37986217ae36`。全部 11 个应用与字体 C 编译单元
保持原样，上游 Git 干净；外部兼容头映射 IDF 6 更名的 GPIO 唤醒 API，没有修改业务代码。

旧 DNS 示例依赖即使只开 IPv4，也使用 IPv6 类型。插件改为链接 ZION 实际 AP DNS
服务，编译期对照原配置结构校验 ABI。适配器支持 `WIFI_AP_DEF` 或其明确 IP 的单条
通配规则，其他配置返回 NULL。主机测试覆盖报文模糊输入、范围及启动停止失败。
真机日志确认 APSTA 转 STA 过程中启动、停止共享 DNS；手机实际连接 AP 未测。

基座现有 500 个自动导入符号及手动服务，新增实际 SDK 睡眠、SNTP、NVS blob/i64、
字符串和 LVGL 能力；SNTP 支持三个服务器槽位。缺失的 BSP 生命周期接口复用官方
版本 `3d685a90bdb96dd8526d84edde1e63fec25b460e`，保留实验版显示和按键改动。
官方音频故障恢复、休眠读回主机测试已经针对复用实现执行。用户无法配合时不执行
实际深睡眠，避免 USB 断开后不能继续测试。

本批 factory 为 `0x284000` 字节，结束地址 `0x294000`，只比此前 2.5 MiB 增加
16 KiB；APP、bootloader、分区表必须一致，身份区不变。主机测试覆盖 factory
结束地址不按 64 KiB 对齐、应用与数据分配及准确回收。
重复更新发现空间误判：`0x360000..0x500000` 有 1,703,936 字节，实际只需
1,695,744 字节，旧最坏对齐估算却申请 1,708,032 字节。分配器已经保证起点按
64 KiB 对齐，安装器改为预留精确大小、复核选定地址，新包校验和提交成功前保留旧包。
此前被拒绝的同一空闲区随后成功完成安装。

独立包和混合包独立模式通过 HTTP 配置、已保存文章显示、翻页、长按亮度与 OK 弹窗。
混合包重启后正文像素与此前保存的独立版文章完全相同。混合包插件模式也通过原始
HTTP 保存、HTTPS/JSON/文章持久化及按键流程；最终批次纯插件和归档结果另记于下方。

外部 AI 服务使用本地 HTTPS 端和假密钥；真机执行实际 TLS 1.2、证书校验、接收回调、
JSON、NVS 和渲染，测试端观测套件为 `ECDHE-RSA-AES256-GCM-SHA384`。
时钟种子和预检查域名属于测试替换，不代表真实 SNTP 或付费 DeepSeek 验收。
不同 SDK 和配置下的空闲堆差异不能直接当作插件框架的纯开销。

首次工具误把电脑测试端 IP 当设备地址，现已要求明确传入设备 IP。
不完整截图保留失败记录，只允许重新抓整帧，不补造像素。
像素行改为整条缓冲发送，避免逐字符输出丢字节；这些测试设施问题不代表界面缺陷。

最终阅读批次：`dist/community-link-validation/3cb3cb00b308/`。
宿主 APP 为 2,622,128 字节，独立 BIN 为 3,014,368，插件为 1,587,344，混合 BIN 为
4,601,728。四条运行路径都通过各自主流程；纯插件重启后也能恢复保存的文章。
独立、混合独立、混合插件、纯插件及插件重启后的正文像素完全相同。
最终两条插件路径的 16 张截图均有完整 320 行，没有重抓。

随后同一基座用未修改的原打印机插件完成本地 TLS/MQTT 音频回归，保持
RUNNING 转 FINISH 状态，实际执行新 BSP 的 16kHz codec 打开、
`CODEC_MUTED volume=0 mute=1` 和 `I2S_ZERO_OUTPUT bytes=512`。
此前测试端在界面轮询前把 FINISH 覆盖为 RUNNING，保留为刺激不足记录，不算驱动失败。
回归插件、ELF 和四张阅读截图已加入测试包。全部 23 个白名单文件均读回校验哈希，
不含私密 Wi-Fi 值和实际测试端私钥；PEM 解析器的公开分隔字符串不等于私钥。
设备已返回管理列表。

Build：PASS。Host tests：PASS（16 组及不支持导入拒绝测试）。Device tests：PASS，
范围为上述阅读主流程与静音音频回归。完整 gate 因未修改的小智双语文档检查失败。
未验证：实际深睡眠、真实 SNTP/付费 AI、手机 AP/官方小程序投递、物理按键触点、
主观音质、长时间及最大吞吐量测试。

## DiDaaa：实时 UDP 键控与 MQTT 信令

[社区玩法 629](https://ai-passport.folotoy.cn/plays/629/)、
[源码](https://github.com/subooku/didaaa)，版本
`502db4b9e254eabcbaeeabefb831cee9732e051f`（固件 1.1.18）。七个原始 C 编译单元、
原始箭头位图和 CA 保持不变，上游 Git 干净。配置保留原始符号比较警告，仅对
`cw_net.c` 的原始 IP 地址转换禁用严格别名优化，没有修改业务文件。

该应用需要 PRESS/RELEASE 原始边沿和第四档并联按键。生成入口提取原始 ADC 表，
真机日志确认实际注册四档。默认三档、配置回滚和回调所有权均有主机测试。
现有基座包含 532 个自动导入符号及手动服务，增加固定版本 ESP WebSocket client 1.8.0
及原始 Montserrat 16。factory 为 `0x289000` 字节，结束地址 `0x299000`；
当前 APP 为 2,654,016 字节。

独立构建使用外置紧凑双 OTA 表：NVS/PHY 地址和容量不变，OTA 元数据放在
`0x10000..0x12000`，OTA 0 从 `0x20000` 开始，OTA 1 保持 `0x400000`。
这样混合包不会因尾部擦除填充占用数兆字节。完整混合包 1,957,304 字节，已经实际
从 `0x0` 整包写入，身份区保留。插件不支持 SDK 独立固件 OTA 写入；独立模式仍用
实际 SDK OTA 接口。

独立、混合独立、混合插件、纯插件四条路径均通过。本地测试端实际进行 UDP 认证和
MQTT 信令，提供台站名单、频谱占用、时间并发送远端点码。设备时间戳实测短音
89–90ms、长音 279–280ms；频率从 7,100,000 调到 7,101,000Hz 后恢复。
界面实际解码 TX `E T`、RX `E`；虚拟组合键切换在线/离线，2.2 秒按住进入、退出原始
2 秒阈值的设置菜单。实际观测到 codec 静音和零 PCM I2S 写入；物理触点未测。

保留两类早期测试设施问题：等待电脑确认曾把请求的按住时间延长至 603ms，现改为
设备任务定时；静音器逐 PCM 块写寄存器也会争用 I2C，独立版曾丢失边沿。
最终配置只在打开与设置音量时强制静音，I2S 始终置零，不再逐块写 I2C。
最终四条路径在该低负载配置下通过，不代表所有 UI/网络负载下都已验证。

原阅读应用也已针对新基座重建，重启后再次通过 HTTPS、文章保存、翻页与按键，
回归默认三键路径。19 组主机测试通过；完整 gate 仍因未修改的小智双语文档失败。
公网服务、WSS、独立 OTA/回滚、物理触点/ADC 噪声、主观音质和长时间时序未测。
证据位于 `diagnostics/community/didaaa/` 与 `diagnostics/community/reader-532-regression/`。

最终匹配测试包：`dist/community-link-validation/eff4dc42ef7f/`。
宿主 APP 2,654,016 字节；DiDaaa 独立 BIN 1,747,296；插件 209,992；混合 BIN
1,957,304。计入重定位缓存及对齐后，插件实际安装占用 323,584 字节。
测试包包含同基座阅读回归插件、匹配 ELF/MAP 和五张真机截图。
设备停留在 ZION 列表，保留两个外部玩法，没有设置默认启动。

