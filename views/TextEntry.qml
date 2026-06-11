import QtQuick
import Components

FocusScope {
    id: textEntryRoot

    signal goBack()
    signal saved(string value)

    property var navParams: ({})
    property string moduleId: navParams.moduleId || ""
    property string settingKey: navParams.settingKey || ""
    property string settingLabel: navParams.settingLabel || ""
    property string initialValue: navParams.initialValue || ""
    property bool masked: navParams.masked || false
    property string mode: navParams.mode || "text"  // "text" | "digits"

    property string editValue: initialValue
    property int cursorPos: editValue.length
    property int keyRow: 0
    property int keyCol: 0

    readonly property var digitKeys: ["0","1","2","3","4","5","6","7","8","9",".","/",":","-","_"]
    readonly property var alphaRows: [
        ["A","B","C","D","E","F"],
        ["G","H","I","J","K","L"],
        ["M","N","O","P","Q","R"],
        ["S","T","U","V","W","X"],
        ["Y","Z","@","#","&"," "]
    ]

    function insertChar(ch) {
        var before = editValue.substring(0, cursorPos)
        var after = editValue.substring(cursorPos)
        editValue = before + ch + after
        cursorPos++
    }

    function deleteChar() {
        if (cursorPos <= 0) return
        editValue = editValue.substring(0, cursorPos - 1) + editValue.substring(cursorPos)
        cursorPos--
    }

    function currentKeyLabel() {
        if (mode === "digits") {
            if (keyRow === 0) return digitKeys[keyCol] || ""
            if (keyRow === 1 && keyCol === 0) return "DEL"
            if (keyRow === 1 && keyCol === 1) return "DONE"
            return ""
        }
        if (keyRow < alphaRows.length)
            return alphaRows[keyRow][keyCol] || ""
        if (keyRow === alphaRows.length && keyCol === 0) return "DEL"
        if (keyRow === alphaRows.length && keyCol === 1) return "DONE"
        return ""
    }

    function activateKey() {
        var label = currentKeyLabel()
        if (label === "DEL") { deleteChar(); return }
        if (label === "DONE") {
            appCore.save_setting(moduleId, settingKey, editValue)
            saved(editValue)
            goBack()
            return
        }
        if (label !== "") insertChar(label)
    }

    function moveKey(dr, dc) {
        var maxRow = mode === "digits" ? 1 : alphaRows.length
        var maxCol = mode === "digits" ? digitKeys.length - 1 : 5
        if (mode === "digits" && keyRow === 1) maxCol = 1
        if (keyRow < alphaRows.length && mode !== "digits") maxCol = alphaRows[keyRow].length - 1
        keyRow = Math.max(0, Math.min(maxRow, keyRow + dr))
        if (mode === "digits" && keyRow === 1) maxCol = 1
        else if (keyRow < alphaRows.length && mode !== "digits") maxCol = alphaRows[keyRow].length - 1
        else if (mode === "digits") maxCol = digitKeys.length - 1
        keyCol = Math.max(0, Math.min(maxCol, keyCol + dc))
    }

    Component.onCompleted: {
        editValue = initialValue
        cursorPos = editValue.length
    }

    AppBar {
        iconSource: "../../assets/images/settings.svg"
        title: settingLabel || "EDIT"
        subtitle: moduleId.split(".").pop().toUpperCase()
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
        text: {
            if (masked && editValue.length > 0)
                return "*".repeat(editValue.length)
            return editValue || " "
        }
        color: root.primaryColor
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.045
        elide: Text.ElideLeft
        clip: true
    }

    Grid {
        id: keyGrid
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: root.sh * 0.15
        columns: mode === "digits" ? digitKeys.length : 6
        rowSpacing: root.sh * 0.01
        columnSpacing: root.sw * 0.01

        Repeater {
            model: mode === "digits" ? digitKeys.length + 2 : (alphaRows.length * 6 + 2)
            delegate: Rectangle {
                width: root.sw * 0.11
                height: root.sh * 0.065
                visible: {
                    if (mode === "digits") {
                        if (index < digitKeys.length) return true
                        return index === digitKeys.length || index === digitKeys.length + 1
                    }
                    var r = Math.floor(index / 6)
                    var c = index % 6
                    if (r < alphaRows.length) return c < alphaRows[r].length
                    return index === alphaRows.length * 6 || index === alphaRows.length * 6 + 1
                }
                color: {
                    var r = mode === "digits" ? (index < digitKeys.length ? 0 : 1) : Math.floor(index / 6)
                    var c = mode === "digits" ? (index < digitKeys.length ? index : index - digitKeys.length) : index % 6
                    if (mode === "digits" && index >= digitKeys.length) {
                        r = 1; c = index - digitKeys.length
                    }
                    return (keyRow === r && keyCol === c) ? root.accentColor : root.tertiaryColor
                }
                Text {
                    anchors.centerIn: parent
                    text: {
                        if (mode === "digits") {
                            if (index < digitKeys.length) return digitKeys[index]
                            if (index === digitKeys.length) return "DEL"
                            return "DONE"
                        }
                        var r = Math.floor(index / 6)
                        var c = index % 6
                        if (r < alphaRows.length) return alphaRows[r][c]
                        if (index === alphaRows.length * 6) return "DEL"
                        return "DONE"
                    }
                    color: {
                        var r = mode === "digits" ? (index < digitKeys.length ? 0 : 1) : Math.floor(index / 6)
                        var c = mode === "digits" ? (index < digitKeys.length ? index : index - digitKeys.length) : index % 6
                        if (mode === "digits" && index >= digitKeys.length) { r = 1; c = index - digitKeys.length }
                        return (keyRow === r && keyCol === c) ? root.surfaceColor : root.primaryColor
                    }
                    font.family: root.globalFont
                    font.pixelSize: root.sh * 0.035
                }
            }
        }
    }

    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace || event.key === Qt.Key_Back) {
            if (event.key === Qt.Key_Backspace && !event.isAutoRepeat) {
                deleteChar()
                event.accepted = true
                return
            }
            goBack()
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            moveKey(-1, 0)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            moveKey(1, 0)
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            moveKey(0, -1)
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            moveKey(0, 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            activateKey()
            event.accepted = true
        }
    }

    Text {
        text: "[ESC]:CANCEL  [▲▼◄►]:KEYS  [ENTER]:INSERT/DONE"
        color: root.tertiaryColor
        font.family: root.globalFont
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: root.sh * 0.08
        anchors.leftMargin: root.sw * 0.125
        font.pixelSize: root.sh * 0.03
    }
}
