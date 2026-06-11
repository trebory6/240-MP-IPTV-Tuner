import QtQuick

Item {
    id: osdRoot
    visible: opacity > 0.01
    opacity: 0

    property string displayText: ""
    property string colorMode: "green"
    property int holdMs: 2000

    readonly property color osdColor: colorMode === "white" ? "#FFFFFF" : "#4AF626"

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.125
        anchors.leftMargin: root.sw * 0.125
        text: osdRoot.displayText
        color: osdRoot.osdColor
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.08
    }

    function show(text, hold) {
        displayText = text
        fadeOut.stop()
        showAnim.restart()
        holdTimer.interval = hold !== undefined ? hold : holdMs
        holdTimer.restart()
    }

    SequentialAnimation {
        id: showAnim
        PropertyAnimation { target: osdRoot; property: "opacity"; to: 1.0; duration: 80 }
    }

    Timer {
        id: holdTimer
        onTriggered: fadeOut.restart()
    }

    SequentialAnimation {
        id: fadeOut
        PropertyAnimation { target: osdRoot; property: "opacity"; to: 0.0; duration: 400 }
    }
}
