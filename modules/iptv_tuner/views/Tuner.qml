import QtQuick
import "./components"

FocusScope {
    id: tunerRoot

    property var navParams: ({})
    signal goBack()
    signal navigateTo(string path, var params, var listState)
    signal replaceWith(string path, var params)

    property var channels: []
    property var programmes: []
    property var settings: ({})
    property string currentChannelNumber: navParams.startupChannel || ""
    property string pendingStartup: navParams.startupChannel || ""
    property string digitBuffer: ""
    property string viewMode: "tuner"
    property var currentChannel: ({})

    Timer {
        id: digitTimer
        interval: settings.channel_digit_delay_ms || 1500
        onTriggered: commitDigits()
    }

    function loadSettings() {
        settings = iptvTunerBackend.moduleSettings()
    }

    function commitDigits() {
        if (digitBuffer === "") return
        tuneToNumber(digitBuffer)
        digitBuffer = ""
    }

    function tuneToNumber(num) {
        iptvTunerBackend.onVirtualChannelTuneOut()
        var ch = iptvTunerBackend.resolveChannel(num)
        if (!ch || Object.keys(ch).length === 0) {
            showBlank(num)
            return
        }
        currentChannel = ch
        currentChannelNumber = ch.displayNumber || num
        iptvTunerBackend.saveLastChannel(currentChannelNumber)
        channelOsd.show(currentChannelNumber, settings.channel_osd_hold_ms || 2000)

        if (ch.isVirtual) {
            if (ch.virtualType === "guide") {
                iptvTunerBackend.onVirtualChannelTuneIn("guide")
                navigateTo("Guide.qml", { channel: ch, programmes: programmes, channels: channels }, {})
                return
            }
            if (ch.virtualType === "weather") {
                iptvTunerBackend.onVirtualChannelTuneIn("weather")
                navigateTo("Weather.qml", { channel: ch }, {})
                return
            }
        }

        var trans = settings.channel_transition || "static"
        var transCustom = settings.channel_transition_custom || ""
        transition.play(trans, transCustom, ch.name, function() {
            navigateTo("Player.qml", { channel: ch, settings: settings }, {})
        })
    }

    function showBlank(num) {
        currentChannelNumber = num
        viewMode = "blank"
        var bg = settings.blank_channel_background || "static"
        var custom = settings.blank_channel_custom || ""
        transition.mode = bg
        transition.customImage = custom
        transition.labelText = "NO SIGNAL"
        transition.visible = true
        channelOsd.show(num, settings.channel_osd_hold_ms || 2000)
    }

    function channelUp() {
        var ch = iptvTunerBackend.adjacentChannel(currentChannelNumber, 1)
        if (ch && ch.displayNumber) tuneToNumber(ch.displayNumber)
    }

    function channelDown() {
        var ch = iptvTunerBackend.adjacentChannel(currentChannelNumber, -1)
        if (ch && ch.displayNumber) tuneToNumber(ch.displayNumber)
    }

    Connections {
        target: iptvTunerBackend
        function onChannelsReady(list) { channels = list }
        function onGuideReady(chList, progList) {
            channels = chList
            programmes = progList
            if (pendingStartup !== "") {
                var n = pendingStartup
                pendingStartup = ""
                tuneToNumber(n)
            }
        }
        function onLoadFailed(message) {
            transition.labelText = message
            transition.visible = true
        }
    }

    Component.onCompleted: {
        loadSettings()
        if (channels.length === 0) iptvTunerBackend.loadGuide()
        else if (pendingStartup !== "") {
            var n = pendingStartup
            pendingStartup = ""
            tuneToNumber(n)
        }
        if (iptvTunerBackend.isFirstTvSession()) iptvTunerBackend.markFirstTvSessionComplete()
    }

    Rectangle {
        anchors.fill: parent
        color: root.surfaceColor
        visible: viewMode === "tuner" || viewMode === "blank"
    }

    TransitionOverlay { id: transition; anchors.fill: parent }

    ChannelNumberOsd {
        id: channelOsd
        anchors.fill: parent
        colorMode: settings.channel_osd_color || "green"
        holdMs: settings.channel_osd_hold_ms || 2000
    }

    focus: true
    Keys.onPressed: function(event) {
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            digitBuffer += String.fromCharCode(event.key)
            channelOsd.show(digitBuffer, settings.channel_osd_hold_ms || 2000)
            digitTimer.restart()
            event.accepted = true
        } else if (event.key === Qt.Key_ChannelUp || event.key === Qt.Key_PageUp) {
            channelUp()
            event.accepted = true
        } else if (event.key === Qt.Key_ChannelDown || event.key === Qt.Key_PageDown) {
            channelDown()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) {
            goBack()
            event.accepted = true
        }
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: root.sh * 0.08
        anchors.leftMargin: root.sw * 0.125
        text: "CH " + currentChannelNumber + "  [0-9]:TUNE  [CH+/CH-]:SCAN  [ESC]:EXIT"
        color: root.tertiaryColor
        font.family: root.globalFont
        font.pixelSize: root.sh * 0.03
    }
}
