import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

ApplicationWindow {
    id: mainWindow
    visible: true
    width: settings.windowWidth
    height: settings.windowHeight
    title: "MaintecKJ - Karaoke Host System"

    // Persisted UI state (window size + panel proportions)
    Settings {
        id: settings
        category: "MainWindow"
        property real windowWidth: 1280
        property real windowHeight: 720
        property real leftPanelWidth: 280
        property real rightPanelWidth: 320
        property real queuePanelFraction: 0.33
    }

    onWidthChanged: settings.windowWidth = width
    onHeightChanged: settings.windowHeight = height

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Singer Panel (Left)
        Loader {
            id: singerPanel
            source: Qt.resolvedUrl("components/SingerPanel.qml")
            SplitView.minimumWidth: 260
            SplitView.maximumWidth: 480
            SplitView.preferredWidth: Math.max(280, settings.leftPanelWidth)
            onWidthChanged: {
                if (Math.abs(settings.leftPanelWidth - width) > 1)
                    settings.leftPanelWidth = width
            }
        }

        SplitView {
            id: centerPanel
            SplitView.fillWidth: true
            orientation: Qt.Vertical

            // Extra height given to the singer queue on top of the remembered
            // share of the window. It is deliberately not folded into the saved
            // fraction, otherwise it would grow by this much on every launch.
            readonly property real queueExtraHeight: 150

            // Song Database Panel (Top Center)
            Loader {
                id: databasePanel
                source: Qt.resolvedUrl("components/SongDatabasePanel.qml")
                SplitView.minimumHeight: 200
                SplitView.fillHeight: true
            }

            // Singer Queue Panel (Bottom Center) — defaults to ~1/3 of the window
            Loader {
                id: queuePanel
                source: Qt.resolvedUrl("components/SingerQueuePanel.qml")
                SplitView.minimumHeight: 150
                SplitView.preferredHeight: Math.max(150, mainWindow.height * settings.queuePanelFraction)
                                          + centerPanel.queueExtraHeight
                onHeightChanged: {
                    if (mainWindow.height > 40) {
                        var f = (height - centerPanel.queueExtraHeight) / mainWindow.height
                        if (f < 0)
                            f = 0
                        if (Math.abs(settings.queuePanelFraction - f) > 0.02)
                            settings.queuePanelFraction = f
                    }
                }
            }
        }

        // Deck Panel (Right)
        Loader {
            id: deckPanel
            source: Qt.resolvedUrl("components/DeckPanel.qml")
            SplitView.minimumWidth: 250
            SplitView.maximumWidth: 480
            SplitView.preferredWidth: settings.rightPanelWidth
            onWidthChanged: {
                if (Math.abs(settings.rightPanelWidth - width) > 1)
                    settings.rightPanelWidth = width
            }
        }
    }
}
