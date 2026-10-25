火炬之光重燃 (Torchlight Rekindled)

基于 GreenSwampDweller/torchlight-rekindled 1.0.1 的 Android 版改写版启动器。

| 项目 | 说明 |
| --- | --- |
| 基础版本 | torchlight-rekindled 1.0.1 |
| 平台 | Android |
| ABI | arm64-v8a |
| 签名 | debug |
---

简介

本项目是对原版 Android 启动器的改写版，主要做了三件事：应用汉化、读取 pak.zip 内中文字库、支持外置 MODS。

---

改动内容

1. 名称：火炬之光重燃

· 应用名（桌面图标下的名字）改为 火炬之光重燃。
· 启动器界面整体改为中文。

2. 中文字库：读取 pak.zip 里的中文

引擎侧

· 界面字体的字形范围从 ASCII + Latin-1（U+0020–U+00FF） 扩展到 U+0020–U+9FFF，覆盖 CJK 汉字区与中文标点。
· 字体光栅尺寸相应调整，让 4 万多个字形能塞进一张 4096×4096 的字体图集。
· 文字绘制本身走 OGRE 的 UTF-8 解码，因此 pak.zip 里的中文只要有字形，就能正常显示。

启动器侧

· 新增「中文字库」检测项。启动前会：
  · 直接打开 pak.zip；
  · 解析其中 media/UI/*.ttf 的 TrueType cmap 表；
  · 报告有多少个汉字字形，并列出 pak.zip 内所有字体；
  · 没有汉字字形的 pak.zip 会被明确提示。

说明：引擎读取的界面字体是 pak.zip 里的 media/UI/BRLNSDB.TTF（菜单）与 Torchlight Regular.ttf（对白）。国内中文版 pak.zip 通常会把这两个文件换成带汉字的字体，所以「读取国内 pak.zip 的中文字库」= 让引擎把这些字体的汉字字形真正画出来，并让你在启动前确认。

3. 外置 MODS

· 启动器新增「MODS」管理界面：
  · 将 mod 放入游戏文件夹的 mods/ 下（子文件夹，或 .zip / .mod / .pak 文件）；
  · 在启动器内逐个勾选启用。
· 启动时写入 args.txt，以 --extra-data 传给引擎，挂在游戏数据之上。
· 引擎本身支持可重复的 --extra-data，因此新增文件（物品、单位、任务、UI 等）会直接生效。

---

用法

1. 安装 APK
   · 首次需要允许「安装未知应用」；
   · 并授予「所有文件访问权限」。
2. 选择游戏文件夹
   · 需包含 pak.zip 与 music/，例如 /sdcard/Torchlight。
3. 主界面可进行：
   · 更换文件夹
   · 修改分辨率
   · 检测中文字库
   · 管理 MODS
4. 点击「启动游戏」。

---

MODS 的边界

引擎资源索引采用「先挂载者优先」策略，而 --extra-data 是按顺序挂在 pak.zip 之后的，因此：

操作 是否支持
新增文件（物品 / 单位 / 任务 / UI…） ✅ 支持
覆盖 pak.zip 中已存在的同名文件 ❌ 不支持

这一点与桌面版 ModLauncher 合并 pak 的做法不同。

---

已知限制

· 文字测量不准确：引擎自身的 measure_text（用于对白换行与提示框排版）是按字节数计算的，遇到 UTF-8 中文时宽度会算小。中文能正常显示，但对白框的自动换行和少数居中排版的偏移会不准确。该函数位于 .so 中，无源码，无法在不重编译的情况下修正。
· 字库光栅尺寸：为兼顾 4 万多个字形，压到了 13pt。中文在原尺寸下清晰，放很大时略软。
· 签名不同：这是重新签名的版本（debug 签名），与原版签名不同；安装前必须先卸载原版。

---

技术细节：.so 的原地补丁

原版 libtorchlight.so（arm64-v8a）中，界面字体创建逻辑有三处关键调用：

```cpp
addCodePointRange(Ogre::Font::CodePointRange(32, 255)); // 字型范围
setTrueTypeSize(48 / 32);                               // 光栅尺寸
```

对应源码位于 src/render/hud.cpp、menu.cpp、menu_chrome.cpp。

补丁表

文件偏移（虚拟地址） 原指令 改后
0x405440, 0x4054A4, 0x44A1F0, 0x44A254, 0x45E0BC, 0x45E120 movk x8, #0xff, lsl #32 movk x8, #0x9fff, lsl #32
0x405418 movi v0.2s, #0x42, lsl #24（32.0f） fmov s0, #13.0
0x44A1C4, 0x45E090 mov w8, #0x42400000（48.0f） mov w8, #0x41500000（13.0f）

· 第 1 组：将字形范围 0x00FF → 0x9FFF。
· 第 2、3 组：将字体光栅尺寸统一压到 13.0。

---

重新构建

引擎

无需重编译，直接按下文偏移表修改 .so 即可。

启动器

1. 编译 src/android/java/port/android/Launcher.java → classes.dex。
2. 与改好的 libtorchlight.so 一起打包：
   ```bash
   aapt2 link ...      # 生成未签名 APK
   zipalign -f 4 ...   # 对齐
   apksigner sign ...  # 签名
   ```
3. 修改 AndroidManifest.xml 中 android:label="火炬之光重燃"。

---

致谢

· 原版工程：GreenSwampDweller/torchlight-rekindled
· 引擎：OGRE