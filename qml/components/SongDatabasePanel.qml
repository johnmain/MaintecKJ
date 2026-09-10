import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: databasePanel
    SplitView.minimumHeight: 200
    SplitView.preferredHeight: 350
    SplitView.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        TextField {
            id: searchField
            placeholderText: "Search songs..."
            Layout.fillWidth: true
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ListModel {
                ListElement { artist: "Artist 1"; title: "Song Title 1"; duration: "3:45" }
                ListElement { artist: "Artist 2"; title: "Song Title 2"; duration: "" }
            }
            delegate: ItemDelegate {
                width: parent.width
                text: artist + " - " + title + " (" + duration + ")"
            }
        }
    }
}
