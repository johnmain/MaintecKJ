import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 1280
    height: 720
    title: "MaintecKJ - Test"

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Load SingerPanel directly from file
        Loader {
            source: "file://" + Qt.resolvedUrl("qml/components/SingerPanel.qml")
            id: singerPanel
        }

        SplitView {
            id: centerPanel
            SplitView.fillWidth: true
            orientation: Qt.Vertical

            Loader {
                source: "file://" + Qt.resolvedUrl("qml/components/SongDatabasePanel.qml")
                id: databasePanel
            }

            Loader {
                source: "file://" + Qt.resolvedUrl("qml/components/SingerQueuePanel.qml")
                id: queuePanel
            }
        }

        Loader {
            source: "file://" + Qt.resolvedUrl("qml/components/DeckPanel.qml")
            id: deckPanel
        }
    }
}
