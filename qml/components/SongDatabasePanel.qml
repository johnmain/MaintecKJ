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

    // Reported by the background index run when it reports back.
    property string scanMessage: ""

    // Indexing runs off the UI thread now, so rows appear when the run reports
    // back rather than the instant the folder is added.
    Connections {
        target: databaseManager
        function onLibraryScanFinished(added, unreadable) {
            songDatabaseModel.refreshData()
            databasePanel.scanMessage = unreadable > 0
                ? ("Indexed " + added + " songs; " + unreadable + " files could not be read.")
                : ("Indexed " + added + " songs.")
        }
    }

    function formatDuration(seconds) {
        var value = Number(seconds)
        if (!value || value <= 0)
            return "--:--"
        var mins = Math.floor(value / 60)
        var secs = value % 60
        return mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    // Rows the actions act on: the multi-selection, else the current row.
    function targetRows() {
        var rows = songList.selectedRows()
        if (rows.length === 0 && songList.currentIndex >= 0)
            rows = [songList.currentIndex]
        return rows
    }

    function liveSelectionIds() {
        var ids = []
        var rows = targetRows()
        for (var i = 0; i < rows.length; ++i) {
            var song = songDatabaseModel.get(rows[i])
            if (song && song.isDeleted !== true)
                ids.push(song.id)
        }
        return ids
    }

    function deletedSelectionIds() {
        var ids = []
        var rows = targetRows()
        for (var i = 0; i < rows.length; ++i) {
            var song = songDatabaseModel.get(rows[i])
            if (song && song.isDeleted === true)
                ids.push(song.id)
        }
        return ids
    }

    function addRowsToQueue(rows) {
        if (!rows || rows.length === 0)
            return
        if (!songQueueModel.selectedSingerName || songQueueModel.selectedSingerName.length === 0) {
            noSingerDialog.open()
            return
        }
        var singer = songQueueModel.selectedSingerName
        for (var i = 0; i < rows.length; ++i) {
            var song = songDatabaseModel.get(rows[i])
            if (!song)
                continue
            songQueueModel.addSong(singer, song.title, song.artist,
                                   song.filePath, Number(song.duration), song.source || "")
        }

        // A singer who had run out of songs is back in the rotation.
        var idx = singerModel.indexOfName(singer)
        if (idx >= 0 && singerModel.statusAt(idx) !== "Active")
            singerModel.setStatus(idx, "Active")
    }

    function addRowToQueue(row) {
        if (row >= 0)
            addRowsToQueue([row])
    }

    Dialog {
        id: deleteDialog
        title: "Delete Song"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        property var songIds: []
        property string songTitle: ""

        Label {
            text: deleteDialog.songIds.length === 1
                  ? "Move \"" + deleteDialog.songTitle + "\" to deleted songs?\n\nThe file stays on disk and will not be re-added on rescan. Tick \"Show deleted\" to restore it."
                  : "Move " + deleteDialog.songIds.length + " songs to deleted songs?\n\nThe files stay on disk and will not be re-added on rescan. Tick \"Show deleted\" to restore them."
            wrapMode: Text.WordWrap
            width: 320
        }

        onAccepted: {
            for (var i = 0; i < deleteDialog.songIds.length; ++i)
                songDatabaseModel.removeSong(deleteDialog.songIds[i])
        }
    }

    Dialog {
        id: purgeDialog
        title: "Delete Permanently"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        property var songIds: []
        property string songTitle: ""

        Label {
            text: purgeDialog.songIds.length === 1
                  ? "Permanently remove \"" + purgeDialog.songTitle + "\" from the database?\n\nThis cannot be undone. The file on disk is not affected."
                  : "Permanently remove " + purgeDialog.songIds.length + " songs from the database?\n\nThis cannot be undone. The files on disk are not affected."
            wrapMode: Text.WordWrap
            width: 320
        }

        onAccepted: {
            for (var i = 0; i < purgeDialog.songIds.length; ++i)
                songDatabaseModel.purgeSong(purgeDialog.songIds[i])
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
                enabled: !databaseManager.scanning
                onClicked: folderDialog.open()
            }

            Button {
                text: "Rescan"
                enabled: folderField.text.length > 0 && !databaseManager.scanning
                onClicked: databaseManager.rescanDirectory(folderField.text)
            }

            Button {
                text: "Remove"
                enabled: folderField.text.length > 0 && !databaseManager.scanning
                onClicked: {
                    databaseManager.removeDirectory(folderField.text)
                    songDatabaseModel.refreshData()
                }
            }
        }

        Label {
            visible: databaseManager.scanning || databasePanel.scanMessage.length > 0
            text: databaseManager.scanning
                  ? ("Measuring " + databaseManager.scanProgress + " of "
                     + databaseManager.scanTotal + " files…")
                  : databasePanel.scanMessage
            font.pixelSize: 10
            color: databaseManager.scanning ? "#4A90E2" : palette.windowText
            opacity: databaseManager.scanning ? 1.0 : 0.7
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
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
                text: databasePanel.targetRows().length > 1
                      ? ("Add " + databasePanel.targetRows().length + " to Queue")
                      : "Add to Queue"
                enabled: databasePanel.targetRows().length > 0
                onClicked: addRowsToQueue(databasePanel.targetRows())
            }

            Button {
                // A selection of nothing but deleted songs is a permanent purge;
                // otherwise the live ones are moved to the deleted list.
                text: (databasePanel.liveSelectionIds().length === 0
                       && databasePanel.deletedSelectionIds().length > 0)
                      ? "Delete Permanently" : "Delete"
                enabled: databasePanel.targetRows().length > 0
                onClicked: {
                    var live = databasePanel.liveSelectionIds()
                    var rows = databasePanel.targetRows()
                    var single = rows.length === 1 ? songDatabaseModel.get(rows[0]) : null
                    if (live.length > 0) {
                        deleteDialog.songIds = live
                        deleteDialog.songTitle = single ? (single.title || single.artist || "song") : "song"
                        deleteDialog.open()
                    } else {
                        purgeDialog.songIds = databasePanel.deletedSelectionIds()
                        purgeDialog.songTitle = single ? (single.title || single.artist || "song") : "song"
                        purgeDialog.open()
                    }
                }
            }

            Button {
                text: "Undelete"
                enabled: databasePanel.deletedSelectionIds().length > 0
                onClicked: {
                    var ids = databasePanel.deletedSelectionIds()
                    for (var i = 0; i < ids.length; ++i)
                        songDatabaseModel.restoreSong(ids[i])
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

            // Multi-selection (index -> true), same shape as the singer queue.
            property var selectedIndices: ({})
            property int anchorIndex: -1

            function isSelected(i) {
                return selectedIndices[i] === true
            }

            function selectOnly(i) {
                var m = ({})
                m[i] = true
                selectedIndices = m
            }

            function toggleIndex(i) {
                var m = ({})
                for (var k in selectedIndices)
                    m[k] = selectedIndices[k]
                if (m[i])
                    delete m[i]
                else
                    m[i] = true
                selectedIndices = m
                anchorIndex = i
            }

            function selectRange(from, to) {
                var m = ({})
                var a = Math.min(from, to)
                var b = Math.max(from, to)
                for (var i = a; i <= b; ++i)
                    m[i] = true
                selectedIndices = m
            }

            function selectAll() {
                var m = ({})
                for (var i = 0; i < songList.count; ++i)
                    m[i] = true
                selectedIndices = m
                anchorIndex = songList.count > 0 ? 0 : -1
            }

            function selectedRows() {
                var rows = []
                for (var k in selectedIndices)
                    if (selectedIndices[k])
                        rows.push(Number(k))
                rows.sort(function(a, b) { return a - b })
                return rows
            }

            function clearSelection() {
                selectedIndices = ({})
            }

            // Ctrl+A selects every listed song for bulk queueing/deletion. Scoped
            // to the list's focus so it never steals Ctrl+A from the search field.
            Shortcut {
                sequence: StandardKey.SelectAll
                enabled: songList.activeFocus
                onActivated: songList.selectAll()
            }

            delegate: ItemDelegate {
                id: songDelegate
                width: songList.width

                background: Rectangle {
                    color: songList.isSelected(index)
                           ? Qt.rgba(databasePanel.palette.highlight.r,
                                     databasePanel.palette.highlight.g,
                                     databasePanel.palette.highlight.b, 0.45)
                           : (songList.currentIndex === index ? "#DCEBFA"
                              : (songMouse.containsMouse ? databasePanel.stripeHoverColor
                                 : (index % 2 === 1 ? databasePanel.stripeColor : "transparent")))
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

                MouseArea {
                    id: songMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton

                    onClicked: function(mouse) {
                        // The list takes focus so Ctrl+A targets it, not the search box.
                        songList.forceActiveFocus()
                        if (mouse.modifiers & Qt.ControlModifier) {
                            songList.toggleIndex(index)
                        } else if ((mouse.modifiers & Qt.ShiftModifier) && songList.anchorIndex >= 0) {
                            songList.selectRange(songList.anchorIndex, index)
                        } else {
                            songList.selectOnly(index)
                            songList.anchorIndex = index
                            songList.currentIndex = index
                        }
                    }

                    onDoubleClicked: addRowToQueue(index)
                }
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
