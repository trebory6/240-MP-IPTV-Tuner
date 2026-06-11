import QtQuick

Rectangle {
    id: overlay
    anchors.fill: parent
    visible: false
    color: "#101010"
    z: 100

    property string mode: "static"
    property string customImage: ""
    property string labelText: ""

    Image {
        anchors.fill: parent
        visible: mode === "custom_image" && customImage !== ""
        source: customImage !== "" ? "file://" + customImage : ""
        fillMode: Image.PreserveAspectFit
    }

    Rectangle {
        anchors.fill: parent
        visible: mode === "smpte_bars"
        Row {
            anchors.fill: parent
            Repeater {
                model: ["#C0C0C0", "#C0C000", "#00C0C0", "#00C000", "#C000C0", "#C00000", "#0000C0"]
                Rectangle { width: parent.width / 7; height: parent.height; color: modelData }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: mode === "please_stand_by"
        text: "PLEASE STAND BY"
        color: "#FFFFFF"
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.06
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: root.sh * 0.15
        visible: labelText !== ""
        text: labelText
        color: "#4AF626"
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.04
    }

    function play(transitionMode, customPath, label, doneCallback) {
        mode = transitionMode || "static"
        customImage = customPath || ""
        labelText = label || ""
        visible = mode !== "none"
        if (!visible) {
            if (doneCallback) doneCallback()
            return
        }
        hideTimer.callback = doneCallback
        hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: 500
        property var callback: null
        onTriggered: {
            visible = false
            if (callback) callback()
        }
    }
}
