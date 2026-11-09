# 开发

[← 返回 README](../README.md)

C++20 · Qt Quick（QML）· SQLite · libtorrent，仅支持 Windows x64。

## 构建

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

## 测试

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
- `qml/Shell.qml`：页面共用的边距和工具栏高度，随侧栏宽度变化。
- `libmpv-2.dll` 超过 GitHub 的单文件大小上限，没有放进仓库。克隆后从 [shinchiro/mpv-winbuild-cmake](https://github.com/shinchiro/mpv-winbuild-cmake) 的发布页下载 `mpv-dev-x86_64` 压缩包，把其中的 `libmpv-2.dll` 放到 `third_party/mpv/`；缺少时程序仍能构建，只是内置播放器不可用。
- 内置播放器：`third_party/mpv/libmpv-2.dll` 运行时加载（`src/app/PlayerController.cpp`），由 CMake 复制到构建目录，发布时放在程序旁边，Anime4K 着色器放在 `shaders/anime4k/`。画面由 `src/app/MpvVideoItem.cpp` 通过 mpv 的 D3D11 渲染接口画到纹理上。设置环境变量 `MIKU_LIBRARY_MPV_LOG` 为文件路径可输出 mpv 日志。
- `qml/Scaffold.qml`：页面骨架：标题、工具栏和固定行（`fixed`）。内容会滚到标题下方的页面把滚动距离传给 `scrolled`，标题和工具栏随内容一起移出窗口；带固定行的列表页自己裁剪。
- `qml/Glass.qml`、`qml/Backdrop.qml`：只有侧栏用玻璃材质；内容层不透明，海报模糊后从侧栏下方透出。
- `qml/SmoothWheel.qml`：所有列表的滚轮滚动。它必须铺满列表（`parent: view`）：指针处理器只接收自身范围内的事件，留在 Flickable 的内容层里就是 0×0，收不到滚轮。`pinned` 记录视图是否停在顶部，表头变高时视图据此留在顶部。
- 窗口圆角：窗口带透明通道（`setDefaultAlphaBuffer`），底色是一个圆角矩形，半径 `Shell.corner`（侧栏半径 16 + 边距 8，两条弧线平行）。会碰到窗口四角的东西都要自己带圆角：`Backdrop`、窗口按钮的底色、弹窗遮罩、关闭按钮。不用整窗遮罩，那样每帧多一遍全屏渲染。
- 侧栏动画：所有子项都跟随 `sidebarReveal`（0 到 1）这一个值。悬停展开和手动展开走同一条路径，内容都会被推开。页面逐帧跟随侧栏边缘（`Shell.leading`）调整大小。海报的解码尺寸按 40 像素分档，窗口停止变化后才更新（`PosterGrid.decodeWidth`），列数不随侧栏变化，所以调整时既不重新加载也不换行；会切换版式的判断用 `Shell.restingLeading`。
- `qml/ThemeFade.qml`：换肤过渡。先截下整个窗口（包含这一层本身，所以连续切换能接上当前颜色），画面到屏幕后才切换配色，再由 GPU 淡出。

改了 `qml/Shell.qml` 或 `qml/T.qml` 里属性的类型后，要让所有 QML 重新编译（比如 `touch qml/*.qml`）。qmlcachegen 把这些类型写进每个页面的编译结果，而 Ninja 不知道这层依赖：没改过的页面会按旧类型读值，不报错，只是读到错的数字。

## 调试用环境变量

| 变量 | 作用 |
|---|---|
| `MIKU_LIBRARY_DATA_DIR` | 使用指定的数据目录，代替 `%USERPROFILE%\.miku-library` |
| `MIKU_LIBRARY_MOTION_TRACE` | 设置后，在日志中记录换肤过渡的帧数和最长帧间隔 |

## 许可证

项目以 [GPL-3.0](../LICENSE) 发布，每个源文件开头带有 `SPDX-License-Identifier: GPL-3.0-only` 声明，新文件请保持一致。提交的代码同样按 GPL-3.0 授权；引入新的第三方库前，请确认其许可与 GPL-3.0 兼容（MIT、BSD、Apache-2.0、LGPL 等均可），并把许可文本放进 `licenses/`。

## 约定

- 界面文字使用中文，代码注释使用英文，只解释「为什么」。
- 新颜色先加到 `T.qml`，每个颜色同时给出深浅两个值。
- 基础控件基于 `QtQuick.Templates`，不依赖任何内置样式。模板控件要自己声明 `implicitHeight`，字体也不继承应用字体，文本框用 `font: Qt.application.font`。
- 不要引入 Qt Widgets：托盘、文件框和无边框窗口都直接走 Win32，发布包因此不带 `Qt6Widgets.dll`。
- 程序固定使用 Windows 的 schannel TLS 后端（`main.cpp`）。测试网络时不要在 `QSslSocket::setActiveBackend` 之前让 Qt 用上别的后端：OpenSSL 后端经本机代理时，首次连接约一半会以 `Connection closed` 失败，发布版不包含它。
- 每次改动在 [更新记录](../UI_REWRITE.md) 中写明内容与验证方式。
