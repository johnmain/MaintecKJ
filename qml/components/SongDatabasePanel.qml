import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: databasePanel
    SplitView.minimumHeight: 200
    SplitView.preferredHeight: 350
    SplitView.fillHeight: true

    // Theme-aware row shading, so long lists are easier to follow.
    readonly property color stripeColor: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.05)
    readonly property color stripeHoverColor: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.12)

    // A null QString arrives in QML as undefined, so normalise before asking
    // either of these for a length.
    readonly property string exportError: songListExporter.lastError ? songListExporter.lastError : ""
    readonly property string exportSummary: songListExporter.lastSummary ? songListExporter.lastSummary : ""

    function formatDuration(seconds) {
        var value = Number(seconds)
        if (!value || value <= 0)
            return "--:--"
        var mins = Math.floor(value / 60)
        var secs = value % 60
        return mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    function selectedSong() {
        if (songList.currentIndex < 0)
            return null
        return songDatabaseModel.get(songList.currentIndex)
    }

    function selectedSongIsDeleted() {
        var song = selectedSong()
        return song ? song.isDeleted === true : false
    }

    function addRowToQueue(row) {
        if (row < 0)
            return
        var song = songDatabaseModel.get(row)
        if (!song)
            return
        if (!songQueueModel.selectedSingerName || songQueueModel.selectedSingerName.length === 0) {
            noSingerDialog.open()
            return
        }
        var singer = songQueueModel.selectedSingerName
        songQueueModel.addSong(singer, song.title, song.artist,
                               song.filePath, Number(song.duration), song.source || "")

        // A singer who had run out of songs is back in the rotation.
        var idx = singerModel.indexOfName(singer)
        if (idx >= 0 && singerModel.statusAt(idx) !== "Active")
            singerModel.setStatus(idx, "Active")
    }

    Dialog {
        id: deleteDialog
        title: "Delete Song"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string songTitle: ""
        property int songId: -1

        Label {
            text: "Move \"" + deleteDialog.songTitle + "\" to deleted songs?\n\nThe file stays on disk and will not be re-added on rescan. Tick \"Show deleted\" to restore it."
            wrapMode: Text.WordWrap
            width: 320
        }

        onAccepted: {
            if (deleteDialog.songId >= 0)
                songDatabaseModel.removeSong(deleteDialog.songId)
        }
    }

    Dialog {
        id: purgeDialog
        title: "Delete Permanently"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string songTitle: ""
        property int songId: -1

        Label {
            text: "Permanently remove \"" + purgeDialog.songTitle + "\" from the database?\n\nThis cannot be undone. The file on disk is not affected."
            wrapMode: Text.WordWrap
            width: 320
        }

        onAccepted: {
            if (purgeDialog.songId >= 0)
                songDatabaseModel.purgeSong(purgeDialog.songId)
        }
    }

    Dialog {
        id: noSingerDialog
        title: "No Singer Selected"
        modal: true
        standardButtons: Dialog.Ok

        Label {
            text: "Select a singer in the Singer Rotation panel first, then add the song to their queue."
            wrapMode: Text.WordWrap
            width: 320
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Select Music Folder"
        onAccepted: {
            var path = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            folderField.text = path
            databaseManager.addDirectory(path)
            songDatabaseModel.refreshData()
        }
    }

    FileDialog {
        id: songListExportDialog
        title: "Export Song List"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        onAccepted: songListExporter.exportToFile(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: searchField
                placeholderText: "Search songs by artist or title..."
                Layout.fillWidth: true
                onTextChanged: songDatabaseModel.setFilter(text)
            }

            Button {
                text: "Refresh"
                onClicked: songDatabaseModel.refreshData()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: folderField
                placeholderText: "Music folder path (e.g. /home/user/Karaoke)..."
                Layout.fillWidth: true
            }

            Button {
                text: "Add Folder…"
                onClicked: folderDialog.open()
            }

            Button {
                text: "Rescan"
                enabled: folderField.text.length > 0
                onClicked: {
                    databaseManager.rescanDirectory(folderField.text)
                    songDatabaseModel.refreshData()
                }
            }

            Button {
                text: "Remove"
                enabled: folderField.text.length > 0
                onClicked: {
                    databaseManager.removeDirectory(folderField.text)
                    songDatabaseModel.refreshData()
                }
            }
        }

        // Library-wide actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // Plain Artist/Title pairs, deduplicated, for the song-book workflow.
            Button {
                text: "Export Song List…"
                onClicked: songListExportDialog.open()
            }

            Label {
                text: databasePanel.exportError.length > 0
                      ? databasePanel.exportError
                      : databasePanel.exportSummary
                visible: text.length > 0
                color: databasePanel.exportError.length > 0 ? "#CC0000" : "#00AA00"
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // Selected-song actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: "Add to Queue"
                enabled: songList.currentIndex >= 0
                onClicked: addRowToQueue(songList.currentIndex)
            }

            Button {
                text: selectedSongIsDeleted() ? "Delete Permanently" : "Delete"
                enabled: songList.currentIndex >= 0
                onClicked: {
                    var song = selectedSong()
                    if (!song)
                        return
                    if (song.isDeleted) {
                        purgeDialog.songTitle = song.title || song.artist || "song"
                        purgeDialog.songId = song.id
                        purgeDialog.open()
                    } else {
                        deleteDialog.songTitle = song.title || song.artist || "song"
                        deleteDialog.songId = song.id
                        deleteDialog.open()
                    }
                }
            }

            Button {
                text: "Undelete"
                enabled: songList.currentIndex >= 0 && selectedSongIsDeleted()
                onClicked: {
                    var song = selectedSong()
                    if (song)
                        songDatabaseModel.restoreSong(song.id)
                }
            }

            CheckBox {
                id: showDeletedBox
                text: "Show deleted"
                onToggled: songDatabaseModel.setIncludeDeleted(checked)
            }

            Label {
                text: songQueueModel.selectedSingerName.length > 0
                      ? "\u2192 " + songQueueModel.selectedSingerName
                      : "(no singer selected)"
                color: songQueueModel.selectedSingerName.length > 0 ? "#2E9E2E" : "#D15C5C"
                font.pixelSize: 11
                Layout.leftMargin: 8
            }

            Item { Layout.fillWidth: true }
        }

        // Column headers (Artist/Title are clickable to sort)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                flat: true
                padding: 0
                Layout.fillWidth: true
                Layout.preferredWidth: 160
                Layout.horizontalStretchFactor: 1
                onClicked: songDatabaseModel.toggleSort(1)
                contentItem: Label {
                    text: "Artist" + (songDatabaseModel.sortColumn === 1 ? (songDatabaseModel.sortAscending ? "  \u25B2" : "  \u25BC") : "")
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Button {
                flat: true
                padding: 0
                Layout.fillWidth: true
                Layout.preferredWidth: 160
                Layout.horizontalStretchFactor: 1
                onClicked: songDatabaseModel.toggleSort(2)
                contentItem: Label {
                    text: "Title" + (songDatabaseModel.sortColumn === 2 ? (songDatabaseModel.sortAscending ? "  \u25B2" : "  \u25BC") : "")
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Button {
                flat: true
                padding: 0
                Layout.preferredWidth: 130
                onClicked: songDatabaseModel.toggleSort(3)
                contentItem: Label {
                    text: "Source" + (songDatabaseModel.sortColumn === 3 ? (songDatabaseModel.sortAscending ? "  \u25B2" : "  \u25BC") : "")
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Label {
                text: "Duration"
                font.bold: true
                Layout.preferredWidth: 70
                horizontalAlignment: Text.AlignRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#CCCCCC"
        }

        ListView {
            id: songList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: songDatabaseModel

            delegate: ItemDelegate {
                id: songDelegate
                width: songList.width

                background: Rectangle {
                    color: ListView.isCurrentItem ? "#DCEBFA"
                           : (songDelegate.hovered ? databasePanel.stripeHoverColor
                              : (index % 2 === 1 ? databasePanel.stripeColor : "transparent"))
                    radius: 3
                }

                contentItem: RowLayout {
                    spacing: 8

                    Label {
                        text: model.artist || "Unknown Artist"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.horizontalStretchFactor: 1
                        elide: Text.ElideRight
                        opacity: model.isDeleted ? 0.45 : 1.0
                        font.strikeout: model.isDeleted
                    }
                    Label {
                        text: model.title || "Unknown Title"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.horizontalStretchFactor: 1
                        elide: Text.ElideRight
                        opacity: model.isDeleted ? 0.45 : 1.0
                        font.strikeout: model.isDeleted
                    }
                    Label {
                        text: model.source || ""
                        Layout.preferredWidth: 130
                        elide: Text.ElideRight
                        opacity: model.isDeleted ? 0.45 : 0.75
                        font.strikeout: model.isDeleted
                    }
                    Label {
                        text: model.isDeleted ? "deleted" : databasePanel.formatDuration(model.duration)
                        Layout.preferredWidth: 70
                        horizontalAlignment: Text.AlignRight
                        opacity: 0.7
                    }
                }

                onClicked: songList.currentIndex = index
                onDoubleClicked: addRowToQueue(index)
            }

            ScrollBar.vertical: ScrollBar { }
        }

        Label {
            text: songList.count + " song(s) in database"
            font.pixelSize: 11
            opacity: 0.7
            Layout.fillWidth: true
        }
    }
}
