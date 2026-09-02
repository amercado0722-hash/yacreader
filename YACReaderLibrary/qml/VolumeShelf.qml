import QtQuick

// One series taken off the wall and opened: its volumes standing on shelves, covers out.
//
// Covers rather than spines here, which is the opposite of the wall behind it and is the
// right way round. On the wall you are looking for a series among nineteen hundred, and a
// spine is what a shelf of that size can show you. Once you are inside one series you are
// looking for a volume among a handful, and the cover is what tells them apart - the volume
// number is often the only thing printed on a manga spine, and half of these files do not
// know their own number.
Item {
    id: shelf

    property int volumeCount: 0
    property string seriesTitle: ""

    signal closed()

    function reload() {
        volumeCount = bookcase ? bookcase.volumeCount() : 0
        seriesTitle = bookcase ? bookcase.openedSeriesTitle() : ""
        grid.contentY = 0
    }

    anchors.fill: parent
    focus: visible

    // The room, darkened. The wall is still there behind this and should read as still
    // there - dimmed rather than replaced, so closing the series puts you back where you
    // were rather than somewhere new.
    Rectangle {
        anchors.fill: parent
        color: "#e0080706"

        MouseArea {
            anchors.fill: parent
            onClicked: shelf.closed()
        }
    }

    Item {
        id: header

        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 62

        Text {
            id: seriesName
            anchors { left: parent.left; leftMargin: 28; verticalCenter: parent.verticalCenter }
            width: parent.width - 320
            text: shelf.seriesTitle
            elide: Text.ElideRight
            maximumLineCount: 1
            color: typeof bookcaseTextColor !== "undefined" ? bookcaseTextColor : "#ffffff"
            font.pointSize: 15
        }

        Text {
            anchors { left: seriesName.right; leftMargin: 12; baseline: seriesName.baseline }
            text: shelf.volumeCount === 1 ? qsTr("1 volume") : qsTr("%1 volumes").arg(shelf.volumeCount)
            color: "#8d857a"
            font.pointSize: 10
        }

        // The way out to the folder view, for everything the shelf deliberately does not do.
        Text {
            id: openInLibrary
            anchors { right: closeButton.left; rightMargin: 22; verticalCenter: parent.verticalCenter }
            text: qsTr("Open in library")
            color: libraryArea.containsMouse ? "#e8e0d4" : "#8d857a"
            font.pointSize: 10

            MouseArea {
                id: libraryArea
                anchors { fill: parent; margins: -8 }
                hoverEnabled: true
                onClicked: if (bookcase) bookcase.showOpenedSeriesInLibrary()
            }
        }

        Text {
            id: closeButton
            anchors { right: parent.right; rightMargin: 28; verticalCenter: parent.verticalCenter }
            text: qsTr("Back to the shelves")
            color: closeArea.containsMouse ? "#e8e0d4" : "#8d857a"
            font.pointSize: 10

            MouseArea {
                id: closeArea
                anchors { fill: parent; margins: -8 }
                hoverEnabled: true
                onClicked: shelf.closed()
            }
        }
    }

    GridView {
        id: grid

        // Sized so the covers land on a whole number of columns with the shelf boards
        // reaching the full width of each row.
        readonly property int columns: Math.max(1, Math.floor(width / 168))

        anchors { top: header.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
        anchors { leftMargin: 24; rightMargin: 24; bottomMargin: 18 }
        clip: true
        cacheBuffer: 600

        cellWidth: width / columns
        cellHeight: 268

        model: shelf.volumeCount

        delegate: Item {
            id: cell

            required property int index

            readonly property string number: bookcase ? bookcase.volumeNumberAt(index) : ""
            readonly property bool read: bookcase ? bookcase.volumeReadAt(index) : false

            width: grid.cellWidth
            height: grid.cellHeight

            // The board this volume stands on. Drawn per cell and spanning the whole cell
            // width, so neighbouring cells join into one plank across the row - the same
            // trick the library grid uses, and the reason a shelf can exist in a view that
            // does not itself know rows are shelves.
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 26 }
                height: 4
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#9a8a76" }
                    GradientStop { position: 0.5; color: "#6d6155" }
                    GradientStop { position: 1.0; color: "#463d33" }
                }
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 26
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#332c24" }
                    GradientStop { position: 0.5; color: "#211c16" }
                    GradientStop { position: 1.0; color: "#100d0a" }
                }
            }

            Item {
                id: book

                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 30 }
                width: 132
                height: 196
                scale: hover.containsMouse ? 1.05 : 1

                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

                // Standing on the board rather than pasted on the background: a shadow at
                // the foot, and the edge of the pages down one side.
                Rectangle {
                    anchors { left: parent.left; right: parent.right; top: parent.bottom }
                    height: 10
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#b3000000" }
                        GradientStop { position: 1.0; color: "#00000000" }
                    }
                }

                Rectangle {
                    anchors { right: parent.left; top: parent.top; bottom: parent.bottom; topMargin: 3 }
                    width: 5
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#2a2622" }
                        GradientStop { position: 1.0; color: "#cfc6b2" }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: "#1a1714"
                }

                Image {
                    id: cover
                    anchors.fill: parent
                    source: bookcase ? bookcase.volumeCoverAt(cell.index) : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    // A cover is drawn at a fraction of its stored size here; decoding it at
                    // that size rather than full is the difference between a shelf that
                    // opens and one that stutters on a series of two hundred volumes.
                    sourceSize.width: 264
                }

                // Volumes already read sit back a little, the way a finished book does.
                Rectangle {
                    anchors.fill: parent
                    visible: cell.read
                    color: "#73000000"
                }

                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.width: 1
                    border.color: hover.containsMouse ? "#66ffffff" : "#33000000"
                }

                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: if (bookcase) bookcase.openVolume(cell.index)
                }
            }

            // The number on the board in front of the volume, where a shelf label goes.
            Text {
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 6 }
                width: grid.cellWidth - 16
                horizontalAlignment: Text.AlignHCenter
                text: cell.number.length > 0 ? qsTr("Vol. %1").arg(cell.number) : (bookcase ? bookcase.volumeTitleAt(cell.index) : "")
                elide: Text.ElideRight
                maximumLineCount: 1
                color: "#a89e90"
                font.pointSize: 8
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: shelf.volumeCount === 0
        text: qsTr("This series has no volumes in the library")
        color: "#8d857a"
        font.pointSize: 11
    }

    Keys.onEscapePressed: event => {
        shelf.closed()
        event.accepted = true
    }
}
