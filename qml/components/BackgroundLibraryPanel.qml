import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window

// The background music library. Same shape as the karaoke library panel, but
// pointed at the background collection: no delete/undelete, and every action
// feeds the background playlist rather than a singer's queue.
Pane {
    id: bgLibraryPanel
    SplitView.minimumHeight: 200
    SplitView.preferredHeight: 350
    SplitView.fillHeight: true

    // Theme-aware row shading, so long lists are easier to follow.
    readonly property color stripeColor: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.05)
    readonly property color stripeHoverColor: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.12)

    // Set by the actions below. The label showing it prefers the live scan
    // progress while an index run is in flight.
    property string statusText: "Double-click a song to add it to the playlist, or drag it down onto it."

    // The dragged row is mirrored at window level. Inside the list it would be
    // clipped, and the panel below is a later sibling so anything drawn by this
    // panel ends up underneath it - the overlay is the only place the row can be
    // seen all the way down to the playlist.
    function showDragOverlay(item, artist, title) {
        var overlay = Window.window ? Window.window.dragOverlayItem : null
        if (!overlay)
            return
        var origin = item.mapToItem(null, 0, 0)
        overlay.overlayText = artist + " \u2014 " + title
        overlay.width = Math.min(Math.max(240, item.width), 480)
        overlay.x = origin.x
        overlay.y = origin.y
        overlay.visible = true
    }

    function hideDragOverlay() {
        var overlay = Window.window ? Window.window.dragOverlayItem : null
        if (overlay)
            overlay.visible = false
    }

    // Indexing runs off the UI thread, so rows only appear once it reports back.
    Connections {
        target: databaseManager
        function onBackgroundScanFinished(added, unreadable) {
            backgroundSongModel.refreshData()
            bgLibraryPanel.statusText = unreadable > 0
                ? ("Indexed " + added + " songs; " + unreadable + " files could not be read.")
                : ("Indexed " + added + " songs.")
        }
    }

    // Any refresh (filter, rescan) resets the rows, so an index-keyed selection
    // would otherwise point at the wrong songs afterwards.
    Connections {
        target: backgroundSongModel
        function onModelReset() {
            bgSongList.clearSelection()
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
        var rows = bgSongList.selectedRows()
        if (rows.length === 0 && bgSongList.currentIndex >= 0)
            rows = [bgSongList.currentIndex]
        return rows
    }

    function addRowsToPlaylist(rows) {
        if (!rows || rows.length === 0)
            return
        var added = 0
        for (var i = 0; i < rows.length; ++i) {
            var song = backgroundSongModel.get(rows[i])
            if (!song)
                continue
            backgroundPlaylistModel.addSong(song.artist, song.title,
                                             song.filePath, Number(song.duration),
                                             song.source || "")
            ++added
        }
        bgLibraryPanel.statusText = added === 0
            ? "Nothing to add."
            : (added === 1
               ? "Added 1 song to the playlist."
               : ("Added " + added + " songs to the playlist."))
    }

    FolderDialog {
        id: bgFolderDialog
        title: "Select Background Music Folder"
        onAccepted: {
            var path = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            folderField.text = path
            // Rows arrive when the background scan reports back.
            databaseManager.addBackgroundDirectory(path)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: "Background Music Library"
            font.bold: true
            font.pixelSize: 16
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: searchField
                placeholderText: "Search artists, titles..."
                Layout.fillWidth: true
                onTextChanged: backgroundSongModel.setFilter(text)
            }

            // Adds the multi-selection (or the current row) to the playlist.
            Button {
                text: bgLibraryPanel.targetRows().length > 1
                      ? ("Add " + bgLibraryPanel.targetRows().length + " to Playlist")
                      : "Add to Playlist"
                enabled: bgLibraryPanel.targetRows().length > 0
                onClicked: bgLibraryPanel.addRowsToPlaylist(bgLibraryPanel.targetRows())
            }

            // Everything in the background library, once. Pressing it again only
            // picks up songs added since - already-listed tracks are skipped.
            Button {
                text: "Add All to Playlist"
                onClicked: {
                    var added = backgroundPlaylistModel.addAllFromLibrary()
                    bgLibraryPanel.statusText = added > 0
                        ? ("Added " + added + " songs to the playlist.")
                        : "Every background song is already on the playlist."
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: folderField
                placeholderText: "Background music folder path..."
                Layout.fillWidth: true
            }

            Button {
                text: "Add Folder…"
                enabled: !databaseManager.scanning
                onClicked: bgFolderDialog.open()
            }

            Button {
                text: "Rescan"
                enabled: folderField.text.length > 0 && !databaseManager.scanning
                onClicked: databaseManager.rescanBackgroundDirectory(folderField.text)
            }

            Button {
                text: "Remove"
                enabled: folderField.text.length > 0 && !databaseManager.scanning
                onClicked: {
                    databaseManager.removeBackgroundDirectory(folderField.text)
                    backgroundSongModel.refreshData()
                }
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
            id: bgSongList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backgroundSongModel

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
                for (var i = 0; i < bgSongList.count; ++i)
                    m[i] = true
                selectedIndices = m
                anchorIndex = bgSongList.count > 0 ? 0 : -1
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

            // Ctrl+A selects every listed song for bulk playlist additions. Scoped
            // to the list's focus so it never steals Ctrl+A from the search field.
            Shortcut {
                sequence: StandardKey.SelectAll
                enabled: bgSongList.activeFocus
                onActivated: bgSongList.selectAll()
            }

            delegate: ItemDelegate {
                id: bgDelegate
                width: bgSongList.width
                height: 32

                // The row hides while it is dragged; the window-level overlay is
                // what the cursor carries instead.
                opacity: bgDragArea.drag.active ? 0 : 1

                // Read straight off the dragged item by the playlist's DropArea,
                // which avoids hand-encoding a payload into mime data.
                property string dragArtist: model.artist
                property string dragTitle: model.title
                property string dragFilePath: model.filePath
                property int dragDuration: model.duration
                property string dragSource: model.source
                property real startX: 0
                property real startY: 0

                Drag.active: bgDragArea.drag.active
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2
                Drag.source: bgDelegate

                contentItem: RowLayout {
                    spacing: 8

                    Label {
                        text: model.artist
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.horizontalStretchFactor: 1
                        elide: Text.ElideRight
                    }
                    Label {
                        text: model.title
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.horizontalStretchFactor: 1
                        elide: Text.ElideRight
                    }
                    Label {
                        text: model.source ? model.source : ""
                        opacity: 0.6
                        Layout.preferredWidth: 110
                        elide: Text.ElideRight
                    }
                    Label {
                        text: bgLibraryPanel.formatDuration(model.duration)
                        opacity: 0.7
                        Layout.preferredWidth: 60
                        horizontalAlignment: Text.AlignRight
                    }
                }

                background: Rectangle {
                    color: bgSongList.isSelected(index)
                           ? Qt.rgba(bgLibraryPanel.palette.highlight.r,
                                     bgLibraryPanel.palette.highlight.g,
                                     bgLibraryPanel.palette.highlight.b, 0.45)
                           : (bgDragArea.containsMouse
                              ? bgLibraryPanel.stripeHoverColor
                              : (index % 2 === 1 ? bgLibraryPanel.stripeColor : "transparent"))
                    radius: 3
                }

                MouseArea {
                    id: bgDragArea
                    anchors.fill: parent
                    hoverEnabled: true
                    drag.target: bgDelegate
                    drag.axis: Drag.YAxis
                    drag.threshold: 8

                    onClicked: function(mouse) {
                        // The list takes focus so Ctrl+A targets it, not the search box.
                        bgSongList.forceActiveFocus()
                        if (mouse.modifiers & Qt.ControlModifier) {
                            bgSongList.toggleIndex(index)
                        } else if ((mouse.modifiers & Qt.ShiftModifier) && bgSongList.anchorIndex >= 0) {
                            bgSongList.selectRange(bgSongList.anchorIndex, index)
                        } else {
                            bgSongList.selectOnly(index)
                            bgSongList.anchorIndex = index
                            bgSongList.currentIndex = index
                        }
                    }

                    onPressed: {
                        bgDelegate.startX = bgDelegate.x
                        bgDelegate.startY = bgDelegate.y
                    }

                    onPositionChanged: {
                        if (drag.active)
                            bgLibraryPanel.showDragOverlay(bgDelegate, model.artist, model.title)
                    }

                    onReleased: {
                        if (bgDelegate.Drag.active)
                            bgDelegate.Drag.drop()
                        // Snap back to the slot the ListView assigned.
                        bgDelegate.x = bgDelegate.startX
                        bgDelegate.y = bgDelegate.startY
                        bgLibraryPanel.hideDragOverlay()
                    }

                    onCanceled: bgLibraryPanel.hideDragOverlay()

                    onDoubleClicked: {
                        backgroundPlaylistModel.addSong(model.artist, model.title,
                                                        model.filePath, model.duration,
                                                        model.source || "")
                        bgLibraryPanel.statusText = "Added \"" + model.title + "\" to the playlist."
                    }
                }
            }

            ScrollBar.vertical: ScrollBar { }
        }

        Label {
            text: databaseManager.scanning
                  ? ("Measuring " + databaseManager.scanProgress + " of "
                     + databaseManager.scanTotal + " files…")
                  : bgLibraryPanel.statusText
            font.pixelSize: 10
            color: databaseManager.scanning ? "#4A90E2" : palette.windowText
            opacity: databaseManager.scanning ? 1.0 : 0.7
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: "Background songs indexed: " + databaseManager.backgroundSongCount
            font.pixelSize: 12
            opacity: 0.7
            Layout.fillWidth: true
        }
    }
}
