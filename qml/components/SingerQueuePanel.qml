import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: queuePanel
    SplitView.minimumHeight: 150
    SplitView.preferredHeight: 250

    // Theme-aware row shading, so long lists are easier to follow.
    readonly property color stripeColor: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.05)

    function formatDuration(seconds) {
        var value = Number(seconds)
        if (!value || value <= 0)
            return "--:--"
        var mins = Math.floor(value / 60)
        var secs = value % 60
        return mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    function sortMark(col) {
        return songQueueModel.sortColumn === col
               ? (songQueueModel.sortAscending ? "  \u25B2" : "  \u25BC")
               : ""
    }

    function formatKey(k) {
        var v = Number(k) || 0
        return v > 0 ? "+" + v : "" + v
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: rotationController.currentSinger.length > 0
                      ? "Queue \u2014 " + rotationController.currentSinger
                      : "Current Singer Queue (select a singer)"
                font.bold: true
                Layout.fillWidth: true
            }
            Label {
                text: queueList.count + " item(s)"
                font.pixelSize: 11
                opacity: 0.7
            }
        }

        // Column headers (click to sort the queue)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                flat: true
                padding: 0
                Layout.preferredWidth: 90
                onClicked: songQueueModel.toggleSort(1)
                contentItem: Label {
                    text: "Singer" + queuePanel.sortMark(1)
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            // Artist before Title, matching the song list column order.
            Button {
                flat: true
                padding: 0
                Layout.fillWidth: true
                Layout.preferredWidth: 160
                Layout.horizontalStretchFactor: 1
                onClicked: songQueueModel.toggleSort(3)
                contentItem: Label {
                    text: "Artist" + queuePanel.sortMark(3)
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
                onClicked: songQueueModel.toggleSort(2)
                contentItem: Label {
                    text: "Title" + queuePanel.sortMark(2)
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Button {
                flat: true
                padding: 0
                Layout.preferredWidth: 110
                onClicked: songQueueModel.toggleSort(4)
                contentItem: Label {
                    text: "Source" + queuePanel.sortMark(4)
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Label {
                text: "Key"
                font.bold: true
                Layout.preferredWidth: 50
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            Button {
                flat: true
                padding: 0
                Layout.preferredWidth: 60
                onClicked: songQueueModel.toggleSort(5)
                contentItem: Label {
                    text: "Time" + queuePanel.sortMark(5)
                    font.bold: true
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        ListView {
            id: queueList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: songQueueModel

            // Multi-selection state (index -> true)
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

            function selectedRows() {
                var rows = []
                for (var k in selectedIndices)
                    if (selectedIndices[k])
                        rows.push(Number(k))
                rows.sort(function(a, b) { return a - b })
                return rows
            }

            function markSelection(played) {
                var rows = selectedRows()
                for (var i = 0; i < rows.length; ++i)
                    songQueueModel.markAsPlayed(rows[i], played)
            }

            function removeSelection() {
                var rows = selectedRows()
                rows.sort(function(a, b) { return b - a })
                for (var i = 0; i < rows.length; ++i)
                    songQueueModel.removeSong(rows[i])
                selectedIndices = ({})
            }

            // Rows the Key controls act on: the selection, else the current row.
            function keyRows() {
                var rows = selectedRows()
                if (rows.length === 0 && currentIndex >= 0)
                    rows = [currentIndex]
                return rows
            }

            // Editing a queue entry only changes what is stored against it. The
            // deck is transposed only when the edited entry is the very song
            // loaded in it, so touching one singer's queue never shifts the
            // song that someone else is singing right now.
            function syncDeckKey(rows) {
                if (rows.length !== 1)
                    return
                if (!mediaPlayer.filePath || mediaPlayer.filePath.length === 0)
                    return
                if (songQueueModel.filePathAt(rows[0]) !== mediaPlayer.filePath)
                    return
                mediaPlayer.setPitch(songQueueModel.keyShiftAt(rows[0]))
            }

            function adjustKey(delta) {
                var rows = keyRows()
                for (var i = 0; i < rows.length; ++i)
                    songQueueModel.setKeyShift(rows[i], songQueueModel.keyShiftAt(rows[i]) + delta)
                syncDeckKey(rows)
            }

            function resetKey() {
                var rows = keyRows()
                for (var i = 0; i < rows.length; ++i)
                    songQueueModel.setKeyShift(rows[i], 0)
                syncDeckKey(rows)
            }

            delegate: ItemDelegate {
                id: queueDelegate
                width: queueList.width

                contentItem: RowLayout {
                    spacing: 8

                    Label {
                        text: model.singerName
                        Layout.preferredWidth: 90
                        elide: Text.ElideRight
                        opacity: model.isPlayed ? 0.45 : 1.0
                        font.strikeout: model.isPlayed
                    }

                    Label {
                        text: model.artist || ""
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.horizontalStretchFactor: 1
                        elide: Text.ElideRight
                        opacity: model.isPlayed ? 0.45 : 1.0
                        font.strikeout: model.isPlayed
                    }

                    Label {
                        text: model.songTitle || "Unknown Title"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.horizontalStretchFactor: 1
                        elide: Text.ElideRight
                        opacity: model.isPlayed ? 0.45 : 1.0
                        font.strikeout: model.isPlayed
                    }

                    Label {
                        text: model.source || ""
                        Layout.preferredWidth: 110
                        elide: Text.ElideRight
                        opacity: model.isPlayed ? 0.45 : 0.75
                        font.strikeout: model.isPlayed
                    }

                    Label {
                        text: queuePanel.formatKey(model.keyShift)
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.85
                        font.strikeout: model.isPlayed
                    }

                    Label {
                        text: queuePanel.formatDuration(model.duration)
                        Layout.preferredWidth: 60
                        horizontalAlignment: Text.AlignRight
                        opacity: 0.7
                    }
                }

                background: Rectangle {
                    color: queueList.isSelected(index)
                           ? Qt.rgba(queuePanel.palette.highlight.r, queuePanel.palette.highlight.g, queuePanel.palette.highlight.b, 0.45)
                           : (delegateMouse.containsMouse
                              ? Qt.rgba(queuePanel.palette.windowText.r, queuePanel.palette.windowText.g, queuePanel.palette.windowText.b, 0.12)
                              : (index % 2 === 1 ? queuePanel.stripeColor : "transparent"))
                    radius: 3
                }

                MouseArea {
                    id: delegateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            if (!queueList.isSelected(index))
                                queueList.selectOnly(index)
                            contextMenu.popup()
                        } else if (mouse.modifiers & Qt.ControlModifier) {
                            queueList.toggleIndex(index)
                        } else if ((mouse.modifiers & Qt.ShiftModifier) && queueList.anchorIndex >= 0) {
                            queueList.selectRange(queueList.anchorIndex, index)
                        } else {
                            queueList.selectOnly(index)
                            queueList.anchorIndex = index
                        }
                    }

                    onDoubleClicked: {
                        if (!model.filePath || model.filePath.length === 0)
                            return

                        mediaPlayer.load(model.filePath)
                        // Apply this queue item's saved key shift before it plays.
                        mediaPlayer.setPitch(model.keyShift)
                        // Double-clicking a specific entry is an explicit "play
                        // this now", unlike the rotation, which only ever selects
                        // the next singer and deliberately starts nothing.
                        mediaPlayer.play()
                        songQueueModel.markAsPlayed(index, true)
                    }
                }
            }

            ScrollBar.vertical: ScrollBar { }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: queueList.selectedRows().length > 0
                      ? queueList.selectedRows().length + " selected"
                      : "Ctrl/Shift-click to multi-select \u00B7 right-click for menu"
                font.pixelSize: 10
                opacity: 0.7
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Label {
                text: "Key:"
                font.pixelSize: 11
                opacity: 0.8
            }

            Button {
                text: "\u2212"
                implicitWidth: 34
                enabled: queueList.keyRows().length > 0
                onClicked: queueList.adjustKey(-1)
            }

            Label {
                text: queueList.keyRows().length > 0
                      ? queuePanel.formatKey(songQueueModel.keyShiftAt(queueList.keyRows()[0]))
                      : "0"
                Layout.preferredWidth: 28
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                text: "+"
                implicitWidth: 34
                enabled: queueList.keyRows().length > 0
                onClicked: queueList.adjustKey(1)
            }

            Button {
                text: "Reset"
                enabled: queueList.keyRows().length > 0
                onClicked: queueList.resetKey()
            }

            Button {
                text: "Remove"
                enabled: queueList.selectedRows().length > 0 || queueList.currentIndex >= 0
                onClicked: {
                    if (queueList.selectedRows().length > 0)
                        queueList.removeSelection()
                    else if (queueList.currentIndex >= 0)
                        songQueueModel.removeSong(queueList.currentIndex)
                }
            }

            Button {
                text: "Clear Queue"
                onClicked: {
                    songQueueModel.clearQueue()
                    queueList.selectedIndices = ({})
                }
            }
        }
    }

    // Right-click actions for the selected queue items
    Menu {
        id: contextMenu
        MenuItem {
            text: "Mark as Played"
            onTriggered: queueList.markSelection(true)
        }
        MenuItem {
            text: "Mark as Unplayed"
            onTriggered: queueList.markSelection(false)
        }
        MenuSeparator { }
        MenuItem {
            text: "Key +1"
            onTriggered: queueList.adjustKey(1)
        }
        MenuItem {
            text: "Key \u22121"
            onTriggered: queueList.adjustKey(-1)
        }
        MenuItem {
            text: "Reset Key"
            onTriggered: queueList.resetKey()
        }
        MenuSeparator { }
        MenuItem {
            text: "Remove Selected"
            onTriggered: queueList.removeSelection()
        }
    }
}
