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

    // Whether the singer currently shown exists in the portal's Request DB.
    property bool singerInPortal: false

    function updatePortalStatus() {
        singerInPortal = portalClient.isSingerInPortal(songQueueModel.selectedSingerName)
    }

    Connections {
        target: portalClient
        function onSingersChanged() { queuePanel.updatePortalStatus() }
        function onQueuePreviewed(summary) {
            pushConfirm.summary = summary
            pushConfirm.singerName = songQueueModel.selectedSingerName
            pushConfirm.open()
        }
    }

    Connections {
        target: songQueueModel
        function onSelectedSingerNameChanged() { queuePanel.updatePortalStatus() }
    }

    Component.onCompleted: {
        updatePortalStatus()
        portalClient.refreshSingers()
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
            // The indicator tracks the portal either way, so the host can see
            // whether a walk-up is linked; the push control only appears once
            // they are in the Request DB.
            Label {
                visible: songQueueModel.selectedSingerName.length > 0
                text: queuePanel.singerInPortal ? "In Request DB" : "Not in Request DB"
                color: queuePanel.singerInPortal ? "#4ade80" : "#fbbf24"
                font.pixelSize: 11
            }
            Button {
                visible: songQueueModel.selectedSingerName.length > 0 && queuePanel.singerInPortal
                text: "Push to Portal"
                enabled: songQueueModel.songsForSinger(songQueueModel.selectedSingerName).length > 0
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: "Replace this singer's portal requests with their app queue"
                onClicked: portalClient.previewSingerQueue(
                               songQueueModel.selectedSingerName,
                               songQueueModel.songsForSinger(songQueueModel.selectedSingerName))
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

            function selectAll() {
                var m = ({})
                for (var i = 0; i < queueList.count; ++i)
                    m[i] = true
                selectedIndices = m
                anchorIndex = queueList.count > 0 ? 0 : -1
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
                // Compared through playablePathAt, so a queue row holding the
                // .cdg half of a pair still recognises the .mp3 the deck loaded.
                if (songQueueModel.playablePathAt(rows[0]) !== mediaPlayer.filePath)
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

            // Ctrl+A selects the whole queue so bulk actions (mark all unplayed,
            // remove, key change) apply in one go. Scoped to the list's focus so
            // it never steals Ctrl+A from a text field elsewhere.
            Shortcut {
                sequence: StandardKey.SelectAll
                enabled: queueList.activeFocus
                onActivated: queueList.selectAll()
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
                        // Give the list focus so Ctrl+A selects the queue rather
                        // than reaching whatever text field was focused before.
                        queueList.forceActiveFocus()
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
                        // playablePathAt substitutes the library's own copy when the
                        // stored path belongs to another machine, which is what an
                        // OpenKJ import leaves behind when it ran before the local
                        // library was indexed.
                        var path = songQueueModel.playablePathAt(index)
                        if (!path || path.length === 0)
                            return

                        // load() starts playback itself. Calling play() as well would
                        // restart whatever was loaded before when this file turns
                        // out to be missing.
                        mediaPlayer.load(path)
                        // Apply this queue item's saved key shift.
                        mediaPlayer.setPitch(model.keyShift)
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
                      : "Ctrl/Shift-click, Ctrl+A for all \u00B7 right-click for menu"
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

    // Confirm a queue push after the portal's dry run reports the changes.
    Dialog {
        id: pushConfirm
        modal: true
        // A Popup centres over its parent by itself; anchoring it to the overlay
        // and letting a wrapping Label drive the width caused a binding loop on
        // implicitWidth, so the width is fixed instead.
        width: 380
        title: "Push queue to the Request DB"
        property var summary: ({})
        property string singerName: ""
        readonly property int willAdd: summary.created !== undefined ? summary.created : 0
        readonly property int willUpdate: summary.updated !== undefined ? summary.updated : 0
        readonly property int willRemove: summary.removed !== undefined ? summary.removed : 0

        onAccepted: portalClient.pushSingerQueue(
                        pushConfirm.singerName,
                        songQueueModel.songsForSinger(pushConfirm.singerName))

        contentItem: ColumnLayout {
            spacing: 8
            Label {
                text: "For " + pushConfirm.singerName
                font.bold: true
            }
            Label {
                text: pushConfirm.willAdd + " added, " + pushConfirm.willUpdate + " updated, "
                      + pushConfirm.willRemove + " removed"
            }
            Label {
                visible: pushConfirm.willRemove > 0
                text: "Songs no longer in the app queue will be removed from the portal "
                      + "(played songs are kept)."
                color: "#fbbf24"
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 360
            }
        }

        footer: DialogButtonBox {
            Button {
                text: "Push to Portal"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: "Cancel"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }
}
