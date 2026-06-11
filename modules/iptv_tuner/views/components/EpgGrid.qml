import QtQuick

FocusScope {
    id: gridRoot
    property var channels: []
    property var programmes: []
    property var theme: ({})
    property bool interactive: false
    property int scrollIntervalMs: 3000
    property int focusRow: 0
    property int focusCol: 0
    property int scrollOffset: 0

    readonly property int rowHeight: root.sh * 0.07
    readonly property int timeColWidth: root.sw * 0.55
    readonly property int channelColWidth: root.sw * 0.2

    signal tuneChannel(var channel)
    signal goBack()

    function programmesFor(channelId) {
        var list = []
        for (var i = 0; i < programmes.length; i++) {
            if (programmes[i].channelId === channelId) list.push(programmes[i])
        }
        return list
    }

    function categoryColor(contentType) {
        return iptvTunerBackend.getCategoryColor(contentType || "other")
    }

    Rectangle {
        anchors.fill: parent
        color: theme.background || root.surfaceColor
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.08
        anchors.leftMargin: root.sw * 0.125
        text: "TV GUIDE"
        color: theme.header || root.primaryColor
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.05
    }

    ListView {
        id: channelList
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.15
        anchors.leftMargin: root.sw * 0.125
        width: channelColWidth
        clip: true
        model: channels
        currentIndex: focusRow
        interactive: false

        delegate: Rectangle {
            width: channelList.width
            height: rowHeight
            color: index % 2 === 0 ? (theme.rowPrimary || root.surfaceColor) : (theme.rowAlt || root.tertiaryColor)
            Text {
                anchors.fill: parent
                anchors.leftMargin: root.sw * 0.01
                verticalAlignment: Text.AlignVCenter
                text: (modelData.displayNumber || "") + " " + (modelData.name || "")
                color: theme.text || root.primaryColor
                font.family: root.globalFont
                font.pixelSize: root.sh * 0.035
                elide: Text.ElideRight
            }
        }
    }

    ListView {
        id: progList
        anchors.top: channelList.top
        anchors.bottom: channelList.bottom
        anchors.left: channelList.right
        anchors.right: parent.right
        anchors.rightMargin: root.sw * 0.125
        clip: true
        model: channels
        currentIndex: focusRow
        interactive: false

        delegate: Item {
            width: progList.width
            height: rowHeight
            Row {
                anchors.fill: parent
                spacing: root.sw * 0.005
                Repeater {
                    model: gridRoot.programmesFor(modelData.tvgId || "").slice(0, 3)
                    Rectangle {
                        width: timeColWidth / 3 - root.sw * 0.005
                        height: parent.height
                        color: gridRoot.categoryColor(modelData.contentType)
                        Text {
                            anchors.fill: parent
                            anchors.margins: root.sw * 0.005
                            text: modelData.title || ""
                            color: "#000000"
                            font.family: root.globalFont
                            font.pixelSize: root.sh * 0.03
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }

    function syncGuideRow() {
        focusRow = iptvTunerBackend.computeGuideRow()
        channelList.currentIndex = focusRow
        progList.currentIndex = focusRow
        channelList.positionViewAtIndex(focusRow, ListView.Contain)
        progList.positionViewAtIndex(focusRow, ListView.Contain)
    }

    Timer {
        id: passiveScroll
        running: !interactive && channels.length > 0
        interval: scrollIntervalMs
        repeat: true
        onTriggered: gridRoot.syncGuideRow()
    }

    onChannelsChanged: {
        if (!interactive && channels.length > 0)
            syncGuideRow()
    }

    Component.onCompleted: {
        if (!interactive && channels.length > 0)
            syncGuideRow()
    }

    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) {
            goBack()
            event.accepted = true
        } else if (interactive) {
            if (event.key === Qt.Key_Up && focusRow > 0) { focusRow--; event.accepted = true }
            else if (event.key === Qt.Key_Down && focusRow < channels.length - 1) { focusRow++; event.accepted = true }
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (channels[focusRow]) tuneChannel(channels[focusRow])
                event.accepted = true
            }
        }
        channelList.currentIndex = focusRow
        progList.currentIndex = focusRow
    }
}
