[English](community-validation-paused.md) · **简体中文**

# 社区实测交接——2026-10-28 暂停

按用户要求暂停。未经重新要求，不继续拉新候选或真机测试。本轮未提交、推送或发布。

## 已验证检查点

- 新设备：COM7，ESP32-C3，8MB。静音 DeviceTest 宿主 ELF：
  `7fc7dd1f56116e0d53a73df8f4d5cf981988e45dff643850d1146923058e2fa0`。
- Passport OS / 438：`b70879b` 业务代码不变；纯插件、混合 BIN 提取插件两条路径，
  均通过实际启动、上下选择、短按/长按 OK、秒表、运行中俄罗斯方块和设置。
  已检查实际 LCD 截图，见 `diagnostics/community/passport-os`。
  独立模式此前在旧设备使用相同 APP 字节、保留身份区的重排分区布局通过主流程。
  未把原始独立/混合合并镜像刷入受保护身份区。
- 新设备身份区 `0x356000..0x35a000` 前后回读逐字节一致。
- 保留 MEM-08 至 MEM-11，包括每页 32 节的元数据、提前预留并复用暂存区、
  合并可写节分配。438 从 698 个分配块降至 16 个；启动空闲/最大连续块为
  30,696/12,800 字节，纯插件主流程后为 44,476/17,408。
- 对应固件、插件、调试文件：
  `dist/community-link-validation/passportos-packed-arenas/7fc7dd1f5611`。
  这是本地 DeviceTest 验证构建，不是公开发布包。
- 完整检查仍因继承的小智 Markdown 语言/配对规则失败，按用户范围未修复。
  针对性主机检查覆盖 30 组。
- 连续计数仍为 **0/3**：本例改过基座。

## 下一候选，尚未通过

PokeWalk / 234 按实际点赞倒序接口选出，仓库在 `../candidates/pokewalk`，
提交 `dfc9098465b32a68f5218eb33fcd648ad1a7207a`，业务代码保持干净。
`examples/link-apps/pokewalk` 只是构建准备配置：90 个原始编译单元已编译成
2,628,468 字节 ELF 导入计划，仍缺 22 个底层提供者。
未生成可运行安装包，未构建独立固件，未作真机通过声明。

主要缺口：7,792 字节旧存档工作区必须进入 RTC 内存，多持久化分区，
以及原代码对原始分区地址的检查。不能省略存储或把 RTC 工作区悄悄塞进主 DRAM。
最大原始可写对象 39,976 字节，可写节总计 142,621 字节；这些是 ELF 测量，
不是设备运行时余量。

未完成的 RTC 加载器实验已移出活动核心文件，单独保存为
`../diagnostics/pokewalk-paused/rtc-experiment-unverified.patch`。
它**尚未验证通过**：主机检查在既有 Passport OS ELF 上失败。
加载器、头文件和宿主文件已逐字节恢复至已验证检查点；恢复证据在同目录
`restored-verified-source.json`。导入计划：`build-link-pokewalk/import-plan.json`；
完整编译日志：`../diagnostics/pokewalk-import-plan-build.log`。

## 恢复工作时

重新识别设备及端口，不假定仍是 COM7。先读 runtime AGENTS 和本检查点，保留已有修改，
先在主机解决 RTC、分区契约，再刷 PokeWalk。按点赞排序、一款一款继续，直到连续
三款无需修改基座即可通过。保持静音，保留身份区；模拟网络或编译成功不能当作真机验收。
