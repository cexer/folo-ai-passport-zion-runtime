[English](README.md) · **简体中文**

# Passport OS 原始源码候选

上游：https://github.com/manchunx7-bit/passport-os，版本 `b70879b`。
这是多应用源码仓库，不是单独的 voice 仓库。编译与真机验收分别记录在
`diagnostics/community/passport-os/`；存在此配置不代表测试通过。

保留原 main、Muse、Noise、Wi-Fi 组件的编译单元、默认 Muse 图片和内嵌网页、
城市资源。原有内存诊断和专用录音、播放、反馈音频 BSP 在插件内私有编译，
底层仍调用共享 SDK 驱动，不编译第二套显示、I2C 所有者。缺失的 SDK 字体从同一基座的 LVGL
源码作为插件私有数据编译。未修改业务源码或资源。

四个链接 wrap 来自上游 main/Muse 的 CMake：Opus 调用方存储和 TLS 分配所有权。
这是原应用的构建语义，不是新增修补，仍在清单中明确记录。
基座还需要将 SDK 内部调用分发到这些包装函数。仅包装插件链接引用，不能接管
已经链接到共享库内部的调用。
`muse_proxy_defaults.h` 对应上游公开发布配置：主机为空、端口为零，不固化维护者代理。
GCM 头文件桥使用 IDF 6 当前 TF-PSA 的真实声明和结构，加密头文件纳入精确基座 ABI 指纹。

输入使用原电压表轮询 ADC，不使用 BSP 事件回调。SPIFFS 申请私有 512 KiB
`storage` 空间；原生布局的 2 MiB 预留无法满足 runtime 的连续 Flash 分配，原失败证据已归档。
没有删资源，但可上传容量减少。原完整固件的身份分区地址不同，严禁将其直接
整包写入并覆盖 runtime 的 `0x356000..0x35a000` 保护区域。
