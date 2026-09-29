import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Host triage for singer requests claimed from the web portal (pull model).
// Uses the shared context properties: webRequestModel, databaseManager,
// singerModel, songQueueModel and portalClient.
Pane {
    id: panel

    function singerFor(stageName, singerName) {
        return (stageName && stageName.length > 0) ? stageName : singerName
    }

    // Local library files matching the request, labelled for the file chooser.
    function matchesFor(artist, title) {
        var rows = databaseManager.findSongsByArtistTitle(artist, title)
        if (!rows)
            return []
        var out = []
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i]
            var name = row.filePath ? row.filePath.split('/').pop() : ""
            var label = (row.source && row.source.length > 0 ? row.source + " — " : "") + name
            out.push({ label: label, filePath: row.filePath, duration: row.duration, source: row.source })
        }
        return out
    }

    // Picker entries: a new singer named after the portal user, or an existing
    // rotation singer. Choosing an existing one renames it to the portal name so
    // the singer keeps a single identity.
    function singerOptions(stageName, singerName) {
        var portalName = singerFor(stageName, singerName)
        var names = singerModel.singerNames()
        var options = [{ label: "New singer: " + portalName, name: "" }]
        for (var i = 0; i < names.length; ++i)
            options.push({ label: "Existing: " + names[i] + "  →  " + portalName, name: names[i] })
        return options
    }

    function preferredSingerIndex(stageName, singerName) {
        var portalName = singerFor(stageName, singerName).toLowerCase()
        var names = singerModel.singerNames()
        for (var i = 0; i < names.length; ++i)
            if (names[i].toLowerCase() === portalName)
                return i + 1
        return 0
    }

    function assignExisting(chosen, portalName) {
        if (!chosen || chosen.length === 0 || chosen === portalName)
            return
        // Only rename when the portal name is not already taken.
        if (singerModel.indexOfName(portalName) >= 0)
            return
        var index = singerModel.indexOfName(chosen)
        if (index >= 0)
            singerModel.renameSinger(index, portalName)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: webRequestModel.pendingCount > 0
                  ? ("Pending requests (" + webRequestModel.pendingCount + ")")
                  : "Pending requests"
            font.bold: true
            font.pixelSize: 16
            Layout.fillWidth: true
        }

        Label {
            text: "Claimed from the singer portal. Adding one queues it for the singer and tells the portal; rejecting does the same."
            wrapMode: Text.WordWrap
            opacity: 0.7
            font.pixelSize: 11
            Layout.fillWidth: true
        }

        ListView {
            id: requestList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: webRequestModel
            spacing: 6

            delegate: Frame {
                width: requestList.width
                padding: 10

                // Captured so ComboBox (which has its own `model` property) does
                // not shadow the delegate model.
                readonly property string reqPortalId: model.portalRequestId
                readonly property string reqStage: model.stageName
                readonly property string reqSinger: model.singerName
                readonly property string reqArtist: model.artist
                readonly property string reqTitle: model.title
                readonly property string reqNote: model.note || ""
                readonly property string reqRequestedAt: model.requestedAt || ""
                readonly property var candidates: panel.matchesFor(reqArtist, reqTitle)

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: panel.singerFor(reqStage, reqSinger)
                            font.bold: true
                        }

                        Label {
                            text: reqRequestedAt
                            opacity: 0.5
                            font.pixelSize: 10
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        text: reqArtist + " — " + reqTitle
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: reqNote.length > 0
                        text: "Note: " + reqNote
                        opacity: 0.7
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ComboBox {
                            id: singerCombo
                            Layout.fillWidth: true
                            model: panel.singerOptions(reqStage, reqSinger)
                            textRole: "label"
                            currentIndex: panel.preferredSingerIndex(reqStage, reqSinger)
                        }

                        ComboBox {
                            id: fileCombo
                            Layout.fillWidth: true
                            enabled: candidates.length > 1
                            model: candidates
                            textRole: "label"
                        }

                        Button {
                            text: "Add to Queue"
                            enabled: candidates.length > 0
                            onClicked: {
                                var match = candidates[fileCombo.currentIndex >= 0
                                                       ? fileCombo.currentIndex : 0]
                                if (!match)
                                    return
                                var portalName = panel.singerFor(reqStage, reqSinger)
                                var chosen = singerCombo.currentIndex > 0
                                             ? singerCombo.model[singerCombo.currentIndex].name
                                             : ""
                                panel.assignExisting(chosen, portalName)
                                if (singerModel.indexOfName(portalName) < 0)
                                    singerModel.addSinger(portalName)
                                songQueueModel.addSong(portalName, reqTitle, reqArtist,
                                                       match.filePath, match.duration, match.source,
                                                       reqPortalId)
                                portalClient.updateRequestStatus(reqPortalId, "approved")
                                webRequestModel.resolve(reqPortalId)
                            }
                        }

                        Button {
                            text: "Reject"
                            onClicked: {
                                portalClient.updateRequestStatus(reqPortalId, "rejected")
                                webRequestModel.reject(reqPortalId)
                            }
                        }
                    }

                    Label {
                        visible: candidates.length === 0
                        text: "Not in this library — update the folder or reject."
                        color: "#CC8800"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Label {
            visible: requestList.count === 0
            text: "No pending requests. The portal is polled automatically."
            opacity: 0.7
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }
}