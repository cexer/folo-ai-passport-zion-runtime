<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

## Zion 字体

`fonts/zion_font_14.c` 为 14px、1-bit、6873 字符字体，由 Google Fonts Noto Sans SC
生成，许可为 SIL OFL 1.1。来源、哈希、许可和字符清单见 fonts 目录，重建脚本为
`../tools/generate-zion-font.py`。覆盖 GB2312 汉字、ASCII 和部分标点，无字距表及压缩；
启动器不链接音频资源。安装页使用 `images/folo_mini_code_208.c`，将官网公开的
微信小程序码编码为 208×208 的 I1 图案，加 8 字节调色板共 5,416 字节。
原始 WebP、单色预览、原始数据和哈希清单保留在 `images/` 下；使用
`tools/generate-zion-mini-code.py` 转换，可选 Python 依赖见
`tests/realdevice/requirements-zion-pixels.txt`。这种服务端签发的小程序码不能用
包含网页网址的普通二维码代替。来源为 FoloToy 官网；来源未单独声明资源再分发许可，
其权利不等同于仓库代码许可。图案不包含设备专属秘密。

正式界面文字选用 `fonts/zion_font_ui_14.c`（14px、4bpp、原生界面字符子集）。
电量百分比选用 `fonts/zion_font_ui_10.c`（10px、4bpp、可打印 ASCII），
列表编号、大小、版本行保留原来的 Unscii 8 字体及字号。
回退链为 10px 界面字库 → 14px 界面字库 → 完整 14px／1bpp 字库，玩法名称中
不在界面子集的字仍保留原有完整字库覆盖。这两个未压缩子集使用同一 Noto Sans SC
源字体、600 字重及 OFL 许可。字符、回退及哈希见 `fonts/zion-ui-font-manifest.json`，
使用 `../tools/generate-zion-ui-font.py` 重建。`fonts/zion_font_ui_inventory.h` 用于
测试固件调用真实 LVGL 解码器并逐像素比较 A8 结果，不链接到正式版。
配网结果页同时嵌入现有的 `images/folo-mini-code-208.png`（2,210 字节），复用同一小程序码及来源，
用于离线展示；此兜底入口不需要新增图片或访问外网。
