# 开发

[← 返回 README](../README.md)

C++20 · Qt Quick（QML）· SQLite · libtorrent · libmpv，支持 Windows x64、Linux x86_64 和 macOS（Apple 芯片）。

## 构建

三个平台都需要 Qt 6.8.3（含 qtdeclarative）、CMake 3.24 以上、Ninja，以及 libtorrent 2.0.7 以上（Windows 用 vcpkg 的 2.1；Linux 发行版和 Homebrew 提供 2.0，两者的差异由 `src/net/LibtorrentCompat.h` 处理）。

### Windows

依赖：Visual Studio Build Tools（MSVC x64）、Qt 6.8.3 MSVC 2022 x64（含 qtdeclarative）、CMake、Ninja、vcpkg、Python 3。

```cmd
C:\vcpkg\vcpkg.exe install libtorrent:x64-windows
set QT_DIR=C:\Qt\6.8.3\msvc2022_64
set VCPKG_ROOT=C:\vcpkg
build.bat
```

在中文等非英文 Windows 上，`build.bat` 会先切换到 UTF-8 代码页（`chcp 65001`）：Ninja 依靠编译器输出的包含文件列表判断修改头文件后要重编译哪些文件，这份列表默认按本地代码页输出，Ninja 无法识别，会导致改了头文件却不重编译、程序运行时内存错误。自己调用 CMake / Ninja 时也请先执行 `chcp 65001`。

`build.bat` 默认使用 `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`，其他安装位置请修改脚本开头的 `VS_DIR`。输出为 `dist\MikuLibrary\MikuLibrary.exe`，目录内已包含运行所需的 Qt、libtorrent、OpenSSL、Boost 和插件。

