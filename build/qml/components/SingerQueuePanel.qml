import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: queuePanel
    SplitView.minimumHeight: 150
    SplitView.preferredHeight: 250

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: "Current Singer Queue"
            font.bold: true
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ListModel {
                ListElement { title: "Song in Queue 1"; artist: "Queue Artist"; duration: "3:30" }
                ListElement { title: "Song in Queue 2"; artist: "Queue Artist"; duration: "4:15" }
            }
            delegate: ItemDelegate {
                width: parent.width
                text: title + " - " + artist + " (" + duration + ")"
            }
        }
    }
}
