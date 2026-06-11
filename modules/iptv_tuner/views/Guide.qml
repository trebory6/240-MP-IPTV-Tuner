import QtQuick
import "./components"

FocusScope {
    id: guideRoot

    property var navParams: ({})
    signal goBack()
    signal navigateTo(string path, var params, var listState)

    property var channel: navParams.channel || ({})
    property var channels: navParams.channels || []
    property var programmes: navParams.programmes || []
    property var settings: iptvTunerBackend.moduleSettings()
    property bool interactive: settings.guide_interactive === true || settings.guide_interactive === "ON"

    Image {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: root.sh * 0.08
        anchors.rightMargin: root.sw * 0.125
        width: root.sw * 0.15
        height: root.sh * 0.12
        visible: (settings.guide_custom_logo || "") !== ""
        source: (settings.guide_custom_logo || "") !== "" ? "file://" + settings.guide_custom_logo : ""
        fillMode: Image.PreserveAspectFit
    }

    EpgGrid {
        anchors.fill: parent
        channels: guideRoot.channels
        programmes: guideRoot.programmes
        theme: iptvTunerBackend.getGuideTheme()
        interactive: guideRoot.interactive
        scrollIntervalMs: settings.guide_scroll_interval_ms || 3000

        onTuneChannel: function(ch) {
            if (!ch) return
            if (ch.isVirtual) return
            iptvTunerBackend.onVirtualChannelTuneOut()
            navigateTo("Player.qml", { channel: ch, settings: settings }, {})
        }
        onGoBack: {
            iptvTunerBackend.onVirtualChannelTuneOut()
            guideRoot.goBack()
        }
    }
}