`python tools\package_portable.py` 生成便携 ZIP 到 `..\release_packages\`：先审计依赖，再校验解压后的文件哈希，最后在隔离的用户配置中启动一次确认可用。

### Linux

依赖 libtorrent 2.0.7 以上（Ubuntu 24.04 的 `libtorrent-rasterbar-dev` 是 2.0.10；22.04 的 2.0.5 太旧，需要自己从源码构建）、libmpv、OpenGL 开发包。以 Ubuntu 24.04 为例，Qt 用官方安装器或 [aqtinstall](https://github.com/miurahr/aqtinstall) 装到 `~/Qt`：

```bash
sudo apt install build-essential cmake ninja-build libtorrent-rasterbar-dev libboost-dev libssl-dev libmpv-dev libgl1-mesa-dev libxkbcommon-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build --parallel
```

程序是 `build/miku-library`。`cmake --install build --prefix /usr/local` 装到 `bin/miku-library`，站点目录和 Anime4K 着色器放进 `share/miku-library/`，另外装上 `.desktop` 文件和图标。内置播放器使用系统的 libmpv（`libmpv.so.2`；mpv 0.35 以前是 `libmpv.so.1`，也能用）。

### macOS

```bash
brew install ninja libtorrent-rasterbar mpv
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos;$(brew --prefix)"
cmake --build build --parallel
```

输出是 `build/Miku Library.app`，站点目录和着色器在包内的 `Contents/Resources/`。播放器先找包内的 `Contents/Frameworks/libmpv.2.dylib`，没有时用 Homebrew 的。发布用的包由 CI 生成：`macdeployqt` 放入 Qt 和 libtorrent，`dylibbundler` 放入 libmpv 及其依赖；图标 `assets/macos/AppIcon.icns` 由 `assets/logo.png` 用 `sips` 和 `iconutil` 生成，仓库里不存。

## 各平台的差异

| | Windows | macOS | Linux |
|---|---|---|---|
| 播放器画面 | mpv 的 D3D11 接口，画到 Qt 场景图的 D3D11 纹理 | 场景图改用 OpenGL（4.1 core），mpv 画到 FBO 纹理 | 同 macOS，另把 X11 / Wayland 显示连接交给 mpv，硬件解码免拷贝 |
| 窗口 | 无边框，DWM 阴影、边缘缩放、贴靠（`WindowChrome.cpp`） | 系统标题栏设为透明（`Qt::NoTitleBarBackgroundHint` 加 `WindowChrome_mac.mm`），用系统的红绿灯按钮、圆角和缩放；页面标题下移让出按钮（`Shell.buttonInset`） | 无边框，界面自己画窗口按钮，四边四角的缩放区由 `qml/ResizeEdges.qml` 交给窗口管理器 |
| 托盘、文件框 | 直接调用 Win32（`TrayIcon.cpp`、`NativeDialogs.cpp`），不带 Qt Widgets | `QSystemTrayIcon`、`QFileDialog`（`*_qt.cpp`），程序对象是 `QApplication` | 同 macOS |
| TLS | 固定 schannel | 系统默认（Secure Transport） | 系统默认（OpenSSL） |
| 字体 | Segoe UI，中日文用微软雅黑 / 正黑 / Yu Gothic | 系统字体，中日文用苹方 / Hiragino | Noto Sans，中日文用 Noto Sans CJK |
| 隐藏保存的种子 | 隐藏属性 | 文件名以 `.` 开头 | 文件名以 `.` 开头 |
| 随程序分发的文件 | 程序旁边 | `Contents/Resources` | 程序旁边或 `share/miku-library`（`AppConfig::resourcePath`） |

平台分支集中在 `src/app/Platform.h`（程序对象和图形接口）以及上表列出的文件。mpv 的路径列表在 Windows 用 `;`、其他系统用 `:` 分隔，拼接着色器列表时要用 `QDir::listSeparator()`。

## 测试

macOS 和 Linux 上用 `bash tools/run_smoke.sh build` 依次运行全部测试并汇总结果；没有显示器的 Linux 机器在外面套上 `xvfb-run -a`。

9 组冒烟测试随构建一起生成在 `build\` 下，直接运行即可；`QuickUiSmoke` 会短暂显示真实窗口。

| 测试 | 覆盖 |
|---|---|
| `LocalLibrarySmoke` | 下载记录持久化、自动目录、重复预留、字幕组与季度隔离、画质去重、合集重叠、追更目标、新集计算、追更与自动下载选项、备份与恢复 |
| `DownloadSmoke` | 用本机 HTTP、Tracker 和自生成种子跑完整下载流程：隐藏种子文件、完成后停止与恢复做种、重启恢复、文件选择、删除清理 |
| `OptimizationSmoke` | 站点持久化、全来源调度、分页、请求取消、别名匹配、单实例参数转交、占位文件大小、版本号解析、代理规则、离线放送表、断连重试 |
| `RpcSmoke` | qBittorrent / Transmission / aria2 接口的认证、添加、进度、暂停、恢复、移除 |
| `ExternalDownloaderSmoke` | 外部程序启动参数、种子落盘、重复提交、文件校验与路由 |
| `DirectorySmoke` | 下载目录的创建与校验 |
| `DetailLibrarySmoke` | `DetailController`：字幕组排序与筛选、下载 / 播放切换、本地详情、下载方式弹窗、加载状态、列表增量更新 |
| `SettingsLimitsSmoke` | `SettingsController`：设置项、下载器草稿、应用栏显示、立即保存的常规设置、代理设置、停止做种的二次确认 |
| `QuickUiSmoke` | 加载真实界面走完全部页面和弹窗（出现任何 QML 警告即失败）、换肤过渡、窗口边缘与最大化、托盘、输入法组合输入、列表模型、任务多选、站点筛选、图标 |

测试只使用临时目录和自生成数据，不访问真实影片内容。

## 代码结构

```
src/
  core/   配置、站点目录、数据库、标题解析、字幕组归并、下载记录（LocalLibrary）
  net/    HTTP、Bangumi、各资源站适配与聚合搜索、内置下载引擎、外部下载器、追更检查、海报缓存
  app/    QML 用到的控制器和列表模型，以及无边框窗口、托盘、系统文件框
