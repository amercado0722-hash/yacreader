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

    // Counters behind the readout at the foot of the shelf. Three attempts at fixing this
    // view failed because every one of them was a theory about which events were arriving,
    // and there was no way to find out which were. These say so outright.
    property int backdropPresses: 0
    property int buttonPresses: 0
    property int buttonReleases: 0
    property int coverHovers: 0
    property int wheels: 0

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
            onPressed: shelf.backdropPresses++
            onClicked: shelf.closed()
            // Absorbed rather than left to fall through. Behind this sits the wall's own
            // wheel handler, and a wheel that misses the shelf was silently spinning the
            // wall underneath it.
            onWheel: wheel => wheel.accepted = true
        }
    }

    Item {
        id: header

        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 62

        // The two actions first, because everything to their left is sized from where they
        // end. Laid out right to left like this, the title and the count cannot run
        // underneath them, which is what a fixed width for the title allowed: a short series
        // name left the count sitting on top of the first link.
        Row {
            id: actions

            anchors { right: parent.right; rightMargin: 24; verticalCenter: parent.verticalCenter }
            spacing: 6

            // Written out twice rather than made into a component with a signal. The
            // component version rendered its labels and then swallowed every click on them
            // without acting, and this is the shape that is known to work in this file: a
            // rectangle, a mouse area filling it, and the work done in the handler.
            Rectangle {
                width: libraryLabel.implicitWidth + 26
                height: 32
                radius: 4
                border.width: 1
                border.color: libraryArea.containsMouse ? "#6b6055" : "#332c24"
                color: libraryArea.containsMouse ? "#2a241d" : "transparent"

                Text {
                    id: libraryLabel
                    anchors.centerIn: parent
                    text: qsTr("Open in library")
                    color: libraryArea.containsMouse ? "#efe8dc" : "#a89e90"
                    font.pointSize: 10
                }

                MouseArea {
                    id: libraryArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: if (bookcase) bookcase.showOpenedSeriesInLibrary()
                }
            }

            Rectangle {
                width: closeLabel.implicitWidth + 26
                height: 32
                radius: 4
                border.width: 1
                border.color: closeArea.containsMouse ? "#6b6055" : "#332c24"
                color: closeArea.containsMouse ? "#2a241d" : "transparent"

                Text {
                    id: closeLabel
                    anchors.centerIn: parent
                    text: qsTr("Back to the shelves")
                    color: closeArea.containsMouse ? "#efe8dc" : "#a89e90"
                    font.pointSize: 10
                }

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: shelf.buttonPresses++
                    onReleased: shelf.buttonReleases++
                    onClicked: shelf.closed()
                }
            }
        }

        Text {
            id: countLabel
            anchors { right: actions.left; rightMargin: 18; verticalCenter: parent.verticalCenter }
            text: shelf.volumeCount === 1 ? qsTr("1 volume") : qsTr("%1 volumes").arg(shelf.volumeCount)
            color: "#8d857a"
            font.pointSize: 10
        }

        Text {
            id: seriesName
            anchors { left: parent.left; leftMargin: 28; right: countLabel.left; rightMargin: 14; verticalCenter: parent.verticalCenter }
            text: shelf.seriesTitle
            elide: Text.ElideRight
            maximumLineCount: 1
            color: typeof bookcaseTextColor !== "undefined" ? bookcaseTextColor : "#ffffff"
            font.pointSize: 15
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

        // No wheel handling here. A GridView reparents its declared children into its own
        // moving content item, so a handler written inside it does not sit over the view at
        // all - which is why the first attempt at this changed nothing. It is done by a
        // plain mouse area laid over the grid instead, below.

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
                    onEntered: shelf.coverHovers++
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

    // The wheel, taken over the grid by something that is unambiguously on top of it.
    //
    // NoButton means presses and releases are not accepted here at all and fall straight
    // through to the covers underneath, so this steals the wheel and nothing else. Sixty one
    // volumes is thirteen rows and three of them fit on screen; the wheel is not optional.
    MouseArea {
        anchors.fill: grid
        // Accepting the left button and then refusing each press, rather than declaring
        // NoButton. NoButton is the Qt 5 way to make an area that only wants the wheel, and
        // in Qt 6 it can take the item out of mouse delivery altogether - which would leave
        // this handler exactly as dead as the one it replaced.
        acceptedButtons: Qt.LeftButton
        propagateComposedEvents: true

        onPressed: mouse => mouse.accepted = false

        onWheel: wheel => {
            shelf.wheels++
            const limit = Math.max(0, grid.contentHeight - grid.height)
            grid.contentY = Math.max(0, Math.min(limit, grid.contentY - wheel.angleDelta.y))
            wheel.accepted = true
        }
    }

    // What is actually arriving, on screen, because three rounds of inferring it from
    // symptoms got the wrong answer three times.
    Rectangle {
        anchors { left: parent.left; bottom: parent.bottom; leftMargin: 8; bottomMargin: 8 }
        width: probe.implicitWidth + 16
        height: probe.implicitHeight + 8
        color: "#cc000000"
        radius: 3
        z: 999

        Text {
            id: probe
            anchors.centerIn: parent
            color: "#9a938a"
            font.pointSize: 8
            text: "grid " + Math.round(grid.width) + "x" + Math.round(grid.height)
                    + " content " + Math.round(grid.contentHeight)
                    + " cols " + grid.columns
                    + " n " + shelf.volumeCount
                    + "  |  backdrop " + shelf.backdropPresses
                    + " btn " + shelf.buttonPresses + "/" + shelf.buttonReleases
                    + " cover " + shelf.coverHovers
                    + " wheel " + shelf.wheels
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
