import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Pane {
    id: deckPanel
    SplitView.minimumWidth: 280
    SplitView.preferredWidth: 320
    SplitView.maximumWidth: 480

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: width * (9 / 16)
            color: "#000000"
            border.color: "#444444"
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: "Secondary Window Preview"
                color: "#888888"
                font.pixelSize: 13
            }
        }

        Button {
            text: "Launch Secondary Window"
            Layout.fillWidth: true
            onClicked: secondaryWindow.show()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#333333"
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: "Now Playing"
                font.bold: true
                font.pixelSize: 16
            }

            Label {
                text: " Track Loaded"
                font.pixelSize: 14
                color: "#aaaaaa"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            ProgressBar {
                Layout.fillWidth: true
                value: 0.0
            }
        }

        Item {
            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Key / Pitch Change"
                font.bold: true
                font.pixelSize: 12
                color: "#cccccc"
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 2
                Button { text: "-4"; Layout.fillWidth: true }
                Button { text: "-3"; Layout.fillWidth: true }
                Button { text: "-2"; Layout.fillWidth: true }
                Button { text: "-1"; Layout.fillWidth: true }
                Button { text: "0";  Layout.fillWidth: true; highlighted: true }
                Button { text: "+1"; Layout.fillWidth: true }
                Button { text: "+2"; Layout.fillWidth: true }
                Button { text: "+3"; Layout.fillWidth: true }
                Button { text: "+4"; Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label { text: "Tempo"; font.pixelSize: 12 }
                Slider { Layout.fillWidth: true; value: 0.5 }
            }
        }
    }

    Window {
        id: secondaryWindow
        title: "MaintecKJ - Display Output"
        width: 800
        height: 600
        visible: false
    }
}
