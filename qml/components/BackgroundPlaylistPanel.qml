import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The background music playlist. It has no singer column and takes no part in
// the rotation - the deck plays straight down it, and reaching the end stops
// rather than looping, so the operator always chooses what plays next.
Pane {
    id: bgPlaylistPanel
    SplitView.minimumHeight: 150

    readonly property color stripeColor: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.05)
    readonly property color stripeHoverColor: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.12)

    function formatDuration(seconds) {
        var value = Number(seconds)
        if (!value || value <= 0)
            return "--:--"
        var mins = Math.floor(value / 60)
        var secs = value % 60
        return mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    // Accepts a song dragged out of the background library.
    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (!drop.source || drop.source.dragFilePath === undefined)
                return
            backgroundPlaylistModel.addSong(drop.source.dragArtist,
                                            drop.source.dragTitle,
                                            drop.source.dragFilePath,
                                            drop.source.dragDuration,
                                            drop.source.dragSource || "")
            drop.accept()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Background Playlist"
                font.bold: true
                font.pixelSize: 16
                Layout.fillWidth: true
            }

            // One pass per press. The list is never reshuffled on its own, so the
            // running order cannot change under the operator between songs.
            Button {
                text: "\uD83D\uDD00 Random"
                enabled: backgroundPlaylistModel.count > 1
                onClicked: backgroundPlaylistModel.shuffle()
            }

            Button {
                text: "Remove"
                enabled: playlistList.currentIndex >= 0
                onClicked: backgroundPlaylistModel.removeSong(playlistList.currentIndex)
            }

            Button {
                text: "Clear"
                enabled: backgroundPlaylistModel.count > 0
                onClicked: backgroundPlaylistModel.clearPlaylist()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Artist"
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 160
                Layout.horizontalStretchFactor: 1
            }
            Label {
                text: "Title"
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 160
                Layout.horizontalStretchFactor: 1
            }
            Label {
                text: "Source"
                font.bold: true
                Layout.preferredWidth: 110
            }
            Label {
                text: "Time"
                font.bold: true
                Layout.preferredWidth: 60
                horizontalAlignment: Text.AlignRight
            }
        }

        ListView {
            id: playlistList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backgroundPlaylistModel

            delegate: ItemDelegate {
                id: playlistDelegate
                width: playlistList.width
                height: 32

                readonly property bool isCurrent: index === backgroundPlaylistModel.currentIndex

                contentItem: RowLayout {
                    spacing: 8

                    Label {
                        visible: playlistDelegate.isCurrent
                        text: "\u25B6"
                        color: "#4A90E2"
                        font.pixelSize: 12
                    }
                    Label {
                        text: model.artist
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.horizontalStretchFactor: 1
                        elide: Text.ElideRight
                        font.bold: playlistDelegate.isCurrent
                    }
                    Label {
                        text: model.title
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.horizontalStretchFactor: 1
                        elide: Text.ElideRight
                        font.bold: playlistDelegate.isCurrent
                    }
                    Label {
                        text: model.source ? model.source : ""
                        opacity: 0.6
                        Layout.preferredWidth: 110
                        elide: Text.ElideRight
                    }
                    Label {
                        text: bgPlaylistPanel.formatDuration(model.duration)
                        opacity: 0.7
                        Layout.preferredWidth: 60
                        horizontalAlignment: Text.AlignRight
                    }
                }

                background: Rectangle {
                    color: playlistDelegate.isCurrent
                           ? Qt.rgba(bgPlaylistPanel.palette.highlight.r,
                                     bgPlaylistPanel.palette.highlight.g,
                                     bgPlaylistPanel.palette.highlight.b, 0.25)
                           : (playlistHover.containsMouse
                              ? bgPlaylistPanel.stripeHoverColor
                              : (index % 2 === 1 ? bgPlaylistPanel.stripeColor : "transparent"))
                    radius: 3
                    border.color: playlistDelegate.isCurrent ? "#4A90E2" : "transparent"
                    border.width: playlistDelegate.isCurrent ? 1 : 0
                }

                MouseArea {
                    id: playlistHover
                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: playlistList.currentIndex = index
                    onDoubleClicked: modeController.playBackgroundRow(index)
                }
            }

            ScrollBar.vertical: ScrollBar { }
        }

        Label {
            text: backgroundPlaylistModel.count === 0
                  ? "The playlist is empty. Add songs from the background library above."
                  : (backgroundPlaylistModel.count + " songs. Double-click one to play it; "
                     + "the next entry plays automatically when a song ends.")
            font.pixelSize: 11
            opacity: 0.7
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
