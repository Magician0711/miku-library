# 外部下载器

[← 返回 README](../README.md)

默认使用内置下载器（libtorrent），无需安装其他软件。如果你习惯用别的下载器，可以让 Anime Nexus 只负责找资源和整理目录，把下载交给它。

## 切换

点击侧栏底部的下载器卡片，或「下载任务」页上方的下载器按钮，即可切换**之后新建**的任务使用的软件；已有任务继续由原下载器处理。程序路径在「偏好设置 → 下载器」设置，可自动查找常见安装位置。

| 类型 | 交接方式 |
|---|---|
| qBittorrent | 传入保存目录及磁力或种子，跳过添加窗口 |
| BitComet | 种子静默添加到保存目录；磁力通过打开链接的方式添加 |
| Transmission | 通过打开入口添加，请在其窗口中核对保存目录 |
| aria2 | 选择 `aria2c.exe`，传入保存目录，支持按文件补缺集，后台运行 |
| 自定义 | 选择其他下载器程序，按其说明填写参数模板 |

自定义参数模板支持 `{input}`（磁力或种子路径）、`{saveDir}`（番剧目录）、`{name}`（资源标题）。默认 `"{input}"`；含空格的参数要加双引号。参数直接传给程序，不经过命令行解释器。

以程序方式交接时，任务显示「正在发送 / 已发送 / 发送失败」，进度与暂停在对应软件里管理；移除记录不会取消该软件中的任务。Anime Nexus 仍会按番剧、字幕组、季度和集数预留，避免重复提交。

## 本机连接管理

qBittorrent、Transmission、aria2 还可以在「偏好设置 → 下载器」中启用本机连接管理，由 Anime Nexus 直接控制：

1. 在下载器中开启 Web UI / RPC。
2. 在 Anime Nexus 中填写本机地址（`localhost`、`127.0.0.1` 或 `[::1]`）和凭据。

连接后可以在应用内看到进度，并暂停、继续、移除任务（移除保留文件）；下载完成的视频会自动加入本地番剧。下载完成后自动停止由本应用在运行期间执行。凭据保存在当前用户的配置中，不随发布包分发。

接口按 qBittorrent Web API v2（4.x / 5.x）、Transmission RPC、aria2 JSON-RPC 实现，已用本机模拟服务测试；请按你实际安装的版本开启接口。

参考：[qBittorrent API](https://github.com/qbittorrent/qBittorrent/wiki/WebUI-API-%28qBittorrent-5.0%29) · [Transmission RPC](https://github.com/transmission/transmission/blob/4.0.6/docs/rpc-spec.md) · [aria2 RPC](https://aria2.github.io/manual/en/html/aria2c.html#rpc-interface) · [qBittorrent 命令行](https://github.com/qbittorrent/qBittorrent/blob/master/doc/en/qbittorrent.1.md) · [BitComet 命令行](https://wiki.bitcomet.com/bitcomet-command-line/)

## 限制

- 应用中的上传限速和「完成后停止」主要作用于内置下载器；aria2 启动时会带上完成后停止做种的参数。
- 只有磁力、尚无文件信息的缺集合集，仍由内置下载器处理。
- 程序启动方式下，只有磁力而没有本地种子、纯 v2 种子或部分选择的合集，暂不能自动确认下载完成。
