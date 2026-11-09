// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Scaffold {
    id: page
    readonly property var s: App.settings
    readonly property var f: s.form
    readonly property var intros: [
        "外观、窗口行为与追更提醒，修改后立即生效。",
        "下载位置、下载器与速度。内置下载器默认上传上限 1 MiB/s，填 0 表示不限。",
        "搜索用的资源站点和代理，以及内置下载器的连接方式。",
        "海报缓存，以及设置和记录的备份。",
        "番剧资源检索与下载管理工具。"
    ]
    readonly property int controlWidth: 200
    readonly property bool shown: enabled
    onShownChanged: if (shown) { s.load(); s.updateCacheSize() }
    Component.onCompleted: s.updateCacheSize()

    title: "偏好设置"
    subtitle: overview ? "全部设置一览。" : intros[s.section]
    subtitleTip: s.status
    fixedHeight: overview ? 0 : 46
    readonly property real contentWidth: width - leading - trailing
    // A wide window has room for every section at once.
    readonly property bool overview: restingWidth >= 1500
    readonly property var sections: [
        { text: "通用", value: 0, icon: "settings" }, { text: "下载", value: 1, icon: "download" },
        { text: "资源与网络", value: 2, icon: "sites" }, { text: "数据", value: 3, icon: "folder" }, { text: "关于", value: 4, icon: "info" }
    ]
    readonly property int shownSection: s.section
    onShownSectionChanged: Qt.callLater(showSection)
    function showSection() {
        const top = overview ? holder.children[shownSection].heading.y : 0
        flick.contentY = Math.max(0, Math.min(top, flick.contentHeight - flick.height))
    }

    fixed: Segmented {
        x: page.leading + (page.contentWidth - width) / 2
        y: Shell.band + 2
        visible: !page.overview
        items: page.sections
        value: page.s.section
        onActivated: value => page.s.section = value
    }

    // One section: its cards, and in the overview a heading above them.
    component SectionView: Item {
        id: group
        required property int section
        readonly property bool active: page.overview || page.s.section === section
        readonly property Item heading: head
        function arrange() { holder.arrange() }
        onActiveChanged: arrange()
        opacity: active ? 1 : 0
        visible: active || opacity > 0
        enabled: active
        Behavior on opacity { NumberAnimation { duration: T.fast; easing.type: Easing.OutCubic } }
        Column {
            id: head
            visible: page.overview
            spacing: 2
            bottomPadding: 2
            onHeightChanged: group.arrange()
            Text { text: page.sections[group.section].text; color: T.text; font.pixelSize: T.title2; font.weight: Font.Bold }
            BodyText { width: parent.width; text: page.intros[group.section]; color: T.text3; font.pixelSize: T.callout }
        }
    }

    Flickable {
        id: flick
        x: page.leading
        y: page.contentTop + 4
        width: page.contentWidth
        height: page.height - y - (saveBar.height > 0 ? saveBar.height + 12 : 0)
        contentHeight: holder.height + 24
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        Tpl.ScrollBar.vertical: NxScrollBar {}
        SmoothWheel { view: flick }
        Item {
            id: holder
            width: flick.width
            onWidthChanged: arrange()
            Component.onCompleted: arrange()
            function arrange() { Qt.callLater(place) }
            // Cards flow into as many columns as the window has room for. The overview
            // keeps each section together and flows the sections instead.
            function place() {
                const gap = 14
                const columns = width >= 1500 ? 3 : width >= 820 ? 2 : 1
                const cardWidth = (width - gap * (columns - 1)) / columns
                const heights = new Array(columns).fill(0)
                const shortest = () => {
                    let column = 0
                    for (let i = 1; i < columns; ++i)
                        if (heights[i] < heights[column] - 1) column = i
                    return column
                }
                const put = (item, column) => {
                    item.width = cardWidth
                    item.x = column * (cardWidth + gap)
                    item.y = heights[column]
                    heights[column] += item.height + gap
                }
                for (const group of children) {
                    if (!group.active)
                        continue
                    const home = shortest()
                    if (page.overview)
                        put(group.heading, home)
                    for (const card of group.children)
                        if (card !== group.heading && card.visible)
                            put(card, page.overview ? home : shortest())
                    if (page.overview)
                        heights[home] += 16
                }
                height = Math.max(...heights)
            }

            // General
            SectionView {
                section: 0
                SettingCard {
                    title: "界面配色"
                    SettingRow {
                        title: "外观"
                        hint: "「跟随系统」会随 Windows 的深色 / 浅色设置自动切换。"
                        divided: false
                        Segmented {
                            compact: true
                            items: [{ text: "跟随系统", value: "system" }, { text: "浅色", value: "light" }, { text: "深色", value: "dark" }]
                            value: Theme.mode
                            onActivated: value => page.s.setAppearance(value)
                        }
                    }
                }
                SettingCard {
                    title: "窗口"
                    SettingRow {
                        title: "关闭窗口时"
                        hint: "最小化到托盘后，下载和追更检查会在后台继续。"
                        divided: false
                        NxSelect {
                            implicitWidth: page.controlWidth
                            model: [{ text: "每次询问", value: "ask" }, { text: "最小化到托盘", value: "tray" }, { text: "退出软件", value: "quit" }]
                            value: page.f.closeAction
                            onChosen: value => page.s.set("closeAction", value)
                        }
                    }
                }
                SettingCard {
                    title: "追更与通知"
                    SettingRow {
                        title: "检查频率"
                        hint: "程序运行期间按此间隔再次检查。每部番剧可在详情页选择提醒或自动下载。"
                        divided: false
                        NxSelect {
                            implicitWidth: page.controlWidth
                            model: [{ text: "仅在启动时", value: 0 }, { text: "每 3 小时", value: 3 }, { text: "每 6 小时", value: 6 }, { text: "每 12 小时", value: 12 }]
                            value: page.f.updateInterval
                            onChosen: value => page.s.set("updateInterval", value)
                        }
                    }
                    SettingRow {
                        title: "下载完成时通知"
                        hint: "在屏幕右下角显示系统通知，点击打开「我的追番」。"
                        NxSwitch { checked: page.f.notifyDownloads === true; onToggled: page.s.set("notifyDownloads", checked) }
                    }
                }
            }

            // Downloads
            SectionView {
                section: 1
                SettingCard {
                    title: "下载位置"
                    SettingRow {
                        title: "默认下载目录"
                        hint: "只影响之后新建的任务。下载按「番剧名 / 字幕组 / 第 N 季」自动整理。"
                        divided: false
                        wide: true
                        Row {
                            width: parent.width
                            spacing: 8
                            NxField {
                                width: parent.width - browseSave.width - 8
                                text: page.f.savePath || ""
                                onTextEdited: page.s.set("savePath", text)
                            }
                            NxButton { id: browseSave; text: "浏览…"; onClicked: page.s.browseSavePath() }
                        }
                    }
                }
                SettingCard {
                    title: "下载方式"
                    SettingRow {
                        title: "使用的下载器"
                        hint: "只影响新任务，已添加的任务继续由原下载器处理。"
                        divided: false
                        NxSelect {
                            implicitWidth: page.controlWidth
                            model: page.s.downloaderTypes
                            value: page.f.downloaderType
                            onChosen: value => page.s.set("downloaderType", value)
                        }
                    }
                    Text { width: parent.width; text: page.f.downloaderHelp || ""; color: T.text3; font.pixelSize: T.callout; wrapMode: Text.Wrap }
                    Text {
                        width: parent.width
                        visible: page.f.externalVisible === true
                        text: page.f.rpcEnabled === true ? "连接后同步任务与进度。自动停止仅在本应用运行时执行；限速应用于新建任务，下载器的全局上限仍然有效。"
                                                         : "程序启动模式：进度和完成后停止由对应软件管理。"
                        color: T.text3
                        font.pixelSize: T.callout
                        wrapMode: Text.Wrap
                    }
                }
                SettingCard {
                    title: "程序位置"
                    visible: page.f.externalVisible === true && page.f.rpcEnabled !== true
                    Row {
                        width: parent.width
                        spacing: 8
                        NxField {
                            width: parent.width - detectButton.width - browseButton.width - 16
                            placeholderText: "选择已安装下载器的 .exe 文件"
                            text: page.f.downloaderPath || ""
                            onTextEdited: page.s.set("downloaderPath", text)
                        }
                        NxButton { id: detectButton; text: "自动查找"; onClicked: page.s.detectDownloader() }
                        NxButton { id: browseButton; text: "浏览…"; onClicked: page.s.browseDownloader() }
                    }
                }
                SettingCard {
                    title: "启动参数"
                    visible: page.f.customVisible === true
                    NxField {
                        width: parent.width
                        text: page.f.customArguments || ""
                        onTextEdited: page.s.set("customArguments", text)
                    }
                    Text {
                        width: parent.width
                        text: "{input} 资源链接或种子  ·  {saveDir} 保存目录  ·  {name} 资源标题\n包含空格的参数请使用双引号，例如 \"{input}\"。"
                        color: T.text3
                        font.pixelSize: T.callout
                        wrapMode: Text.Wrap
                    }
                }
                SettingCard {
                    title: "本机连接管理"
                    visible: page.f.rpcVisible === true
                    SettingRow {
                        title: "同步进度并在本应用中管理任务"
                        hint: "需先在下载器中启用 Web UI 或 RPC，仅连接本机地址。"
                        divided: false
                        NxSwitch { checked: page.f.rpcEnabled === true; onToggled: page.s.set("rpcEnabled", checked) }
                    }
                    SettingRow {
                        title: "连接地址"
                        NxField {
                            implicitWidth: page.controlWidth
                            enabled: page.f.rpcEnabled === true
                            placeholderText: "例如 http://127.0.0.1:8080"
                            text: page.f.rpcUrl || ""
                            onTextEdited: page.s.set("rpcUrl", text)
                        }
                    }
                    SettingRow {
                        title: "用户名"
                        NxField {
                            implicitWidth: page.controlWidth
                            enabled: page.f.rpcEnabled === true
                            text: page.f.rpcUser || ""
                            onTextEdited: page.s.set("rpcUser", text)
                        }
                    }
                    SettingRow {
                        title: "密码 / aria2 密钥"
                        hint: "凭据保存在当前用户的配置文件中。"
                        NxField {
                            implicitWidth: page.controlWidth
                            enabled: page.f.rpcEnabled === true
                            echoMode: TextInput.Password
                            text: page.f.rpcPassword || ""
                            onTextEdited: page.s.set("rpcPassword", text)
                        }
                    }
                }
                SettingCard {
                    title: "任务"
                    SettingRow {
                        title: "下载完成后自动停止"
                        hint: "默认关闭：下载完成后继续做种，让其他人也能下载。开启后停止下载和上传，任务保留在列表中。"
                        divided: false
                        NxSwitch {
                            // The form decides: turning this on needs a second confirmation.
                            checked: page.f.stopOnCompletion === true
                            onToggled: {
                                const wanted = checked
                                checked = Qt.binding(() => page.f.stopOnCompletion === true)
                                page.s.requestStopOnCompletion(wanted)
                            }
                        }
                    }
                    SettingRow {
                        title: "无数据时切换来源"
                        hint: "尚未收到视频数据时尝试其他来源；已开始传输的任务保留原来源。"
                        NxSpin {
                            implicitWidth: page.controlWidth
                            from: 0; to: 3600
                            suffix: " 秒"; zeroText: "关闭"
                            value: page.f.sourceStall || 0
                            onEdited: value => page.s.set("sourceStall", value)
                        }
                    }
                    SettingRow {
                        title: "移除下载任务时"
                        hint: "「同时删除文件」会删除视频、种子文件和留空的文件夹，无法撤销。"
                        NxSelect {
                            implicitWidth: page.controlWidth
                            model: [{ text: "每次询问", value: "ask" }, { text: "仅移除任务", value: "keep" }, { text: "同时删除文件", value: "delete" }]
                            value: page.f.removeAction
                            onChosen: value => page.s.set("removeAction", value)
                        }
                    }
                }
                SettingCard {
                    title: "速度与连接"
                    SettingRow {
                        title: "全局下载限速"; divided: false
                        NxSpin { implicitWidth: page.controlWidth; from: 0; to: 1000000; suffix: " KiB/s"; zeroText: "不限"; value: page.f.downloadLimit || 0; onEdited: value => page.s.set("downloadLimit", value) }
                    }
                    SettingRow {
                        title: "全局上传限速"; hint: "所有任务共享；1024 KiB/s = 1 MiB/s。"
                        NxSpin { implicitWidth: page.controlWidth; from: 0; to: 1000000; suffix: " KiB/s"; zeroText: "不限"; value: page.f.uploadLimit || 0; onEdited: value => page.s.set("uploadLimit", value) }
                    }
                    SettingRow {
                        title: "最大连接数"
                        NxSpin { implicitWidth: page.controlWidth; from: 0; to: 100000; zeroText: "不限"; value: page.f.maxConnections || 0; onEdited: value => page.s.set("maxConnections", value) }
                    }
                    SettingRow {
                        title: "最大上传槽"
                        NxSpin { implicitWidth: page.controlWidth; from: 0; to: 10000; zeroText: "不限"; value: page.f.maxUploads || 0; onEdited: value => page.s.set("maxUploads", value) }
                    }
                }
                SettingCard {
                    title: "队列"
                    SettingRow {
                        title: "启用任务队列"; hint: "超过下列数量的任务会排队等待。"; divided: false
                        NxSwitch { checked: page.f.queueing === true; onToggled: page.s.set("queueing", checked) }
                    }
                    SettingRow {
                        title: "同时下载"
                        NxSpin { implicitWidth: page.controlWidth; from: 0; to: 100000; zeroText: "不限"; value: page.f.activeDownloads || 0; onEdited: value => page.s.set("activeDownloads", value) }
                    }
                    SettingRow {
                        title: "同时做种"
                        NxSpin { implicitWidth: page.controlWidth; from: 0; to: 100000; zeroText: "不限"; value: page.f.activeSeeds || 0; onEdited: value => page.s.set("activeSeeds", value) }
                    }
                    SettingRow {
                        title: "活动任务上限"
                        NxSpin { implicitWidth: page.controlWidth; from: 0; to: 100000; zeroText: "不限"; value: page.f.activeLimit || 0; onEdited: value => page.s.set("activeLimit", value) }
                    }
                }
            }

            // Sources and network
            SectionView {
                section: 2
                SettingCard {
                    title: "资源站点"
                    SettingRow {
                        title: "搜索来源"
                        hint: App.sites.status
                        divided: false
                        NxButton { text: "管理站点…"; onClicked: App.selectPage(4) }
                    }
                }
                SettingCard {
                    title: "代理"
                    SettingRow {
                        title: "检索与海报"
                        hint: "用于访问资源站、Bangumi 和海报；本机下载器连接始终直连。内置下载器的节点传输不经过代理。"
                        divided: false
                        NxSelect {
                            implicitWidth: page.controlWidth
                            model: [{ text: "跟随系统代理", value: "system" }, { text: "不使用代理", value: "none" }, { text: "HTTP 代理", value: "http" }, { text: "SOCKS5 代理", value: "socks5" }]
                            value: page.f.proxyMode
                            onChosen: value => page.s.set("proxyMode", value)
                        }
                    }
                    SettingRow {
                        title: "地址与端口"
                        hint: "例如本机代理软件的 127.0.0.1 : 7890。"
                        Row {
                            spacing: 8
                            enabled: page.f.proxyMode === "http" || page.f.proxyMode === "socks5"
                            NxField {
                                implicitWidth: 130
                                placeholderText: "127.0.0.1"
                                text: page.f.proxyHost || ""
                                onTextEdited: page.s.set("proxyHost", text)
                            }
                            NxSpin {
                                implicitWidth: 126
                                from: 1; to: 65535
                                value: page.f.proxyPort || 7890
                                onEdited: value => page.s.set("proxyPort", value)
                            }
                        }
                    }
                }
                SettingCard {
                    title: "下载连接"
                    SettingRow {
                        title: "网络接口"
                        hint: "自动模式由系统为每个连接选择路由。"
                        divided: false
                        NxSelect {
                            implicitWidth: page.controlWidth
                            model: page.s.interfaces
                            value: page.f.bindAddress
                            onChosen: value => page.s.set("bindAddress", value)
                        }
                    }
                    SettingRow {
                        title: "监听端口"
                        hint: "0 表示每次启动随机选择。端口被系统占用或保留时，自动改用其他端口。"
                        NxSpin {
                            implicitWidth: page.controlWidth
                            from: 0; to: 65535
                            zeroText: "随机"
                            value: page.f.listenPort || 0
                            onEdited: value => page.s.set("listenPort", value)
                        }
                    }
                    SettingRow {
                        title: "协议加密"
                        NxSelect {
                            implicitWidth: page.controlWidth
                            model: [{ text: "首选", value: 0 }, { text: "强制", value: 1 }, { text: "禁用", value: 2 }]
                            value: page.f.encryption
                            onChosen: value => page.s.set("encryption", value)
                        }
                    }
                }
                SettingCard {
                    title: "节点发现"
                    SettingRow {
                        title: "DHT 网络"; hint: "不依赖 Tracker 查找节点。"; divided: false
                        NxSwitch { checked: page.f.dht === true; onToggled: page.s.set("dht", checked) }
                    }
                    SettingRow {
                        title: "本地节点发现（LSD）"; hint: "在局域网内寻找同一资源的节点。"
                        NxSwitch { checked: page.f.lsd === true; onToggled: page.s.set("lsd", checked) }
                    }
                    SettingRow {
                        title: "UPnP / NAT-PMP 端口映射"; hint: "自动在路由器上开放监听端口。"
                        NxSwitch { checked: page.f.upnp === true; onToggled: page.s.set("upnp", checked) }
                    }
                    SettingRow {
                        title: "补充公共 Tracker"; hint: "仅为缺少 Tracker 的公开种子补充，私有种子保留原有地址。"
                        NxSwitch { checked: page.f.publicTrackerFallback === true; onToggled: page.s.set("publicTrackerFallback", checked) }
                    }
                }
            }

            // Data
            SectionView {
                section: 3
                SettingCard {
                    title: "缓存"
                    SettingRow {
                        title: "海报缓存"
                        hint: "清理后，海报会在下次显示时重新下载。"
                        divided: false
                        Row {
                            spacing: 10
                            Text { anchors.verticalCenter: parent.verticalCenter; text: page.s.cacheSize; color: T.text3; font.pixelSize: T.callout }
                            NxButton { text: "清理"; onClicked: page.s.clearCovers() }
                        }
                    }
                }
                SettingCard {
                    title: "备份与恢复"
                    SettingRow {
                        title: "设置与记录"
                        hint: "一个文件包含设置、资源站点、下载记录和追番列表，用于换电脑或重装后恢复。不包含视频文件。"
                        divided: false
                        Row {
                            spacing: 8
                            NxButton { text: "导出…"; onClicked: page.s.exportBackup() }
                            NxButton { text: "导入…"; onClicked: page.s.importBackup() }
                        }
                    }
                }
            }

            // About
            SectionView {
                section: 4
                SettingCard {
                    title: "Miku Library " + page.s.version
                    Text {
                        width: parent.width
                        text: "聚合多个公开资源站的检索结果，按字幕组整理番剧资源，管理下载并播放本地视频。本软件以 GNU GPL v3.0 许可发布（程序目录的 LICENSE 文件），第三方组件的许可文本位于 licenses 文件夹。"
                        color: T.text3
                        font.pixelSize: T.callout
                        wrapMode: Text.Wrap
                    }
                    Divider { width: parent.width }
                    Text { text: "免费开源"; color: T.text; font.pixelSize: T.body }
                    BodyText { width: parent.width; text: page.s.freeNote; color: T.text3; font.pixelSize: T.callout }
                    Divider { width: parent.width }
                    Text { text: "关于播放"; color: T.text; font.pixelSize: T.body }
                    BodyText { width: parent.width; text: page.s.playerNote; color: T.text3; font.pixelSize: T.callout }
                }
                SettingCard {
                    title: "检查新版本"
                    SettingRow {
                        title: "启动时检查"
                        hint: "只读取 GitHub 上最新发布的版本号，不会自动下载或安装。"
                        divided: false
                        NxSwitch { checked: page.f.checkAppUpdates === true; onToggled: page.s.set("checkAppUpdates", checked) }
                    }
                    SettingRow {
                        title: "手动检查"
                        hint: page.s.updateStatus
                        NxButton { text: "立即检查"; busy: page.s.checkingUpdate; enabled: !page.s.checkingUpdate; onClicked: page.s.checkAppUpdate() }
                    }
                }
                SettingCard {
                    title: "使用声明"
                    Repeater {
                        model: page.s.disclaimer
                        BodyText {
                            required property string modelData
                            required property int index
                            width: parent.width
                            text: (index + 1) + ". " + modelData
                            color: T.text2
                            font.pixelSize: T.callout
                        }
                    }
                }
            }
        }
    }

    // Shown only while the form differs from the saved settings.
    Rectangle {
        id: saveBar
        x: page.leading
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        width: page.contentWidth
        height: page.s.dirty ? 52 : 0
        clip: true
        radius: 26
        color: T.surface
        Behavior on height { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
        Text {
            x: 20
            anchors.verticalCenter: parent.verticalCenter
            text: "设置已修改，点击「应用」后生效"
            color: T.text3
            font.pixelSize: T.callout
        }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            NxButton { text: "放弃更改"; onClicked: page.s.load() }
            NxButton { kind: "primary"; text: "应用"; onClicked: page.s.apply() }
        }
    }
}
