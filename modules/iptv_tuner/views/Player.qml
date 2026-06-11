import QtQuick
import "./components"

FocusScope {
    id: playerRoot

    property var navParams: ({})
    signal goBack()

    property var channel: navParams.channel || ({})
    property var settings: navParams.settings || iptvTunerBackend.moduleSettings()

    ChannelNumberOsd {
        id: channelOsd
        anchors.fill: parent
        colorMode: settings.channel_osd_color || "green"
        holdMs: settings.channel_osd_hold_ms || 2000
        Component.onCompleted: show(channel.displayNumber || "", settings.channel_osd_hold_ms || 2000)
    }

    Connections {
        target: mpvController
        function onPlaybackFinished() {
            iptvTunerBackend.endStream()
            goBack()
        }
        function onPlaybackFailed() {
            iptvTunerBackend.endStream()
            goBack()
        }
    }

    Component.onCompleted: {
        var url = channel.streamUrl || ""
        if (url === "") {
            goBack()
            return
        }
        if (!iptvTunerBackend.beginStream()) {
            goBack()
            return
        }
        var autoZoom = settings.auto_zoom_widescreen === true || settings.auto_zoom_widescreen === "ON"
        var timeout = settings.stream_connect_timeout_sec || 10
        var bitrate = settings.max_bitrate_mbps || 0
        mpvController.loadAndPlay(url, 0.0, 0, -1, [], false, -1, 0.0, "", false, "", false,
                                  autoZoom, timeout, bitrate)
    }

    Component.onDestruction: {
        mpvController.stop()
        iptvTunerBackend.endStream()
    }

    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            mpvController.sendKey("ESC")
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            mpvController.sendKey("UP")
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            mpvController.sendKey("DOWN")
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            mpvController.sendKey("LEFT")
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            mpvController.sendKey("RIGHT")
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            mpvController.sendKey("ENTER")
            event.accepted = true
        }
    }
}
