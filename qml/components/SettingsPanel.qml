import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window

// Application settings that do not belong to a single working panel: the
// karaoke library folder and its export, the OpenKJ singer import, and the idle
// background image for the secondary window.
Pane {
    id: settingsPanel

    // The Display settings live on the main window so this page and the deck's
    // secondary window share one instance; two Settings objects on the same key
    // would not notify each other, and a change here would not show up there
    // until the next launch.
    readonly property QtObject displaySettings: Window.window ? Window.window.displaySettings : null

    // A null QString arrives in QML as undefined, so normalise before asking
    // either of these for a length.
    readonly property string importError: openKjImporter.lastError ? openKjImporter.lastError : ""
    readonly property string importSummary: openKjImporter.lastSummary ? openKjImporter.lastSummary : ""
    readonly property string exportError: songListExporter.lastError ? songListExporter.lastError : ""
    readonly property string exportSummary: songListExporter.lastSummary ? songListExporter.lastSummary : ""

    // Portal upload state. The connection fields live on the main window's
    // Portal Settings so the exporter, this page and PortalClient share one copy.
    readonly property QtObject portalSettings: Window.window ? Window.window.portalSettings : null
    // `typeof` is safe even when the property is missing, so a stale binary
    // running newer QML degrades instead of throwing a ReferenceError.
    readonly property var portal: typeof portalClient !== "undefined" ? portalClient : null
    readonly property string portalError: portal && portal.lastError ? portal.lastError : ""
    readonly property string portalSummary: portal && portal.lastSummary ? portal.lastSummary : ""
    // Combines the last message with how many status updates are waiting a retry.
    readonly property string portalStatus: {
        var base = portalError.length > 0 ? portalError : portalSummary
        var pending = portal ? portal.pendingUpdates : 0
        if (pending > 0)
            return (base.length > 0 ? base + "  ·  " : "") + pending + " update(s) pending retry"
        return base
    }

    // Reported when a karaoke index run finishes.
    property string scanMessage: ""

    // The karaoke search directories read from the database. Refreshed by hand,
    // since the list only changes on add/remove/finish, all of which pass
    // through this panel.
    property var directoryRows: []

    // Row the Update/Remove buttons act on; -1 when nothing is picked.
    property int selectedDirectoryIndex: -1

    // Directories still to visit during an "Update All". Scans run one at a
    // time, so each finish starts the next.
    property var pendingRescans: []

    function refreshDirectories() {
        directoryRows = databaseManager.getDirectories()
        if (selectedDirectoryIndex >= directoryRows.length)
            selectedDirectoryIndex = -1
    }

    function updateAllDirectories() {
        refreshDirectories()
        pendingRescans = []
        for (var i = 0; i < directoryRows.length; ++i)
            pendingRescans.push(directoryRows[i].path)
        startNextRescan()
    }

    function startNextRescan() {
        if (databaseManager.scanning || pendingRescans.length === 0)
            return
        databaseManager.rescanDirectory(pendingRescans.shift())
    }

    // Pattern to use for the next Add when the picker is on a custom regex.
    property string customRegex: ""

    // Picking a row loads its pattern into the picker, so changing the pattern
    // re-indexes the folder that is actually selected. A folder indexed with a
    // custom regex shows no token selection.
    function selectDirectory(index) {
        selectedDirectoryIndex = index
        if (index < 0 || index >= directoryRows.length) {
            patternCombo.currentIndex = 0
            return
        }
        patternCombo.currentIndex = folderScanner.supportedPatterns.indexOf(directoryRows[index].pattern)
    }

    // Indexing runs off the UI thread, so the library rows only appear once it
    // reports back. The model refresh lives here now that the folder controls do.
    Connections {
        target: databaseManager
        function onLibraryScanFinished(added, unreadable) {
            songDatabaseModel.refreshData()
            settingsPanel.refreshDirectories()
            settingsPanel.scanMessage = unreadable > 0
                ? ("Indexed " + added + " songs; " + unreadable + " files could not be read.")
                : ("Indexed " + added + " songs.")
            // An "Update All" walks the list one directory at a time.
            settingsPanel.startNextRescan()
        }
    }

    Component.onCompleted: refreshDirectories()

    // OpenKJ writes JSON; its older XML flavour is read as well.
    FileDialog {
        id: openKjImportDialog
        title: "Import singers from OpenKJ"
        nameFilters: ["OpenKJ export (*.json *.xml)", "All files (*)"]
        onAccepted: openKjImporter.importFromFile(selectedFile)
    }

    FileDialog {
        id: backgroundImageDialog
        title: "Choose a Background Image"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.svg)", "All files (*)"]
        onAccepted: if (settingsPanel.displaySettings)
                        settingsPanel.displaySettings.backgroundImage = selectedFile
    }

    FolderDialog {
        id: folderDialog
        title: "Select Music Folder"
        onAccepted: {
            var path = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            var pattern = settingsPanel.customRegex.length > 0
                          ? settingsPanel.customRegex
                          : (patternCombo.currentIndex >= 0
                             ? patternCombo.currentText
                             : folderScanner.supportedPatterns[0])
            databaseManager.addDirectory(path, pattern)
            settingsPanel.refreshDirectories()
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

    // Custom naming pattern: a raw regex whose group 1 is the Artist and group 2
    // the Title. The preview and the worked examples update as it is typed.
    Dialog {
        id: customPatternDialog
        title: "Custom naming pattern"
        modal: true
        width: 460
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string regexText: ""
        property string exampleText: "Queen - Bohemian Rhapsody"
        readonly property var preview: folderScanner.testPattern(exampleText, regexText)

        onOpened: {
            regexText = (settingsPanel.selectedDirectoryIndex >= 0
                         && settingsPanel.selectedDirectoryIndex < settingsPanel.directoryRows.length)
                        ? settingsPanel.directoryRows[settingsPanel.selectedDirectoryIndex].pattern
                        : ""
            exampleText = "Queen - Bohemian Rhapsody"
        }

        onAccepted: {
            // A selected folder is re-indexed right away; with nothing selected
            // the regex becomes the pattern the next Add Folder will use.
            if (settingsPanel.selectedDirectoryIndex >= 0) {
                var row = settingsPanel.directoryRows[settingsPanel.selectedDirectoryIndex]
                if (row)
                    databaseManager.setDirectoryPattern(row.path, regexText)
            } else {
                settingsPanel.customRegex = regexText
            }
        }

        ColumnLayout {
            width: customPatternDialog.availableWidth
            spacing: 6

            Label { text: "Regular expression"; font.bold: true }
            TextField {
                Layout.fillWidth: true
                placeholderText: "^(.*?) - (.*)$"
                text: customPatternDialog.regexText
                onTextChanged: customPatternDialog.regexText = text
            }
            Label {
                text: "Capture group 1 is the Artist, group 2 the Title. A token pattern such as {Artist} - {Title} works too."
                wrapMode: Text.WordWrap
                font.pixelSize: 11
                opacity: 0.7
                Layout.fillWidth: true
            }

            Label {
                text: "Example filename (without extension)"
                font.bold: true
                Layout.topMargin: 8
            }
            TextField {
                Layout.fillWidth: true
                placeholderText: "Queen - Bohemian Rhapsody"
                text: customPatternDialog.exampleText
                onTextChanged: customPatternDialog.exampleText = text
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: customPatternDialog.preview.ok ? "#2E9E2E" : "#CC0000"
                text: {
                    var r = customPatternDialog.preview
                    if (!r.ok)
                        return "No match" + (r.error.length > 0 ? (": " + r.error) : "")
                    return "Artist: " + r.artist + "      Title: " + r.title
                }
            }

            Label { text: "Examples"; font.bold: true; Layout.topMargin: 8 }
            Repeater {
                model: [
                    "Queen - Bohemian Rhapsody",
                    "Bohemian Rhapsody - Queen",
                    "01 - Queen - Bohemian Rhapsody",
                    "Queen-Bohemian Rhapsody"
                ]
                delegate: Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    text: {
                        var r = folderScanner.testPattern(modelData, customPatternDialog.regexText)
                        if (!r.ok)
                            return modelData + "   \u2192   (no match)"
                        return modelData + "   \u2192   " + r.artist + "  \u00B7  " + r.title
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "Settings"
            font.bold: true
            font.pixelSize: 20
            Layout.fillWidth: true
        }

        ScrollView {
            id: settingsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: settingsScroll.availableWidth
                spacing: 12

                // Karaoke library: folders and the song-book export.
                GroupBox {
                    title: "Karaoke Library"
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Label {
                            text: "Folders scanned for karaoke songs. Select one to update or remove it."
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            opacity: 0.7
                            Layout.fillWidth: true
                        }

                        // Plain rows in a Repeater rather than a ListView: a
                        // ListView is itself a Flickable, and nesting one inside
                        // the page's ScrollView swallowed the row presses. The
                        // page does the scrolling; these rows just sit in it.
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Repeater {
                                model: settingsPanel.directoryRows

                                delegate: ItemDelegate {
                                    id: directoryRow
                                    Layout.fillWidth: true
                                    readonly property bool selected: settingsPanel.selectedDirectoryIndex === index

                                    contentItem: RowLayout {
                                        spacing: 8

                                        Label {
                                            text: modelData.path
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }

                                        Label {
                                            text: modelData.pattern
                                            font.pixelSize: 10
                                            opacity: 0.6
                                        }

                                        Label {
                                            text: modelData.songCount + " song(s)"
                                            font.pixelSize: 11
                                            opacity: 0.7
                                        }
                                    }

                                    background: Rectangle {
                                        color: directoryRow.selected
                                               ? Qt.rgba(settingsPanel.palette.highlight.r,
                                                         settingsPanel.palette.highlight.g,
                                                         settingsPanel.palette.highlight.b, 0.45)
                                               : (directoryMouse.containsMouse
                                                  ? Qt.rgba(settingsPanel.palette.windowText.r,
                                                            settingsPanel.palette.windowText.g,
                                                            settingsPanel.palette.windowText.b, 0.12)
                                                  : (index % 2 === 1
                                                     ? Qt.rgba(settingsPanel.palette.windowText.r,
                                                               settingsPanel.palette.windowText.g,
                                                               settingsPanel.palette.windowText.b, 0.05)
                                                     : "transparent"))
                                        radius: 3
                                    }

                                    MouseArea {
                                        id: directoryMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: settingsPanel.selectDirectory(index)
                                    }
                                }
                            }
                        }

                        // Naming pattern, stored per folder. Add Folder… uses it;
                        // changing it while a folder is selected re-indexes that
                        // folder under the new pattern.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: "Naming pattern"
                                font.pixelSize: 12
                            }

                            ComboBox {
                                id: patternCombo
                                Layout.fillWidth: true
                                model: folderScanner.supportedPatterns
                                currentIndex: 0
                                enabled: !databaseManager.scanning
                                onActivated: function(index) {
                                    if (settingsPanel.selectedDirectoryIndex >= 0) {
                                        // Changing the pattern re-indexes the selected folder.
                                        var row = settingsPanel.directoryRows[settingsPanel.selectedDirectoryIndex]
                                        if (row)
                                            databaseManager.setDirectoryPattern(row.path, currentText)
                                    } else {
                                        // Otherwise it is the pattern the next Add will use.
                                        settingsPanel.customRegex = ""
                                    }
                                }
                            }

                            Button {
                                text: "Custom regex…"
                                enabled: !databaseManager.scanning
                                onClicked: customPatternDialog.open()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                text: "Add Folder…"
                                enabled: !databaseManager.scanning
                                onClicked: folderDialog.open()
                            }

                            Button {
                                text: "Remove"
                                enabled: settingsPanel.selectedDirectoryIndex >= 0 && !databaseManager.scanning
                                onClicked: {
                                    var row = settingsPanel.directoryRows[settingsPanel.selectedDirectoryIndex]
                                    if (row) {
                                        databaseManager.removeDirectory(row.path)
                                        songDatabaseModel.refreshData()
                                        settingsPanel.selectedDirectoryIndex = -1
                                        settingsPanel.refreshDirectories()
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                text: "Update"
                                enabled: settingsPanel.selectedDirectoryIndex >= 0 && !databaseManager.scanning
                                onClicked: {
                                    var row = settingsPanel.directoryRows[settingsPanel.selectedDirectoryIndex]
                                    if (row)
                                        databaseManager.rescanDirectory(row.path)
                                }
                            }

                            Button {
                                text: "Update All"
                                enabled: settingsPanel.directoryRows.length > 0 && !databaseManager.scanning
                                onClicked: settingsPanel.updateAllDirectories()
                            }
                        }

                        Label {
                            visible: databaseManager.scanning || settingsPanel.scanMessage.length > 0
                            text: databaseManager.scanning
                                  ? ("Measuring " + databaseManager.scanProgress + " of "
                                     + databaseManager.scanTotal + " files…")
                                  : settingsPanel.scanMessage
                            font.pixelSize: 10
                            color: databaseManager.scanning ? "#4A90E2" : palette.windowText
                            opacity: databaseManager.scanning ? 1.0 : 0.7
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            // Plain Artist/Title pairs, deduplicated, for the
                            // song-book workflow.
                            Button {
                                text: "Export Song List…"
                                onClicked: songListExportDialog.open()
                            }

                            Label {
                                text: settingsPanel.exportError.length > 0
                                      ? settingsPanel.exportError
                                      : settingsPanel.exportSummary
                                visible: text.length > 0
                                color: settingsPanel.exportError.length > 0 ? "#CC0000" : "#00AA00"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                // Push the song library to the self-hosted singer portal. Uses
                // the same Artist/Title export as the song book, so there is no
                // second format to maintain.
                GroupBox {
                    title: "Singer Portal"
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Label {
                            text: "Upload the song library to the web portal so singers can search it. Use https:// in production; a plain http:// address (or a bare host:port) is fine for local testing. The portal must have the same bridge token configured."
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            opacity: 0.7
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            text: "Enable portal sync"
                            checked: settingsPanel.portalSettings ? settingsPanel.portalSettings.enabled : false
                            onToggled: if (settingsPanel.portalSettings) settingsPanel.portalSettings.enabled = checked
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: "Portal URL" }

                            TextField {
                                id: portalUrlField
                                Layout.fillWidth: true
                                placeholderText: "http://192.168.1.50:3000"
                                text: settingsPanel.portalSettings ? settingsPanel.portalSettings.portalUrl : ""
                                onTextEdited: {
                                    if (settingsPanel.portalSettings)
                                        settingsPanel.portalSettings.portalUrl = portalUrlField.text
                                }
                            }

                            Label { text: "Bridge token" }

                            TextField {
                                id: bridgeTokenField
                                Layout.fillWidth: true
                                echoMode: TextInput.Password
                                text: settingsPanel.portalSettings ? settingsPanel.portalSettings.bridgeToken : ""
                                onTextEdited: {
                                    if (settingsPanel.portalSettings)
                                        settingsPanel.portalSettings.bridgeToken = bridgeTokenField.text
                                }
                            }

                            Label {
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: 10
                                opacity: 0.6
                                text: {
                                    var token = settingsPanel.portalSettings ? settingsPanel.portalSettings.bridgeToken : ""
                                    return token.length > 0
                                           ? ("Token set (" + token.length + " characters) — must match HOST_BRIDGE_TOKEN in the portal .env")
                                           : "No token set — copy HOST_BRIDGE_TOKEN from the portal .env"
                                }
                            }
                        }

                        CheckBox {
                            text: "Sync after each export"
                            checked: settingsPanel.portalSettings ? settingsPanel.portalSettings.autoSyncAfterExport : false
                            onToggled: if (settingsPanel.portalSettings) settingsPanel.portalSettings.autoSyncAfterExport = checked
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                text: "Sync Now"
                                enabled: settingsPanel.portal !== null && !settingsPanel.portal.busy
                                onClicked: {
                                    if (settingsPanel.portal)
                                        settingsPanel.portal.syncSongList()
                                }
                            }

                            Button {
                                text: "Test"
                                enabled: settingsPanel.portal !== null && !settingsPanel.portal.busy
                                onClicked: {
                                    if (settingsPanel.portal)
                                        settingsPanel.portal.testConnection()
                                }
                            }

                            Label {
                                text: settingsPanel.portalStatus
                                visible: text.length > 0
                                color: settingsPanel.portalError.length > 0 ? "#CC0000" : "#00AA00"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                // Bring a regulars list over from OpenKJ: singers and history.
                GroupBox {
                    title: "Singer Rotation"
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Label {
                            text: "Import a regulars list from OpenKJ, including their queues and play history."
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            opacity: 0.7
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "\uD83D\uDCC2 Import from OpenKJ"
                            onClicked: openKjImportDialog.open()
                        }

                        Label {
                            text: settingsPanel.importError.length > 0
                                  ? settingsPanel.importError
                                  : settingsPanel.importSummary
                            visible: text.length > 0
                            color: settingsPanel.importError.length > 0 ? "#CC0000" : "#00AA00"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // Idle backdrop for the secondary window.
                GroupBox {
                    title: "Display"
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Label {
                            text: "Image shown on the secondary window whenever no karaoke graphics are loaded."
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            opacity: 0.7
                            Layout.fillWidth: true
                        }

                        Label {
                            text: settingsPanel.displaySettings
                                  && settingsPanel.displaySettings.backgroundImage.length > 0
                                  ? settingsPanel.displaySettings.backgroundImage
                                  : "No background image set."
                            elide: Text.ElideMiddle
                            font.pixelSize: 10
                            opacity: 0.7
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text: settingsPanel.displaySettings
                                      && settingsPanel.displaySettings.backgroundImage.length > 0
                                      ? "Change Background Image…"
                                      : "Add Background Image…"
                                Layout.fillWidth: true
                                onClicked: backgroundImageDialog.open()
                            }

                            Button {
                                text: "Clear"
                                visible: settingsPanel.displaySettings
                                         && settingsPanel.displaySettings.backgroundImage.length > 0
                                onClicked: if (settingsPanel.displaySettings)
                                               settingsPanel.displaySettings.backgroundImage = ""
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
