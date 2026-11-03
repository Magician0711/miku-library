# Anime Nexus C++

Windows x64 番剧资源聚合搜索与下载客户端，使用 C++20、Qt 6 Widgets、SQLite 和内置 libtorrent。主题色为 `#39c5bb`，使用无边框窗口。

## 运行

可直接下载 GitHub Releases 中的 Windows x64 便携 ZIP，完整解压后运行 `AnimeNexus.exe`。源码仓库不包含编译输出和个人数据。

打开 `dist\AnimeNexus\AnimeNexus.exe`。发布目录包含运行所需的 Qt、libtorrent、OpenSSL、Boost 和插件，无需安装 qBittorrent。`Ctrl+K` 进入搜索。

## 功能

- Bangumi 本季放送表、中日英文搜索、作品详情与追番收藏。
- 界面支持浅色、深色与跟随系统三种配色，可在标题栏右上角或「偏好设置 → 外观」切换，立即生效并保存在 `config.json` 的 `appearance` 中。
- Nyaa、动漫花园、蜜柑计划、ACG.RIP、Tokyo Toshokan、Anime Garden、Bangumi.Moe、SubsPlease 等适配器并发检索；站点 JSON 支持导入、导出与热重载。
- 按字幕组归并资源，解析清晰度、编码、位深、HDR、杜比视界、音频和字幕标签。
- 支持季度、资源类型、画质、字幕、关键词和「全部 / 已下载 / 未下载」筛选。
- 单集下载自动创建 `默认下载目录/番剧名/字幕组/第N季/`，先保存种子，再启动内容下载。
- 字幕组右侧提供「一键下载」；已有下载记录或缓存时显示「更新」。按番剧 ID、字幕组、季度、集数检查重复，只选择一个画质版本，补齐当前检索结果中缺少的正片。
- 合集与已有话数重叠时，只下载可明确识别的缺集视频。无法确认集数的缓存或合集会阻止有重复风险的自动任务，并给出原因。
- 「我的追番」包含收藏和已有本地视频的作品，详情只显示下载完成的字幕组视频；按钮为「播放」，通过系统默认播放器打开。合集可以选择具体视频。
- 正常资源详情中，已下载资源同样可直接播放；下载中的资源会显示状态。
- 设置的「传输与队列」提供「下载完成后自动停止下载和上传」，默认开启。完成任务保留在列表内，暂停状态持久化，其他未完成任务继续运行。
- 下载任务支持暂停、恢复和移除。
- 默认全局上传上限为 1024 KiB/s（1 MiB/s），所有任务共享；下载速度、连接数、上传槽、同时下载/做种和活动任务数量均不限。设置里这些数量填 0 显示为「不限」，仍可手动设置上限。
- 批量种子请求异步进行，慢请求不会阻塞其他任务；下载状态合并刷新，减少多任务时的主线程和磁盘开销。

批量下载以站点实际返回且可识别的话数为准。站点没有返回的旧话或无法确认话数的资源不会被猜测为已下载。当前版本通过手动点击「更新」补缺集，不包含定时追番任务。

## 切换下载器

默认保留内置下载器。在侧栏底部点击箭头，或点击「下载任务」页上方的下载器按钮，可切换后续新任务使用的软件；原有任务继续由原下载器处理。

设置新增「下载器」页，可选择类型、自动查找常见安装位置，也可通过与应用统一的圆角文件选择窗口浏览 `.exe`。各类型分别保存程序路径，默认配置仍使用内置下载器。

| 类型 | 交接方式 |
|---|---|
| qBittorrent | 传入保存目录及磁力或本地种子，跳过添加窗口 |
| BitComet | 本地种子传入保存目录并静默添加；磁力使用 URL 打开方式 |
| Transmission | 使用种子/磁力打开入口，请在其任务窗口核对保存目录 |
| aria2 | 选择 `aria2c.exe`，传入保存目录；支持本地种子的缺集文件选择；后台运行 |
| 自定义 | 选择其他下载器程序，并按该软件的说明填写参数模板 |

自定义参数支持 `{input}`（磁力或本地种子）、`{saveDir}`（番剧目录）、`{name}`（资源标题）。默认 `"{input}"` 适用于接受种子文件路径或磁力参数的软件；包含空格的参数需加双引号。参数按独立值传给程序，不经过命令解释器。

外部下载器需要自行安装。未启用本机连接管理时，任务显示「正在发送 / 已发送 / 发送失败」，进度与暂停等操作在对应软件中管理；应用中的上传限速和完成后停止选项主要控制内置下载器，aria2 启动时可传入完成后停止做种选项。移除外部记录不会取消该软件中的任务。

