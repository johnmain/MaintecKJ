import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore
import MaintecKJ 1.0

Pane {
    id: deckPanel
    SplitView.minimumWidth: 250
    SplitView.preferredWidth: 320
    SplitView.maximumWidth: 480

    // Shown on the secondary display whenever there is nothing to put there:
    // between songs, and for the whole time background music is playing.
    Settings {
        id: displaySettings
        category: "Display"
        property string backgroundImage: ""
    }

    FileDialog {
        id: backgroundImageDialog
        title: "Choose a Background Image"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.svg)", "All files (*)"]
        onAccepted: displaySettings.backgroundImage = selectedFile
    }

    function companionCdg(path) {
        if (!path)
            return ""
        if (path.toLowerCase().endsWith(".mp3"))
            return path.substring(0, path.length - 4) + ".cdg"
        return ""
    }

    function formatTime(ms) {
        var total = Math.floor(Number(ms) / 1000)
        if (!total || total <= 0)
            return "0:00"
        var mins = Math.floor(total / 60)
        var secs = total % 60
        return mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    // Stop and Pause both leave mediaPlayer.playing false, so `playing` alone
    // cannot tell them apart. RubberBandAudioEngine::stop() resets the playhead
    // and re-reports the position; pause() leaves it untouched. A position
    // update arriving in the same turn as the drop therefore means Stop, which
    // is why the decision is deferred until the event loop has settled.
    property bool stopDetected: false

    Timer {
        id: stopSettle
        interval: 0
        onTriggered: {
            if (deckPanel.stopDetected) {
                cdgRenderer.unload()
                secondaryCdg.unload()
                deckPanel.stopDetected = false
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // Secondary window mini-preview (CDG canvas / video)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: width * (9 / 16)
            color: "#000000"
            border.color: deckPanel.palette.mid
            border.width: 1

            // Video takes the shared sink only while the secondary window is hidden.
            VideoOutput {
                id: miniVideo
                anchors.fill: parent
                anchors.margins: 1
                fillMode: VideoOutput.PreserveAspectFit
                visible: mediaPlayer.hasVideo
            }

            CdgRenderer {
                id: cdgRenderer
                anchors.fill: parent
                anchors.margins: 1
                visible: !mediaPlayer.hasVideo
            }

            Label {
                anchors.centerIn: parent
                visible: !mediaPlayer.hasVideo && !cdgRenderer.loaded
                text: "No Graphics Loaded"
                color: "white"
                opacity: 0.7
                font.pixelSize: 13
            }
        }

        // Keep CDG graphics locked to the audio player position. Both the mini
        // preview and the secondary display follow the deck from here.
        Connections {
            target: mediaPlayer

            function onSourceChanged() {
                var cdg = deckPanel.companionCdg(mediaPlayer.filePath)
                cdgRenderer.setSource(cdg)
                cdgRenderer.reset()
                secondaryCdg.setSource(cdg)
                secondaryCdg.reset()
            }

            function onPlayingChanged() {
                if (mediaPlayer.playing) {
                    // A finished song unloads the renderers, so a replay needs
                    // their graphics put back before the clock resumes.
                    var cdg = deckPanel.companionCdg(mediaPlayer.filePath)
                    if (!cdgRenderer.loaded)
                        cdgRenderer.setSource(cdg)
                    if (!secondaryCdg.loaded)
                        secondaryCdg.setSource(cdg)
                    cdgRenderer.start()
                    secondaryCdg.start()
                } else {
                    // Maybe a pause, maybe a stop; stopSettle decides once the
                    // engine has had its say. Either way the frame stays for now,
                    // so a pause keeps showing it and a resume just continues.
                    deckPanel.stopDetected = false
                    cdgRenderer.stop()
                    secondaryCdg.stop()
                    stopSettle.restart()
                }
            }

            function onPositionChanged() {
                if (mediaPlayer.playing) {
                    cdgRenderer.syncToPosition(mediaPlayer.position)
                    secondaryCdg.syncToPosition(mediaPlayer.position)
                } else {
                    // Only stop() moves the playhead while not playing.
                    deckPanel.stopDetected = true
                }
            }

            // A finished song clears the display so the idle background shows.
            function onSongFinished() {
                cdgRenderer.unload()
                secondaryCdg.unload()
            }
        }

        Button {
            text: "Launch Secondary Window"
            Layout.fillWidth: true
            onClicked: secondaryWindow.show()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Button {
                text: displaySettings.backgroundImage.length > 0
                      ? "Change Background Image…"
                      : "Add Background Image…"
                Layout.fillWidth: true
                onClicked: backgroundImageDialog.open()
            }

            Button {
                text: "Clear"
                visible: displaySettings.backgroundImage.length > 0
                onClicked: displaySettings.backgroundImage = ""
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: deckPanel.palette.mid
        }

        // Now playing + transport
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: "Now Playing"
                font.bold: true
                font.pixelSize: 16
            }

            Label {
                text: mediaPlayer.hasMedia ? (mediaPlayer.title || "Unknown Title") : "No Track Loaded"
                font.pixelSize: 14
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Label {
                text: mediaPlayer.artist
                font.pixelSize: 12
                opacity: 0.7
                elide: Text.ElideRight
                Layout.fillWidth: true
                visible: mediaPlayer.artist.length > 0
            }

            // Read-only progress indicator (no scrubbing). Use Stop + Play to
            // restart a song instead.
            ProgressBar {
                id: progressBar
                Layout.fillWidth: true
                implicitHeight: 10
                from: 0
                to: Math.max(mediaPlayer.duration, 1)
                value: mediaPlayer.position
                enabled: mediaPlayer.duration > 0
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: deckPanel.formatTime(mediaPlayer.position)
                    font.pixelSize: 11
                    opacity: 0.7
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: deckPanel.formatTime(mediaPlayer.duration)
                    font.pixelSize: 11
                    opacity: 0.7
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    text: mediaPlayer.playing ? "Pause" : "Play"
                    Layout.fillWidth: true
                    enabled: mediaPlayer.hasMedia
                    onClicked: mediaPlayer.togglePlayPause()
                }

                Button {
                    text: "Stop"
                    Layout.fillWidth: true
                    enabled: mediaPlayer.hasMedia
                    onClicked: mediaPlayer.stop()
                }
            }

            // DJ controller status. Left fader drives the tempo, right fader the
            // key shift, right play/pause toggles playback.
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: midiController.connected
                          ? "\uD83C\uDF9B " + midiController.deviceName
                          : "\uD83C\uDF9B No DJ controller"
                    color: midiController.connected ? "#3DA639" : deckPanel.palette.windowText
                    opacity: midiController.connected ? 0.95 : 0.55
                }

                Button {
                    text: midiController.enabled ? "On" : "Off"
                    font.pixelSize: 10
                    implicitWidth: 42
                    implicitHeight: 22
                    checkable: true
                    checked: midiController.enabled
                    onClicked: midiController.enabled = checked
                    ToolTip.visible: hovered
                    ToolTip.text: "Enable or disable the DJ controller"
                }

                Button {
                    text: "\u27F3"
                    font.pixelSize: 10
                    implicitWidth: 26
                    implicitHeight: 22
                    enabled: !midiController.connected
                    onClicked: midiController.rescan()
                    ToolTip.visible: hovered
                    ToolTip.text: "Look for the controller again"
                }
            }

            Label {
                text: mediaPlayer.errorString
                color: "#CC0000"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                visible: mediaPlayer.errorString.length > 0
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: deckPanel.palette.mid
        }

        // Volume
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "Volume"; font.pixelSize: 12; Layout.preferredWidth: 60 }
            Slider {
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                value: mediaPlayer.volume
                onMoved: mediaPlayer.setVolume(value)
            }
            Label {
                text: Math.round(mediaPlayer.volume) + "%"
                font.pixelSize: 11
                opacity: 0.7
                Layout.preferredWidth: 40
                horizontalAlignment: Text.AlignRight
            }
        }

        // Tempo
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "Tempo"; font.pixelSize: 12; Layout.preferredWidth: 60 }
            Slider {
                Layout.fillWidth: true
                from: 0.5
                to: 2.0
                stepSize: 0.05
                value: mediaPlayer.tempo
                onMoved: mediaPlayer.setTempo(value)
            }
            Label {
                text: Number(mediaPlayer.tempo).toFixed(2) + "x"
                font.pixelSize: 11
                opacity: 0.7
                Layout.preferredWidth: 40
                horizontalAlignment: Text.AlignRight
            }
        }

        // Pitch / key shift
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "Key Shift"; font.pixelSize: 12; Layout.preferredWidth: 60 }
            Slider {
                Layout.fillWidth: true
                from: -6
                to: 6
                stepSize: 1
                snapMode: Slider.SnapAlways
                value: mediaPlayer.pitch
                onMoved: mediaPlayer.setPitch(value)
            }
            Label {
                text: (mediaPlayer.pitch > 0 ? "+" : "") + mediaPlayer.pitch + " st"
                font.pixelSize: 11
                opacity: 0.7
                Layout.preferredWidth: 40
                horizontalAlignment: Text.AlignRight
            }
        }

        // Audio output device
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "Audio Out"; font.pixelSize: 12; Layout.preferredWidth: 60 }
            ComboBox {
                id: deviceCombo
                Layout.fillWidth: true
                model: mediaPlayer.audioDevices()
                currentIndex: mediaPlayer.audioDeviceIndex
                onActivated: mediaPlayer.setAudioDevice(index)
            }
        }

        Item { Layout.fillHeight: true }
    }

    Component.onCompleted: mediaPlayer.setVideoSink(miniVideo.videoSink)

    // Move the shared video sink to whichever output is currently visible.
    Connections {
        target: secondaryWindow
        function onVisibleChanged() {
            mediaPlayer.setVideoSink(secondaryWindow.visible ? secondaryVideo.videoSink : miniVideo.videoSink)
        }
    }

    Window {
        id: secondaryWindow
        title: "MaintecKJ - Display Output"
        width: 960
        height: 540
        visible: false
        color: "black"

        // Idle backdrop. It sits behind everything, so a CDG or a video still
        // wins whenever there is one to show.
        Image {
            id: secondaryBackground
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            source: displaySettings.backgroundImage
            visible: displaySettings.backgroundImage.length > 0
        }

        // Takes the shared video sink while this window is shown.
        VideoOutput {
            id: secondaryVideo
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
            visible: mediaPlayer.hasVideo
        }

        CdgRenderer {
            id: secondaryCdg
            anchors.fill: parent
            // The renderer paints solid black whatever its state, so it has to
            // stay out of the way unless there really is a CDG loaded - left
            // visible it would hide the background image completely.
            visible: !mediaPlayer.hasVideo && secondaryCdg.loaded
        }

        Label {
            anchors.centerIn: parent
            visible: !mediaPlayer.hasVideo && !secondaryCdg.loaded
                     && displaySettings.backgroundImage.length === 0
            text: "No Graphics Loaded"
            color: "white"
            opacity: 0.7
            font.pixelSize: 24
        }

        Label {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 12
            text: "Press Esc to close"
            color: "white"
            opacity: 0.5
            font.pixelSize: 12
        }

        Shortcut {
            sequence: "Escape"
            onActivated: secondaryWindow.hide()
        }
    }
}
