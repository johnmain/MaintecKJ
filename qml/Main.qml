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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Karaoke or background music. The two libraries have nothing to do with
        // each other, so this swaps the panels rather than filtering one list.
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 6
            spacing: 12

            TabBar {
                id: modeTabs
                Layout.fillWidth: false

                TabButton {
                    text: "\uD83C\uDFA4  Karaoke"
                    implicitWidth: 150
                }
                TabButton {
                    text: "\uD83C\uDFB5  Background Music"
                    implicitWidth: 200
                }

                Component.onCompleted: currentIndex = modeController.background ? 1 : 0
                onCurrentIndexChanged: modeController.mode = (currentIndex === 1) ? "background" : "karaoke"

                // The mode can also change from the stored setting, so follow it
                // back. The guard stops the two from chasing each other.
                Connections {
                    target: modeController
                    function onModeChanged() {
                        var wanted = modeController.background ? 1 : 0
                        if (modeTabs.currentIndex !== wanted)
                            modeTabs.currentIndex = wanted
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                opacity: 0.7
                font.pixelSize: 11
                elide: Text.ElideRight
                text: modeController.background
                      ? "Background music plays down the playlist; the next song follows automatically."
                      : "Karaoke queues songs per singer. The rotation advances but never auto-plays."
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            // Singer Panel (Left) - stays put in both modes
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

                // Library (Top Center): karaoke songs or background music
                Loader {
                    id: databasePanel
                    source: modeController.background
                            ? Qt.resolvedUrl("components/BackgroundLibraryPanel.qml")
                            : Qt.resolvedUrl("components/SongDatabasePanel.qml")
                    SplitView.minimumHeight: 200
                    SplitView.fillHeight: true
                }

                // Singer queue or background playlist (Bottom Center) - defaults
                // to roughly a third of the window
                Loader {
                    id: queuePanel
                    source: modeController.background
                            ? Qt.resolvedUrl("components/BackgroundPlaylistPanel.qml")
                            : Qt.resolvedUrl("components/SingerQueuePanel.qml")
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
}
