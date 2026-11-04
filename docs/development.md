# 开发

[← 返回 README](../README.md)

C++20 · Qt 6 Widgets · SQLite · libtorrent，仅支持 Windows x64。

## 构建

依赖：Visual Studio Build Tools（MSVC x64）、Qt 6.8.3 MSVC 2022 x64、CMake、Ninja、vcpkg。

```cmd
C:\vcpkg\vcpkg.exe install libtorrent:x64-windows
set QT_DIR=C:\Qt\6.8.3\msvc2022_64
set VCPKG_ROOT=C:\vcpkg
build.bat
```

`build.bat` 默认使用 `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`，其他安装位置请修改脚本开头的 `VS_DIR`。输出为 `dist\AnimeNexus\AnimeNexus.exe`，目录内已包含运行所需的 Qt、libtorrent、OpenSSL、Boost 和插件。

`python tools\package_portable.py` 生成便携 ZIP 到 `..\release_packages\`：先审计依赖，再校验解压后的文件哈希，最后在隔离的用户配置中启动一次确认可用。

## 测试

12 组冒烟测试随构建一起生成在 `build\` 下，直接运行即可；带 `--test` 的为界面测试，会短暂显示窗口。

| 测试 | 覆盖 |
|---|---|
| `LocalLibrarySmoke` | 下载记录持久化、自动目录、重复预留、字幕组与季度隔离、画质去重、合集重叠、追更目标与新集计算 |
| `DownloadSmoke` | 用本机 HTTP、Tracker 和自生成种子跑完整下载流程：完成即停、重启恢复、文件选择、补缺集 |
| `OptimizationSmoke` | 站点持久化、全来源调度、分页、请求取消、别名匹配、单实例参数转交、占位文件大小 |
| `RpcSmoke` | qBittorrent / Transmission / aria2 接口的认证、添加、进度、暂停、恢复、移除 |
| `ExternalDownloaderSmoke` | 外部程序启动参数、种子落盘、重复提交、文件校验与路由 |
| `DetailLibrarySmoke` | 字幕组排序与筛选、下载 / 播放切换、本地详情 |
| `SettingsLimitsSmoke` | 设置项、下载器草稿、应用栏显示 |
| `UiAuditSmoke --test` | 浮层、欢迎页、过渡动画、海报缓存 |
| `ControlsSmoke --test`、`WindowSmoke`、`DirectorySmoke`、`SearchInputSmoke` | 控件、窗口、下载目录与中文输入 |

测试只使用临时目录和自生成数据，不访问真实影片内容。

## 代码结构

```
src/
  core/   配置、站点目录、数据库、标题解析、字幕组归并、下载记录（LocalLibrary）
  net/    HTTP、Bangumi、各资源站适配与聚合搜索、内置下载引擎、外部下载器、追更检查
  ui/     主题与图标、过渡动画（Motion）、无边框窗口、通用控件与浮层
    pages/  本季放送、搜索、番剧详情、我的追番、下载任务、资源站点、偏好设置
```

几个关键位置：

- `core/LocalLibrary`：集数去重、下载规划、自动目录、下载偏好与追更目标。
- `net/AnimeDownloads`：任务预留、并行添加、旧任务关联、状态同步。
- `net/TorrentEngine`：种子校验与保存、文件选择、完成后暂停。
- `ui/Theme`：浅色 / 深色配色和全部样式表，颜色只在这里定义。
- `ui/Motion`：淡入淡出、滑动指示条、展开收起，以及按屏幕刷新率推进动画的时钟。

## 调试用环境变量

| 变量 | 作用 |
|---|---|
| `ANIME_NEXUS_DATA_DIR` | 使用指定的数据目录，代替 `%USERPROFILE%\.anime-nexus` |
| `ANIME_NEXUS_MOTION_TRACE` | 设置后，每次过渡动画结束时在日志中记录帧数和最长帧间隔 |

## 许可证

项目以 [GPL-3.0](../LICENSE) 发布。提交的代码同样按 GPL-3.0 授权；引入新的第三方库前，请确认其许可与 GPL-3.0 兼容（MIT、BSD、Apache-2.0、LGPL 等均可），并把许可文本放进 `licenses/`。

## 约定

- 界面文字使用中文，代码注释使用英文，只解释「为什么」。
- 新颜色先加到 `Theme` 的两套配色中，再在样式表里用 `$名称$` 引用。
- 每次改动在 [更新记录](../UI_REWRITE.md) 中写明内容与验证方式。
