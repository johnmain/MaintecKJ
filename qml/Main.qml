import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 1280
    height: 720
    title: "MaintecKJ - Karaoke Host System"

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Singer Panel (Left)
        Loader {
            source: Qt.resolvedUrl("components/SingerPanel.qml")
            id: singerPanel
            SplitView.minimumWidth: 200
            SplitView.preferredWidth: 280
            SplitView.maximumWidth: 450
        }

        SplitView {
            id: centerPanel
            SplitView.fillWidth: true
            orientation: Qt.Vertical

            // Song Database Panel (Top Center)
            Loader {
                source: Qt.resolvedUrl("components/SongDatabasePanel.qml")
                id: databasePanel
                SplitView.minimumHeight: 200
                SplitView.preferredHeight: 350
                SplitView.fillHeight: true
            }

            // Singer Queue Panel (Bottom Center)
            Loader {
                source: Qt.resolvedUrl("components/SingerQueuePanel.qml")
                id: queuePanel
                SplitView.minimumHeight: 150
                SplitView.preferredHeight: 250
            }
        }

        // Deck Panel (Right)
        Loader {
            source: Qt.resolvedUrl("components/DeckPanel.qml")
            id: deckPanel
            SplitView.minimumWidth: 250
            SplitView.preferredWidth: 320
            SplitView.maximumWidth: 480
        }
    }
}