qml/      全部界面：主题（T.qml）、基础控件（Nx*）、页面与弹窗
```

`core` 和 `net` 组成 `NexusCore`，`app` 组成 `NexusApp`，`qml` 组成 `NexusQml`，都不依赖 Qt Widgets。

几个关键位置：

- `core/LocalLibrary`：集数去重、下载规划、自动目录、下载偏好与追更目标。
- `net/AnimeDownloads`：任务预留、并行添加、旧任务关联、状态同步。
- `net/TorrentEngine`：种子校验与保存（隐藏文件）、文件选择、完成后做种或停止、删除任务时清理种子文件和空文件夹。
- `app/MapListModel`：所有列表的模型。`update()` 只应用最小改动，新一批结果或进度刷新不会重建屏幕上的行。
- `app/DialogHub`：所有询问和提示。一次显示一个，结果走回调，不阻塞事件循环。
- `qml/T.qml`：浅色 / 深色配色和字号，颜色只在这里定义。
- `qml/Shell.qml`：页面共用的边距、工具栏高度和底部导航栏占的高度（`bottomInset`）。列表延伸到导航栏下方，用 `bottomMargin` 让最后一行停在它上方。
- Windows 的 `libmpv-2.dll` 超过 GitHub 的单文件大小上限，没有放进仓库。克隆后从 [shinchiro/mpv-winbuild-cmake](https://github.com/shinchiro/mpv-winbuild-cmake) 的发布页下载 `mpv-dev-x86_64` 压缩包，把其中的 `libmpv-2.dll` 放到 `third_party/mpv/`；缺少时程序仍能构建，只是内置播放器不可用。
- 录制界面：`tools/showreel_capture.cpp` 按固定的 1/60 秒步进动画并逐帧抓图，窗口停在桌面之外、不抢焦点，用来给发布视频录制真实的界面动画。默认不构建，配置时加 `-DMIKU_BUILD_SHOWREEL_CAPTURE=ON`；脚本命令见文件开头的注释。录制播放器时用环境变量 `MIKU_LIBRARY_MPV_SPEED` 放慢 mpv（例如 `0.1667` 配合每帧 100 毫秒），让画面和界面同步。
- 补帧：「平滑」是 mpv 的 `video-sync=display-resample` 加 `interpolation`（`tscale=oversample`）；「运动补偿」把 `PlayerController.cpp` 里的 VapourSynth 脚本写到数据目录的 `interpolate.vpy`，用 `vf=vapoursynth` 加载，只有能加载 `VSScript.dll` 时才提供。Windows 版自带这套组件：`tools/assemble_vapoursynth.py` 校验三个官方压缩包（VapourSynth R82 便携版、Python 3.13.15 嵌入式版、MVTools v29 的 Windows wheel，原文件存放在 `archives/vapoursynth/`）的 SHA256，按 VapourSynth 自己的便携布局拼成 `third_party/vapoursynth`（去掉调试符号）；CMake 复制到 `build/vapoursynth`，`deploy_dist.py` 复制到程序目录。播放器启动时把 `VSSCRIPT_PATH` 指向其中的 `vsscript.dll`（libmpv 先看这个环境变量，再看注册表），所以总是用自带的这份，与电脑上是否装过 VapourSynth 无关。嵌入式 Python 只读 `python313._pth` 里的路径，不受系统 Python 和 `PYTHONPATH` 影响。打包脚本把 `vapoursynth/` 整个原样带上，依赖检查认可其中的模块从嵌入式 Python 加载。渲染接口看不到屏幕，所以 `MpvVideoItem` 把屏幕刷新率告诉 mpv（`display-fps-override`），并在每帧上屏后调用 `mpv_render_context_report_swap`。
- 内置播放器：libmpv 运行时加载（`src/app/PlayerController.cpp`，各平台的查找位置见 `mpvCandidates()`）；Windows 的 `third_party/mpv/libmpv-2.dll` 由 CMake 复制到构建目录，发布时放在程序旁边。Anime4K 着色器放在 `shaders/anime4k/`。画面由 `src/app/MpvVideoItem.cpp` 画到场景图的纹理上：Windows 用 mpv 的 D3D11 接口，其他平台用 OpenGL 接口。设置环境变量 `MIKU_LIBRARY_MPV_LOG` 为文件路径可输出 mpv 日志。
- `qml/Scaffold.qml`：页面骨架：标题、工具栏和固定行（`fixed`）。内容会滚到标题下方的页面把滚动距离传给 `scrolled`，标题和工具栏随内容一起移出窗口；带固定行的列表页自己裁剪。
- `qml/Glass.qml`：玻璃对 `backdrop` 的取样位置写成 `backdrop.mapFromItem(parent, x, y)`，x、y 真正参与计算，绑定才会跟着变。只为建立依赖而读取（`void [x, y, ...]` 或不用的函数参数）会被 QML 编译器优化掉。`liquid: false`（底部导航栏）是窗口背景的同一种材质：模糊 64、放大 1.5 倍、几乎去色，底色用 `T.bgTint`，再叠一层 `T.fill` 和细边，与背景区分但不融入；不画阴影。
- `qml/Glass.qml`：浮层的玻璃（底部导航栏、右上角窗口按钮的衬底），内容层不用。`liquid: false` 时只有模糊、薄薄一层底色和细边，底部导航栏用这种；默认的 Liquid Glass 只剩窗口按钮的衬底在用。它对 `backdrop` 指定的内容层取样、模糊，边缘一圈取玻璃外侧的内容压缩进来模拟折射，再叠高光和阴影；投影只画在玻璃外侧。`MultiEffect` 的渐变遮罩要写 `maskThresholdMin: 0.5` 加 `maskSpreadAtMin: 1`，默认值是硬边；带模糊的效果不要同时做遮罩，遮罩会被拉到模糊的外扩区域。
- 无边框窗口的系统绘制：`WM_NCACTIVATE` 以 `lParam = -1` 交给 `DefWindowProc`，`WM_SETTEXT` / `WM_SETICON` 处理期间临时去掉 `WS_VISIBLE`，否则系统会把 Windows 7 样式的标题栏直接画在窗口上。透出桌面时窗口没有标题栏：去掉 `WS_CAPTION` 和 `WS_SYSMENU`，只保留 `WS_THICKFRAME` 和最小化、最大化样式。否则 DWM 会在整个客户区的框架上画出系统自己的标题栏按钮，有时还会出现 Windows 7 样式的旧标题栏和边框；没有标题栏就无处可画。可缩放边框保留了拖边缩放和贴靠，任务栏最小化、还原正常；失去的是 Alt+空格 菜单和系统的最小化 / 最大化动画。没有标题栏时 Qt 按几何尺寸最大化，`IsZoomed` 不再为真，所以 `WM_NCHITTEST` 同时看 Qt 的窗口状态来关掉最大化时的边缘缩放。（只去掉 `WS_CAPTION` 而保留 `WS_SYSMENU` 时会出现一圈灰色缩放边框。）另外拦下 `WM_NCUAHDRAWCAPTION`（0xAE）和 `WM_NCUAHDRAWFRAME`（0xAF）这两个未公开的主题绘制消息，透出桌面时也不处理 `WM_NCPAINT`，Chromium 用同样的办法挡住 Windows 7 样式的边框。界面自己的绘制层下面，窗口还有一层 GDI 绘图面，系统画在上面的东西（旧式标题栏、边框）会一直留着，透过半透明的背景露出来；在系统模糊上纯黑等于透明，所以透出桌面时 `clearSurfaceLater()` 在激活、改标题、改大小、主题变化等消息之后把整层涂黑。`WM_NCCALCSIZE` 不带 wParam 的那种也返回 0，免得系统按带边框的尺寸计算。DWM 在窗口失去焦点时会把 Acrylic 换成纯色，所以透出桌面时 `WM_NCACTIVATE` 一律按「激活」交给 `DefWindowProc`，模糊在切到别的窗口后也保留。
- 透出桌面的背景：`WindowChrome.setBackdrop` 在 Windows 11 22H2 及以上开启 Acrylic（`DWMWA_SYSTEMBACKDROP_TYPE`，整个客户区作为边框），macOS 用 `NSVisualEffectView`。成功后 `Chrome.translucent` 为真（`T.seeThrough`），界面把背景画成 `T.bgTint`，内容层的分组和输入框用 `T.contentSurface`（半透明，不再是白色），浅色下的次要文字（`text2`、`text3`）加深以保持 4.5:1；菜单、弹窗、提示等浮在上面的层保持不透明。窗口圆角改为系统的 8 像素。透明效果关闭、对比度主题或系统不支持时保持不透明。
- `qml/Backdrop.qml`：封面模糊成的氛围光。透出桌面时自身渐隐，不透出时过渡到页面底色。
- `qml/SmoothWheel.qml`：所有列表的滚轮滚动。它必须铺满列表（`parent: view`）：指针处理器只接收自身范围内的事件，留在 Flickable 的内容层里就是 0×0，收不到滚轮。`pinned` 记录视图是否停在顶部，表头变高时视图据此留在顶部。
- 窗口圆角：窗口带透明通道（`setDefaultAlphaBuffer`），底色是一个圆角矩形，半径 `Shell.corner`（自己画时 24；透出桌面时随系统为 8，系统会裁掉整个窗口的四角）。会碰到窗口四角的东西都要自己带圆角：`Backdrop`、窗口按钮的底色、弹窗遮罩、关闭按钮。不用整窗遮罩，那样每帧多一遍全屏渲染。
- 底部导航栏（`Main.qml` 的 `navBar`）：窗口底部居中悬浮。平时只显示较大的图标，指针移入或键盘焦点进入时展开名称、图标缩小，栏内所有尺寸都跟随 `reveal`（0 到 1）这一个值；指针离开 280 ms 后才收起。窗口放不下名称时（`compact`）始终只有图标，名称在悬停提示里。Logo（`homeButton`）回到本季放送。平时居中；展开时以中间一项（我的追番）为固定点向两侧长出，只在碰到窗口边缘时才让位。页面一直延伸到导航栏下面，所以栏内按钮的 `TapHandler` 用 `ReleaseWithinBounds` 独占点击，栏底下再垫一个 `MouseArea` 吃掉其余点击，否则点击会穿透到下面的海报。选中项是一块透镜（`navThumb`），在离开的项和目标项之间插值，两端都实时测量，导航栏展开收起时它不会掉队。海报的解码尺寸按 40 像素分档，窗口停止变化后才更新（`PosterGrid.decodeWidth`），拖动窗口时不重新加载。
- `qml/ThemeFade.qml`：换肤过渡。先截下整个窗口（包含这一层本身，所以连续切换能接上当前颜色），画面到屏幕后才切换配色，再由 GPU 淡出。

改了 `qml/Shell.qml` 或 `qml/T.qml` 里属性的类型后，要让所有 QML 重新编译（比如 `touch qml/*.qml`）。qmlcachegen 把这些类型写进每个页面的编译结果，而 Ninja 不知道这层依赖：没改过的页面会按旧类型读值，不报错，只是读到错的数字。

## 多语言

界面支持简体中文（原文）、繁體中文、English 和日本語。译文在 `i18n/<语言>.json`，按原文查表（不分上下文），编译进程序资源，由 `src/app/LanguageController` 读取；切换语言时调用 `QQmlEngine::retranslate()` 刷新所有 `qsTr`，C++ 里生成的文字由各控制器的 `retranslate()` 重新生成。

- 改了界面文字后运行 `python tools/i18n.py update`：新原文加进三个语言文件（译文为空），不再使用的删掉。补完译文后运行 `python tools/i18n.py check`，它检查有没有漏译，以及 `%1` 等占位符是否一致；CI 也会跑这一步。
- 繁體中文可以先用 OpenCC（`s2twp`）转换再人工校对。注意 OpenCC 会把「类型」转成「型別」、「声明」转成「宣告」，这里要改回「類型」「聲明」。
- 新增语言：在 `LanguageController::codes()`、`choices()`、`resolve()` 和 `tools/i18n.py` 的 `LANGUAGES` 里加上代码，在 `CMakeLists.txt` 的 `qt_add_resources` 里加上文件。
- 只在 C++ 里构造一次的文字（全局常量、`static` 局部变量）不会跟着切换，要写成每次调用时生成的函数。

## 调试用环境变量

| 变量 | 作用 |
|---|---|
| `MIKU_LIBRARY_DATA_DIR` | 使用指定的数据目录，代替用户主目录下的 `.miku-library` |
| `MIKU_LIBRARY_MOTION_TRACE` | 设置后，在日志中记录换肤过渡的帧数和最长帧间隔 |

## 许可证

项目以 [GPL-3.0](../LICENSE) 发布，每个源文件开头带有 `SPDX-License-Identifier: GPL-3.0-only` 声明，新文件请保持一致。提交的代码同样按 GPL-3.0 授权；引入新的第三方库前，请确认其许可与 GPL-3.0 兼容（MIT、BSD、Apache-2.0、LGPL 等均可），并把许可文本放进 `licenses/`。

## 约定

- 界面文字的原文使用简体中文：QML 里写 `qsTr("…")`，C++ 里写 `QCoreApplication::translate("Miku", "…")`。和变量拼接的句子写成一整句加 `%1`，不要拆成几段相加，否则其他语言没法调整语序。拿来做比较或当作键的中文（标题解析的关键词、磁盘上的目录名）不要包。代码注释使用英文，只解释「为什么」。
- 新颜色先加到 `T.qml`，每个颜色同时给出深浅两个值。
- 基础控件基于 `QtQuick.Templates`，不依赖任何内置样式。模板控件要自己声明 `implicitHeight`，字体也不继承应用字体，文本框用 `font: Qt.application.font`。
- Windows 版不要引入 Qt Widgets：托盘、文件框和无边框窗口都直接走 Win32，发布包因此不带 `Qt6Widgets.dll`。只有 macOS / Linux 的 `*_qt.cpp` 使用 Widgets，CMake 也只在非 Windows 链接它。
- Windows 版固定使用 schannel TLS 后端（`main.cpp`）。测试网络时不要在 `QSslSocket::setActiveBackend` 之前让 Qt 用上别的后端：OpenSSL 后端经本机代理时，首次连接约一半会以 `Connection closed` 失败，发布版不包含它。
- 每次改动在 [更新记录](../UI_REWRITE.md) 中写明内容与验证方式。
