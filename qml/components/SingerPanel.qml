import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: singerPanel

    property string selectedSingerName: ""

    // Theme-aware row shading, so long lists are easier to follow.
    readonly property color stripeColor: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.05)

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: "Singer Rotation List"
            font.bold: true
            font.pixelSize: 16
            Layout.fillWidth: true
        }

        // Drag & drop enabled singer list (above the controls)
        ListView {
            id: singerList
            Layout.fillWidth: true
            // Sit the list directly under its title. The list also absorbs all
            // the spare panel height - otherwise the ColumnLayout pushes the
            // surplus into the gaps and the list drifts away from the title.
            Layout.topMargin: -8
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            Layout.preferredHeight: 200
            clip: true
            model: singerModel

            // Index of the delegate currently being dragged
            property int dragSourceIndex: -1

            delegate: ItemDelegate {
                id: delegateItem
                width: singerList.width
                height: 40

                // Remember the layout position so we can restore it after a drag.
                property real startX: 0
                property real startY: 0

                Drag.active: dragArea.drag.active
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2
                Drag.source: delegateItem

                contentItem: RowLayout {
                    spacing: 10

                    Label {
                        visible: model.singerName === rotationController.currentSinger
                        text: "\u25B6"
                        color: "#4A90E2"
                        font.pixelSize: 12
                    }

                    Label {
                        text: model.singerName
                        font.bold: model.singerStatus === "Active"
                        opacity: model.singerStatus === "Active" ? 1.0 : 0.55
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        text: model.singerStatus === "Active" ? "\u2713" : "\u2717"
                        font.pixelSize: 14
                        color: model.singerStatus === "Active" ? "#00AA00" : "#CC0000"
                    }
                }

                background: Rectangle {
                    color: ListView.isCurrentItem
                           ? Qt.rgba(singerPanel.palette.highlight.r, singerPanel.palette.highlight.g, singerPanel.palette.highlight.b, 0.45)
                           : (dragArea.containsMouse
                              ? Qt.rgba(singerPanel.palette.windowText.r, singerPanel.palette.windowText.g, singerPanel.palette.windowText.b, 0.12)
                              : (index % 2 === 1 ? singerPanel.stripeColor : "transparent"))
                    radius: 3
                    border.color: model.singerName === rotationController.currentSinger
                                  ? "#4A90E2"
                                  : (ListView.isCurrentItem ? singerPanel.palette.highlight : "transparent")
                    border.width: model.singerName === rotationController.currentSinger
                                  ? 2
                                  : (ListView.isCurrentItem ? 1 : 0)
                }

                MouseArea {
                    id: dragArea
                    anchors.fill: parent
                    hoverEnabled: true
                    drag.target: delegateItem
                    drag.axis: Drag.YAxis
                    drag.minimumY: 0
                    drag.maximumY: Math.max(0, singerList.contentHeight - delegateItem.height)
                    drag.threshold: 6

                    onPressed: {
                        singerList.dragSourceIndex = index
                        delegateItem.startX = delegateItem.x
                        delegateItem.startY = delegateItem.y
                    }

                    onReleased: {
                        if (delegateItem.Drag.active)
                            delegateItem.Drag.drop()
                        // Snap back to the slot the ListView assigned (avoids overlap).
                        delegateItem.x = delegateItem.startX
                        delegateItem.y = delegateItem.startY
                        singerList.dragSourceIndex = -1
                    }

                    onClicked: {
                        singerList.currentIndex = index
                        singerPanel.selectedSingerName = model.singerName
                        // Make this the current singer: the queue panel shows
                        // this singer's personal queue.
                        rotationController.currentSinger = model.singerName
                    }

                    onDoubleClicked: singerModel.toggleSingerStatus(index)
                }

                DropArea {
                    anchors.fill: parent
                    onDropped: {
                        if (singerList.dragSourceIndex >= 0 && singerList.dragSourceIndex !== index)
                            singerModel.moveSinger(singerList.dragSourceIndex, index)
                    }
                }
            }

            ScrollBar.vertical: ScrollBar { }
        }

        // INPUT SECTION - AT THE BOTTOM
        Label {
            text: "Add New Singer"
            font.pixelSize: 14
            font.bold: true
            Layout.fillWidth: true
        }

        TextField {
            id: newSingerInput
            placeholderText: "Enter singer name..."
            Layout.fillWidth: true
            onEditingFinished: {
                var name = newSingerInput.text.trim()
                if (name.length > 0) {
                    singerModel.addSinger(name)
                    newSingerInput.text = ""
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Button {
                text: "Add"
                Layout.fillWidth: true
                onClicked: {
                    var name = newSingerInput.text.trim()
                    if (name.length > 0) {
                        singerModel.addSinger(name)
                        newSingerInput.text = ""
                    }
                }
            }

            Button {
                text: "Remove"
                Layout.fillWidth: true
                onClicked: {
                    if (singerList.currentIndex >= 0) {
                        singerModel.removeSinger(singerList.currentIndex)
                        singerPanel.selectedSingerName = ""
                        rotationController.currentSinger = ""
                    }
                }
            }
        }

        // DRAG & DROP INSTRUCTIONS - AT THE BOTTOM
        Label {
            text: "\uD83C\uDFB5 Drag & Drop: Reorder singers | Double-click: Toggle status"
            font.pixelSize: 10
            color: "#4A90E2"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        // CONTROL SECTION - AT THE BOTTOM
        GroupBox {
            title: "Actions"
            Layout.fillWidth: true
            Layout.minimumHeight: 80

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Label {
                    text: "Selected Singer:"
                    font.pixelSize: 11
                    opacity: 0.7
                    Layout.fillWidth: true
                }

                Label {
                    text: singerList.currentIndex >= 0 ? singerPanel.selectedSingerName : "(None selected)"
                    font.pixelSize: 12
                    font.bold: singerList.currentIndex >= 0
                    opacity: singerList.currentIndex >= 0 ? 1.0 : 0.5
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Button {
                        text: "Toggle Inactive"
                        Layout.fillWidth: true
                        enabled: singerList.currentIndex >= 0
                        onClicked: singerModel.toggleSingerStatus(singerList.currentIndex)
                    }

                    Button {
                        text: "Clear All"
                        Layout.fillWidth: true
                        onClicked: {
                            singerModel.clearAllSingers()
                            singerPanel.selectedSingerName = ""
                            rotationController.currentSinger = ""
                        }
                    }
                }

                // Skip the current singer (e.g. they stepped out): they go to
                // the bottom of the rotation and their song stays unplayed.
                Button {
                    text: "\u23ED Skip Singer to Bottom"
                    Layout.fillWidth: true
                    enabled: rotationController.currentSinger.length > 0
                    onClicked: rotationController.skipCurrentSinger()
                }
            }
        }

        Label {
            text: "Total Singers: " + singerList.count
            font.pixelSize: 12
            opacity: 0.7
            Layout.fillWidth: true
        }
    }
}