应用继续预留同番剧、字幕组、季度和集数，避免重复提交；程序启动模式优先选择单集。需要按合集文件补缺集时，本地种子可用 aria2 指定文件；无法可靠选择文件的启动方式会提示改用单集或内置下载器，不会重复发送整包。

保存了种子、且对应内容已完整落盘时，会在后台校验 v1/hybrid 分片 hash；校验通过的视频自动加入本地番剧与播放列表。程序启动模式下，仅有磁力而没有本地种子、纯 v2 种子、缺少其他分片数据的选择性合集，暂不自动认定完成。预分配文件不会仅因大小正确就被认为已下载。

启动参数参考：[qBittorrent 手册](https://github.com/qbittorrent/qBittorrent/blob/master/doc/en/qbittorrent.1.md)、[BitComet 命令行说明](https://wiki.bitcomet.com/bitcomet-command-line/)、[aria2 手册](https://aria2.github.io/manual/en/html/aria2c.html)、[Transmission Qt 源码](https://github.com/transmission/transmission/tree/main/qt)。

## 本机连接管理

qBittorrent、Transmission、aria2 可在「设置 → 下载器」启用本机连接管理。先在对应软件启用 Web UI / RPC，填写 `localhost`、`127.0.0.1` 或 `[::1]` 地址和凭据。此模式不需要选择程序路径，但下载器服务需已启动。

连接后，本应用同步进度并可暂停、恢复、移除任务；移除保留影片文件。下载器报告校验完成、且目标目录中的视频实际存在时加入本地库存。下载完成自动停止由本应用运行期间的轮询执行；aria2 同时接收停止做种参数。传输速率应用于新任务，外部下载器自己的全局限制仍生效。凭据保存在当前用户配置中，不随发布包分发。

本地种子支持向这三个客户端传递缺集文件选择；只有磁力且尚无文件元数据的缺集合集继续使用内置下载器。BitComet 和自定义下载器保持程序启动方式。接口按 qBittorrent Web API v2（4.x/5.x）、Transmission 兼容 RPC、aria2 JSON-RPC 实现，已用本机 HTTP 模拟服务验证，仍需按实际安装版本开启并配置接口。

协议参考：[qBittorrent API](https://github.com/qbittorrent/qBittorrent/wiki/WebUI-API-%28qBittorrent-5.0%29)、[Transmission RPC](https://github.com/transmission/transmission/blob/4.0.6/docs/rpc-spec.md)、[aria2 RPC](https://aria2.github.io/manual/en/html/aria2c.html#rpc-interface)。

## 搜索与任务恢复

- 启用状态和导入目录保存在用户数据下的 `anime_sites.json`，重启仍有效；所有启用且具备适配器的来源进入检索队列。
- 默认目录有 90 个条目，其中 8 个来源具备内置搜索适配器。其余为网页目录、字幕组页面或需认证的站点，不会假装已参与搜索。自定义条目提供有效 `rss_url` 即可加入，关键词位置用 `{q}`。
- Anime Garden 自动翻页；带 `rel="next"` 的 RSS 跟随同站下一页。没有分页接口的 RSS 仍受站点返回数量限制。分页失败或达到保护上限会报告结果可能不完整。
- 取消或替换检索会释放旧网络请求；短英文别名按词边界匹配，减少误混入其他作品。
- 追番详情右上角「查找可下载资源」进入同一作品的搜索详情，重置筛选、折叠及滚动状态；返回后位于番剧搜索页。
- 内置任务默认 90 秒未收到视频数据时尝试未用过的其他来源，可在「传输与队列」调整或关闭。收到数据后保留当前来源，防止重复或破坏已有部分文件；不会承诺不同网络环境下绝对最快。
- 同一个数据目录只允许一个应用实例写入；再次打开会激活已有窗口并转交启动参数。
- `--add-magnet` / `--add-torrent` 统一走番剧下载流程。可搭配 `--subject <Bangumi ID>`；无法唯一匹配缓存作品时先选择所属番剧，再按当前下载器及目录导入。

## 下载一直 0% 的修复

自动网络模式改为让系统为各个连接选择路由，同时监听 IPv4 / IPv6；从手动网卡切回自动会清除旧绑定。无需修改 Windows 或代理软件设置。

优先使用种子自身的 Tracker，也会合并同一哈希的其他来源磁力链接所带地址。对没有任何 Tracker 的公开种子，默认补充少量 HTTPS / UDP 公共 Tracker，可在「设置 → 网络」关闭。私有种子保留原地址；新增地址不写回原始种子文件，单独保存在任务状态中供重启恢复。不同 Tracker 层级并行发现节点，避免首个地址失效拖住其他层级。

公共地址依据 [OpenTrackr 的使用说明](https://opentrackr.org/) 和 [trackerslist 维护的公开列表](https://github.com/ngosang/trackerslist) 选择，实际可用性取决于当前网络和 Tracker 服务。

下载状态区分节点发现、已连接但等待分片和 Tracker 失败；点击恢复会重新请求节点。选择的种子文件与资源哈希不一致时会阻止添加，防止错误来源。任务状态改为原子保存，保留补充地址及暂停状态。

上述软件问题已用自生成数据在本机重现并验证修复。具体番剧能否持续传输仍需其发布群中存在可用节点。已移除的任务保持已移除，重新选择资源下载即可使用新逻辑。

## 本地数据

用户数据位于 `%USERPROFILE%\.anime-nexus\`：

| 文件 | 内容 |
|---|---|
| `config.json` | 默认目录、引擎及外部下载器连接设置 |
| `anime_sites.json` | 用户站点目录和启用状态 |
| `local-library.json` | 番剧、字幕组、季度、集数、任务 hash、文件路径和完成状态 |
| `anime_nexus.db` | 收藏与 Bangumi 信息缓存 |
| `engine/state.json` | 种子地址、保存目录、文件选择与暂停状态 |

本地目录采用原子写入。只有引擎确认下载完成、且实际文件存在并符合记录大小的视频才会显示为可播放。删除或移动视频后，相关视频不再显示。已有旧引擎任务在名称和季度能唯一匹配缓存番剧时自动关联；存在歧义时等待资源检索匹配到相同 hash 后关联，不会搬动旧文件。请保留下载目录中的 `.torrent` 文件，以便重启恢复。

首次启动会在系统下载文件夹下创建 `AnimeNexus`。每次启动检查已保存的默认目录，目录消失时提示重新选择。运行中新增下载会在该根目录下自动创建番剧目录。

## 构建

依赖：Visual Studio Build Tools（MSVC x64）、Qt 6.8.3 MSVC 2022 x64、CMake、Ninja、vcpkg。

```cmd
C:\vcpkg\vcpkg.exe install libtorrent:x64-windows
set QT_DIR=C:\Qt\6.8.3\msvc2022_64
set VCPKG_ROOT=C:\vcpkg
build.bat
```

`build.bat` 的 VS 默认路径为 `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`，其他安装位置需要调整脚本。输出为 `dist\AnimeNexus\AnimeNexus.exe`。

## 验证

- `OptimizationSmoke`：站点持久化、全来源调度、分页、请求取消、别名边界与单实例参数转交。
- `RpcSmoke`：三个外部接口的认证、添加、进度、暂停、恢复、完成即停与保留文件的移除。

- `ExternalDownloaderSmoke`：外部启动参数、真实进程传参、种子落盘、重复提交、文件校验与内外部路由。
- `SettingsLimitsSmoke`：不限数值、下载器类型、程序路径草稿与自定义模板切换。
- `LocalLibrarySmoke`：库存持久化、自动目录、重复预留、组别与季度隔离、画质去重、合集重叠、真实文件检查。
- `DownloadSmoke`：临时目录中的自生成数据，通过本机 HTTP、Tracker 和种子客户端验证下载流程、完成即停、重启、文件选择和字幕组补缺集，不访问影片站点中的内容。
- `DetailLibrarySmoke`：状态筛选、组别操作、下载/播放切换、追番本地详情、文件删除后的展示。
- `WindowSmoke`、`ControlsSmoke --test`、`UiAuditSmoke --test`、`DirectorySmoke`、`SearchInputSmoke`：窗口、控件、浮层、下载目录与中文输入回归检查。

## 代码结构

- `src/core/LocalLibrary.*`：本地目录、集数去重、下载规划、自动目录。
- `src/net/AnimeDownloads.*`：任务预留、并行添加、旧任务关联、合并状态同步。
- `src/net/TorrentEngine.*`：种子校验与保存、进程内下载、文件选择、完成后暂停。
- `src/ui/pages/AnimeDetailPage.*`：字幕组资源、本地视频与筛选。
- `src/ui/pages/LibraryPage.*`：收藏与本地番剧入口。

## 依赖许可

Qt 6.8.3（LGPLv3/GPLv3）、libtorrent 2.1.2（BSD-3-Clause）、OpenSSL 3（Apache-2.0）、Boost（Boost Software License）。
