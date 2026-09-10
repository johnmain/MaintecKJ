import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: secondaryWindow
    title: "MaintecKJ - Display Output"
    width: 800
    height: 600
    visible: false
    color: "black"

    Rectangle {
        anchors.fill: parent
        color: "black"

        Column {
            anchors.centerIn: parent
            spacing: 20

            Label {
                text: "Secondary Display Window"
                color: "white"
                font.pixelSize: 24
            }

            Label {
                text: "CDG Graphics / Video Output"
                color: "gray"
                font.pixelSize: 16
            }

            Button {
                text: "Hide Window"
                onClicked: secondaryWindow.hide()
            }
        }
    }
}