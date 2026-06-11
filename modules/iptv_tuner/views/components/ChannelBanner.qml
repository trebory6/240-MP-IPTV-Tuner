import QtQuick

Item {
    property string channelName: ""
    property string channelNumber: ""
    property string logoUrl: ""

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        text: channelNumber + "  " + channelName
        color: root.primaryColor
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.04
    }
}
