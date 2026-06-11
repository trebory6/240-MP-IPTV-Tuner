import QtQuick
import Components

FocusScope {
    id: setupRoot

    property var navParams: ({})
    signal goBack()
    signal replaceWith(string path, var params)

    property int step: 0
    property string m3uUrl: ""
    property string epgUrl: ""
    property string statusText: ""
    property bool busy: false
    property var keyRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["A","B","C","D","E","F","G","H","I","J"],
        ["K","L","M","N","O","P","Q","R","S","T"],
        ["U","V","W","X","Y","Z",":","/",".","-"],
        ["_","@","DEL","DONE"]
    ]
    property int keyRow: 0
    property int keyCol: 0

    function activeField() {
        return step === 0 ? m3uUrl : epgUrl
    }

    function setActiveField(val) {
        if (step === 0) m3uUrl = val
        else epgUrl = val
    }

    function keyLabel(r, c) {
        if (r < keyRows.length && c < keyRows[r].length) return keyRows[r][c]
        return ""
    }

    function pressKey() {
        var k = keyLabel(keyRow, keyCol)
        if (k === "DEL") setActiveField(activeField().slice(0, -1))
        else if (k === "DONE") {
            if (step === 0) { step = 1; statusText = "" }
            else validateAndSave()
        } else setActiveField(activeField() + k)
    }

    function validateAndSave() {
        busy = true
        statusText = "VALIDATING..."
        iptvTunerBackend.validateSources(m3uUrl, epgUrl)
    }

    function saveAndContinue() {
        appCore.save_setting("com.240mp.iptv_tuner", "m3u_url", m3uUrl)
        appCore.save_setting("com.240mp.iptv_tuner", "epg_url", epgUrl)
        iptvTunerBackend.loadGuide()
        replaceWith("Tuner.qml", { startupChannel: iptvTunerBackend.startupChannelNumber() })
    }

    Connections {
        target: iptvTunerBackend
        function onSourcesValidated(ok, message) {
            busy = false
            if (!ok) {
                statusText = message
                return
            }
            statusText = "OK"
            saveAndContinue()
        }
    }

    Component.onCompleted: {
        if (navParams.reconfigure) {
            m3uUrl = appCore.get_setting("com.240mp.iptv_tuner", "m3u_url") || ""
            epgUrl = appCore.get_setting("com.240mp.iptv_tuner", "epg_url") || ""
        }
    }

    AppBar {
        iconSource: moduleRoot.moduleIcon
        title: step === 0 ? "M3U URL" : "EPG XML URL"
        subtitle: moduleRoot.moduleName
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.125
        anchors.leftMargin: root.sw * 0.125
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.22
        anchors.leftMargin: root.sw * 0.125
        width: root.sw * 0.75
        text: activeField() || "ENTER URL..."
        color: root.primaryColor
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.04
        elide: Text.ElideLeft
        clip: true
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.28
        anchors.leftMargin: root.sw * 0.125
        width: root.sw * 0.75
        text: statusText
        color: root.accentColor
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.035
        wrapMode: Text.WordWrap
    }

    Grid {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: root.sh * 0.12
        columns: 10
        rowSpacing: root.sh * 0.008
        columnSpacing: root.sw * 0.008
        Repeater {
            model: 45
            delegate: Rectangle {
                property int r: Math.floor(index / 10)
                property int c: index % 10
                visible: r < keyRows.length && c < keyRows[r].length
                width: root.sw * 0.075
                height: root.sh * 0.055
                color: (keyRow === r && keyCol === c) ? root.accentColor : root.tertiaryColor
                Text {
                    anchors.centerIn: parent
                    text: keyLabel(r, c)
                    color: (keyRow === r && keyCol === c) ? root.surfaceColor : root.primaryColor
                    font.family: root.globalFont
                    font.pixelSize: root.sh * 0.03
                }
            }
        }
    }

    focus: true
    Keys.onPressed: function(event) {
        if (busy) { event.accepted = true; return }
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            if (step > 0) { step = 0; event.accepted = true; return }
            goBack()
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            keyRow = Math.max(0, keyRow - 1)
            keyCol = Math.min(keyCol, keyRows[keyRow].length - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            keyRow = Math.min(keyRows.length - 1, keyRow + 1)
            keyCol = Math.min(keyCol, keyRows[keyRow].length - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            keyCol = Math.max(0, keyCol - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            keyCol = Math.min(keyRows[keyRow].length - 1, keyCol + 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            pressKey()
            event.accepted = true
        }
    }

    Text {
        text: "[ESC]:BACK  [▲▼◄►]:KEYS  [ENTER]:INSERT/DONE"
        color: root.tertiaryColor
        font.family: root.globalFont
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: root.sh * 0.06
        anchors.leftMargin: root.sw * 0.125
        font.pixelSize: root.sh * 0.03
    }
}
