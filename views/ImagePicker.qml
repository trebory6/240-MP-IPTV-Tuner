import QtQuick
import Components

FocusScope {
    id: pickerRoot

    signal goBack()

    property var navParams: ({})
    property string moduleId: navParams.moduleId || ""
    property string settingKey: navParams.settingKey || ""

    property string currentBrowsePath: ""
    property var dirEntries: []
    property var fileEntries: []

    property var listModel: {
        var items = [
            { name: "..PARENT DIRECTORY", entryType: "up" },
            { name: "<CLEAR SELECTION>", entryType: "clear" }
        ]
        for (var i = 0; i < fileEntries.length; i++) {
            items.push({ name: fileEntries[i].name, path: fileEntries[i].path, entryType: "file" })
        }
        for (var j = 0; j < dirEntries.length; j++) {
            items.push({ name: dirEntries[j].name + "/", path: dirEntries[j].path, entryType: "dir" })
        }
        return items
    }

    function loadEntries() {
        dirEntries = appCore.listDirectories(currentBrowsePath)
        fileEntries = appCore.listImageFiles(currentBrowsePath)
        fileList.currentIndex = 0
    }

    function navigateInto(path) {
        currentBrowsePath = path
        loadEntries()
    }

    function goUp() {
        var parent = appCore.parentDirectory(currentBrowsePath)
        if (parent === currentBrowsePath) return
        currentBrowsePath = parent
        loadEntries()
    }

    function selectFile(path) {
        appCore.save_setting(moduleId, settingKey, path)
        goBack()
    }

    function clearSelection() {
        appCore.save_setting(moduleId, settingKey, "")
        goBack()
    }

    Component.onCompleted: {
        var saved = appCore.get_setting(moduleId, settingKey) || ""
        if (saved !== "") {
            var slash = Math.max(saved.lastIndexOf("/"), saved.lastIndexOf("\\"))
            currentBrowsePath = slash > 0 ? saved.substring(0, slash) : appCore.homePath()
        } else {
            currentBrowsePath = appCore.homePath()
        }
        loadEntries()
    }

    AppBar {
        iconSource: "../../assets/images/settings.svg"
        title: "IMAGE"
        subtitle: currentBrowsePath
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.125
        anchors.leftMargin: root.sw * 0.125
    }

    ListView {
        id: fileList
        model: pickerRoot.listModel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.sh * 0.25
        anchors.leftMargin: root.sw * 0.115625
        width: root.sw * 0.76875
        height: root.sh * 0.525
        clip: true
        focus: true

        Keys.onUpPressed: { if (currentIndex > 0) currentIndex-- }
        Keys.onDownPressed: { if (currentIndex < count - 1) currentIndex++ }
        Keys.onReturnPressed: {
            var entry = pickerRoot.listModel[currentIndex]
            if (!entry) return
            if (entry.entryType === "up") pickerRoot.goUp()
            else if (entry.entryType === "clear") pickerRoot.clearSelection()
            else if (entry.entryType === "file") pickerRoot.selectFile(entry.path)
            else if (entry.entryType === "dir") pickerRoot.navigateInto(entry.path)
        }
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace || event.key === Qt.Key_Back) {
                pickerRoot.goBack()
                event.accepted = true
            }
        }

        delegate: Item {
            width: fileList.width
            height: root.sh * 0.0583333
            Rectangle {
                anchors.fill: parent
                color: fileList.currentIndex === index ? root.accentColor : "transparent"
                Text {
                    text: modelData.name || ""
                    color: {
                        if (fileList.currentIndex === index) return root.surfaceColor
                        if (modelData.entryType === "up" || modelData.entryType === "clear") return root.accentColor
                        if (modelData.entryType === "file") return root.secondaryColor
                        return root.primaryColor
                    }
                    font.family: root.globalFont
                    font.capitalization: Font.AllUppercase
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: root.sw * 0.009375
                    font.pixelSize: root.sh * 0.05
                }
            }
        }
    }

    Text {
        text: "[ESC]:CANCEL  [▲▼]:NAVIGATE  [ENTER]:SELECT"
        color: root.tertiaryColor
        font.family: root.globalFont
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: root.sh * 0.1041667
        anchors.leftMargin: root.sw * 0.125
        font.pixelSize: root.sh * 0.0333333
    }
}
