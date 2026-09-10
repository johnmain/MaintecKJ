import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: singerPanel
    width: 200
    height: 400

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: "Singer Rotation List"
            font.bold: true
            font.pixelSize: 16
            Layout.fillWidth: true
        }

        TextField {
            id: newSingerInput
            placeholderText: "Enter singer name..."
            Layout.fillWidth: true
            onEditingFinished: {
                var name = newSingerInput.text.trim()
                if (name.length > 0) {
                    singerModel.addSinger(name)
                    newSingerInput.text = ""
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Button {
                id: addButton
                text: "Add"
                Layout.fillWidth: true
                onClicked: {
                    var name = newSingerInput.text.trim()
                    if (name.length > 0) {
                        singerModel.addSinger(name)
                        newSingerInput.text = ""
                    }
                }
            }

            Button {
                id: removeButton
                text: "Remove"
                Layout.fillWidth: true
                onClicked: {
                    if (singerList.currentIndex >= 0) {
                        singerModel.removeSinger(singerList.currentIndex)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Button {
                text: "Up"
                Layout.fillWidth: true
                onClicked: {
                    if (singerList.currentIndex > 0) {
                        singerModel.moveSinger(singerList.currentIndex, singerList.currentIndex - 1)
                    }
                }
            }

            Button {
                text: "Down"
                Layout.fillWidth: true
                onClicked: {
                    if (singerList.currentIndex >= 0 && singerList.currentIndex < singerModel.rowCount() - 1) {
                        singerModel.moveSinger(singerList.currentIndex, singerList.currentIndex + 1)
                    }
                }
            }
        }

        Button {
            text: "Toggle Inactive"
            Layout.fillWidth: true
            onClicked: {
                if (singerList.currentIndex >= 0) {
                    singerModel.toggleSingerStatus(singerList.currentIndex)
                }
            }
        }

        Label {
            text: "Total Singers: " + (singerModel ? singerModel.rowCount() : 0)
            font.pixelSize: 12
            color: "#666666"
            Layout.fillWidth: true
        }

        ListView {
            id: singerList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: singerModel
            delegate: ItemDelegate {
                width: parent.width
                height: 40

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 5
                    spacing: 10

                    Label {
                        text: model.singerName
                        font.bold: model.singerStatus === "Active"
                        color: model.singerStatus === "Active" ? "#000000" : "#888888"
                        Layout.fillWidth: true
                    }

                    Label {
                        text: model.singerStatus === "Active" ? "✓" : "✗"
                        font.pixelSize: 14
                        color: model.singerStatus === "Active" ? "#00AA00" : "#CC0000"
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: singerList.currentIndex = index
                    onDoubleClicked: {
                        singerModel.toggleSingerStatus(index)
                    }
                }
            }

            highlight: Rectangle {
                color: "#E0E0E0"
                radius: 3
            }
            highlightFollowsCurrentItem: true
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
