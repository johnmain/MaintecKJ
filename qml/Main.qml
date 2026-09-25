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

    // The background library reaches this through Window.window to mirror the row
    // it is dragging.
    property Item dragOverlayItem: dragOverlay

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

    // Idle backdrop for the secondary window. It lives here rather than in the
    // deck so the Settings tab and the deck share one instance: two Settings
    // objects on the same key do not notify each other, so a change in one would
    // not reach the other until the next launch.
    Settings {
        id: displaySettingsStore
        category: "Display"
        property string backgroundImage: ""
    }
    readonly property QtObject displaySettings: displaySettingsStore

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

                // Settings is a third tab but not a playback mode, so it stays out
                // of modeController and is handled entirely here.
                readonly property int settingsIndex: 2

                TabButton {
                    text: "\uD83C\uDFA4  Karaoke"
                    implicitWidth: 150
                }
                TabButton {
                    text: "\uD83C\uDFB5  Background Music"
                    implicitWidth: 200
                }
                TabButton {
                    text: "\u2699  Settings"
                    implicitWidth: 120
                }

                Component.onCompleted: currentIndex = modeController.background ? 1 : 0
                onCurrentIndexChanged: {
                    if (currentIndex === settingsIndex)
                        return
                    modeController.mode = (currentIndex === 1) ? "background" : "karaoke"
                }

                // The mode can also change from the stored setting, so follow it
                // back. The guard stops the two from chasing each other, and
                // leaves the settings tab alone.
                Connections {
                    target: modeController
                    function onModeChanged() {
                        if (modeTabs.currentIndex === modeTabs.settingsIndex)
                            return
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
            // The Settings tab takes the whole workspace, so the split view steps
            // aside while it is showing.
            visible: modeTabs.currentIndex !== modeTabs.settingsIndex
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

        // Full-window settings, shown in place of the split view. It stays
        // instantiated so its scan-finished hook keeps refreshing the library
        // even while another tab is on screen.
        Loader {
            id: settingsPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: modeTabs.currentIndex === modeTabs.settingsIndex
            source: Qt.resolvedUrl("components/SettingsPanel.qml")
        }
    }

    // A row dragged out of the background library is mirrored here, at window
    // level. Inside the library it would be clipped by the list, and anything
    // the library panel draws itself ends up underneath the panel below it.
    Rectangle {
        id: dragOverlay
        visible: false
        z: 10000
        width: 240
        height: 32
        radius: 4
        color: palette.highlight
        opacity: 0.92
        border.color: palette.highlightedText
        border.width: 1

        property string overlayText: ""

        Label {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            text: dragOverlay.overlayText
            color: palette.highlightedText
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }
}
