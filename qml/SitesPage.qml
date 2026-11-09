// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Scaffold {
    id: page
    title: "资源站点"
    backTip: "返回偏好设置"
    onBack: App.selectPage(5)
    subtitle: sites.status
    fixedHeight: 46
    readonly property var sites: App.sites
    readonly property real contentWidth: width - leading - trailing
    readonly property bool roomy: restingWidth >= 780
    // Wide windows spread every column, so no single gap opens in the middle of a row.
    readonly property real spread: Math.max(1, contentWidth / 1100)
    readonly property int enabledWidth: 56
    readonly property int nameWidth: 190 * spread
    readonly property int categoryWidth: roomy ? 80 * spread : 0
    readonly property int languageWidth: roomy ? 80 * spread : 0
    readonly property int adapterWidth: roomy ? 100 * spread : 0
    readonly property int stateWidth: 140 * spread
    readonly property int hostWidth: contentWidth - enabledWidth - nameWidth - categoryWidth - languageWidth - adapterWidth - stateWidth

    toolbar: [
        NxButton { text: "导入配置"; iconName: "import"; onClicked: page.sites.importRequested() },
        NxButton { text: "导出配置"; iconName: "export"; onClicked: page.sites.exportRequested() }
    ]
    fixed: Row {
        x: page.leading; y: Shell.band + 2
        width: page.contentWidth
        spacing: 10
        NxField {
            width: parent.width - scope.width - test.width - 20
            iconName: "search"
            clearable: true
            placeholderText: "筛选名称、网址、类别或语言"
            Accessible.name: "筛选资源站点"
            onTextChanged: page.sites.filter = text
        }
        NxSelect {
            id: scope
            implicitWidth: 140
            model: [{ text: "全部站点", value: 0 }, { text: "可搜索来源", value: 1 }, { text: "网页目录", value: 2 }]
            value: page.sites.scope
            onChosen: value => page.sites.scope = value
        }
        NxButton {
            id: test
            text: "测试连接"
            iconName: "pulse"
            tip: "勾选启用搜索来源；选中一行后可测试连接，也可以双击该行"
            enabled: page.sites.selected !== ""
            onClicked: page.sites.testSelected()
        }
    }
    Group {
        id: table
        x: page.leading; y: page.contentTop + 4
        width: page.contentWidth
        height: Math.min(parent.height - y - 20, 38 + list.contentHeight)
        clip: true
        component Head: Text {
            height: 38
            leftPadding: 12
            verticalAlignment: Text.AlignVCenter
            color: T.text3
            font.pixelSize: T.callout
            font.weight: Font.DemiBold
            clip: true
        }
        Row {
            Head { width: page.enabledWidth; text: "启用" }
            Head { width: page.nameWidth; text: "名称" }
            Head { width: page.hostWidth; text: "网址" }
            Head { width: page.categoryWidth; visible: page.roomy; text: "类别" }
            Head { width: page.languageWidth; visible: page.roomy; text: "语言" }
            Head { width: page.adapterWidth; visible: page.roomy; text: "搜索方式" }
            Head { width: page.stateWidth; text: "连接状态" }
        }
        Divider { y: 37; width: parent.width }
        ListView {
            id: list
            y: 38
            width: parent.width
            height: parent.height - y
            model: page.sites.model
            clip: true
            interactive: false
            boundsBehavior: Flickable.StopAtBounds
            Tpl.ScrollBar.vertical: NxScrollBar {}
            SmoothWheel { view: list }
            delegate: Rectangle {
                id: row
                required property int index
                required property string siteId
                required property string name
                required property string host
                required property string url
                required property string category
                required property string language
                required property string adapter
                required property bool searchable
                required property bool enabled
                required property string state
                required property string tone
                width: ListView.view.width
                height: 40
                color: page.sites.selected === siteId ? T.accentSoft : rowHover.hovered ? T.surface2 : "transparent"
                HoverHandler { id: rowHover }
                TapHandler {
                    onTapped: page.sites.selected = row.siteId
                    onDoubleTapped: { page.sites.selected = row.siteId; page.sites.testSelected() }
                }
                NxCheck {
                    x: 15
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.searchable
                    checked: row.enabled
                    focusPolicy: Qt.TabFocus
                    onToggled: page.sites.setEnabled(row.siteId, checked)
                    Tip { text: "参与番剧资源搜索" }
                }
                Text {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !row.searchable
                    text: "网页"
                    color: T.text3
                    font.pixelSize: T.callout
                    Tip { text: "网页目录，没有搜索适配器" }
                }
                component Cell: Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: T.text2
                    font.pixelSize: T.body
                    elide: Text.ElideRight
                }
                Cell { x: page.enabledWidth + 12; width: page.nameWidth - 20; text: row.name; color: T.text; Tip { text: row.url } }
                Cell { x: page.enabledWidth + page.nameWidth + 12; width: page.hostWidth - 20; text: row.host; color: T.text3; Tip { text: row.url } }
                Cell { x: page.enabledWidth + page.nameWidth + page.hostWidth + 12; visible: page.roomy; text: row.category }
                Cell { x: page.enabledWidth + page.nameWidth + page.hostWidth + page.categoryWidth + 12; visible: page.roomy; text: row.language }
                Cell { x: page.enabledWidth + page.nameWidth + page.hostWidth + page.categoryWidth + page.languageWidth + 12; visible: page.roomy; text: row.adapter }
                Cell {
                    x: page.enabledWidth + page.nameWidth + page.hostWidth + page.categoryWidth + page.languageWidth + page.adapterWidth + 12
                    width: page.stateWidth - 20
                    text: row.state
                    color: T.tone(row.tone)
                    Tip { text: row.state }
                }
                Divider { x: page.enabledWidth + 12; width: parent.width - x; visible: row.index > 0 }
            }
        }
    }
}
