import QtQuick
import Components

FocusScope {
    id: weatherRoot

    property var navParams: ({})
    signal goBack()

    property var weatherData: ({})
    property var settings: iptvTunerBackend.moduleSettings()

    Connections {
        target: iptvTunerBackend
        function onWeatherReady(data) { weatherData = data }
        function onLoadFailed(msg) { weatherData = { error: msg } }
    }

    Component.onCompleted: {
        iptvTunerBackend.refreshWeather()
    }

    Rectangle {
        anchors.fill: parent
        color: "#003366"
    }

    AppBar {
        iconSource: moduleRoot.moduleIcon
        title: "WEATHER CHANNEL"
        subtitle: weatherData.city || settings.weather_zip || ""
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.125
        anchors.leftMargin: root.sw * 0.125
    }

    Column {
        anchors.centerIn: parent
        spacing: root.sh * 0.03
        width: root.sw * 0.7

        Text {
            text: weatherData.error ? weatherData.error : (Math.round(weatherData.temp || 0) + "°F")
            color: "#FFFFFF"
            font.family: root.globalFont
            font.pixelSize: root.sh * 0.1
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            visible: !weatherData.error
            text: (weatherData.description || "").toUpperCase()
            color: "#AECFFF"
            font.family: root.globalFont
            font.pixelSize: root.sh * 0.05
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            visible: !weatherData.error
            text: "HUMIDITY " + (weatherData.humidity || "--") + "%  WIND " + (weatherData.wind || "--") + " MPH"
            color: "#FFFFFF"
            font.family: root.globalFont
            font.pixelSize: root.sh * 0.04
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) {
            goBack()
            event.accepted = true
        }
    }

    Text {
        text: "[ESC]:BACK"
        color: root.tertiaryColor
        font.family: root.globalFont
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: root.sh * 0.08
        anchors.leftMargin: root.sw * 0.125
        font.pixelSize: root.sh * 0.03
    }
}
