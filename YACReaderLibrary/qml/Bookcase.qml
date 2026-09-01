import QtQuick

// A wall of shelves curving away around the viewer, with the books standing spine out.
//
// There is no 3D engine here and no shaders. A cylinder seen from its own axis only does
// two things to what is fixed to it: the surface turns away from you towards the edges, so
// what is on it squeezes horizontally, and the shelf lines splay apart because you are
// looking along them rather than at them. Both are per-column arithmetic, which is why
// this is QML and not a new Qt module.
//
// Only the columns that can be seen exist. The library behind this is nineteen hundred
// series; the wall shows about ninety at a time, and asks the view for those by index
// rather than binding a model and instantiating the lot.
Item {
    id: wall

    // Which column sits dead centre. Fractional, so spinning is continuous rather than
    // snapping from one column to the next.
    property real centreColumn: 0
    property int shelfCount: 4
    property int columnsEitherSide: 11

    // Radians between one column and the next around the cylinder.
    readonly property real columnAngle: 0.115
    // How much screen width one radian of turn is worth.
    readonly property real focal: 640

    readonly property real spineWidth: 30
    readonly property real spineHeight: 132
    readonly property real shelfSpacing: 168

    readonly property int seriesCount: bookcase ? bookcase.seriesCount() : 0
    readonly property int columnCount: Math.max(1, Math.ceil(seriesCount / shelfCount))

    property int hoveredIndex: -1
    property int openedIndex: -1

    Connections {
        target: bookcase
        function onSeriesChanged() { wall.reset() }
    }

    function reset() {
        centreColumn = 0
        hoveredIndex = -1
        openedIndex = -1
        columns.model = 0
        columns.model = 2 * columnsEitherSide + 1
    }

    function clampCentre(value) {
        return Math.max(0, Math.min(columnCount - 1, value))
    }

    anchors.fill: parent
    clip: true

    Rectangle {
        anchors.fill: parent
        color: typeof bookcaseBackgroundColor !== "undefined" ? bookcaseBackgroundColor : "#101010"
    }

    // The room behind the shelves. A wall lit from the middle and falling into darkness at
    // the edges does more for the illusion of depth than any amount of work on the books.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#000000" }
            GradientStop { position: 0.5; color: "#1a1a1e" }
            GradientStop { position: 1.0; color: "#000000" }
        }
    }

    Item {
        id: scene
        anchors.fill: parent

        Repeater {
            id: columns
            model: 2 * wall.columnsEitherSide + 1

            Item {
                id: column

                required property int index

                // Whole columns are placed, not individual books, because a vertical strip
                // of the wall shares one angle and therefore one set of distortions.
                readonly property int offset: index - wall.columnsEitherSide
                readonly property real columnIndex: Math.round(wall.centreColumn) + offset
                readonly property real theta: (columnIndex - wall.centreColumn) * wall.columnAngle
                readonly property real cosTheta: Math.cos(theta)

                // Cylindrical placement: position is linear in the angle, which keeps the
                // wall stable at wide angles where a flat projection would fly apart.
                readonly property real screenX: scene.width / 2 + wall.focal * theta
                // The surface turning away from the viewer.
                readonly property real squeeze: Math.max(0.05, cosTheta)
                // Shelf lines splaying apart towards the edges, because at the sides you
                // are looking along them.
                readonly property real splay: 1 + 0.42 * (1 - cosTheta)

                visible: columnIndex >= 0 && columnIndex < wall.columnCount && cosTheta > 0.12
                x: screenX - width / 2
                y: 0
                width: wall.spineWidth
                height: scene.height
                z: Math.round(100 * cosTheta)
                opacity: Math.max(0, Math.min(1, 0.35 + 0.65 * cosTheta))

                Repeater {
                    model: wall.shelfCount

                    Item {
                        id: slot

                        required property int index

                        readonly property int seriesIndex: column.columnIndex * wall.shelfCount + index
                        readonly property bool present: seriesIndex >= 0 && seriesIndex < wall.seriesCount
                        readonly property real rowOffset: index - (wall.shelfCount - 1) / 2

                        width: wall.spineWidth
                        height: wall.spineHeight
                        x: 0
                        y: scene.height / 2 + rowOffset * wall.shelfSpacing * column.splay - wall.spineHeight / 2

                        transform: Scale {
                            origin.x: wall.spineWidth / 2
                            origin.y: wall.spineHeight / 2
                            xScale: column.squeeze
                            yScale: column.splay
                        }

                        // The board this row of books stands on. Each column draws its own
                        // slice, wide enough to meet its neighbours, so the shelf reads as
                        // one continuous plank curving away rather than a row of tiles.
                        Rectangle {
                            width: wall.focal * wall.columnAngle / Math.max(0.2, column.squeeze) + 2
                            height: 9
                            x: (wall.spineWidth - width) / 2
                            anchors.top: parent.bottom
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#6b6055" }
                                GradientStop { position: 0.25; color: "#3a332c" }
                                GradientStop { position: 1.0; color: "#16130f" }
                            }
                        }

                        BookSpine {
                            anchors.fill: parent
                            visible: slot.present
                            seriesIndex: slot.seriesIndex
                            hovered: wall.hoveredIndex === slot.seriesIndex

                            onEntered: wall.hoveredIndex = slot.seriesIndex
                            onExited: if (wall.hoveredIndex === slot.seriesIndex) wall.hoveredIndex = -1
                            onPicked: wall.openedIndex = slot.seriesIndex
                        }
                    }
                }
            }
        }
    }

    // The book pulled out of the wall: the cover, face on, in front of everything.
    Item {
        id: pulled
        anchors.centerIn: parent
        width: 300
        height: 440
        visible: wall.openedIndex >= 0
        z: 500
        scale: visible ? 1 : 0.85
        opacity: visible ? 1 : 0

        Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 180 } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -26
            color: "#c0000000"
            radius: 6
        }

        Image {
            id: pulledCover
            anchors.fill: parent
            source: wall.openedIndex >= 0 && bookcase ? bookcase.coverAt(wall.openedIndex) : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }

        Text {
            anchors { top: parent.bottom; topMargin: 12; horizontalCenter: parent.horizontalCenter }
            width: parent.width + 60
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            text: wall.openedIndex >= 0 && bookcase ? bookcase.titleAt(wall.openedIndex) : ""
            color: typeof bookcaseTextColor !== "undefined" ? bookcaseTextColor : "#ffffff"
            font.pointSize: 13
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (wall.openedIndex >= 0 && bookcase)
                    bookcase.openSeries(wall.openedIndex)
            }
        }
    }

    Text {
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 14 }
        visible: wall.openedIndex >= 0
        z: 501
        text: qsTr("Click the cover to open this series, or press Escape to put it back")
        color: "#9a938a"
        font.pointSize: 9
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        z: -1
        propagateComposedEvents: true

        property real pressX: 0
        property real pressCentre: 0

        onPressed: mouse => {
            pressX = mouse.x
            pressCentre = wall.centreColumn
            wall.forceActiveFocus()
        }

        onPositionChanged: mouse => {
            if (!pressed)
                return
            // Dragging turns the wall directly under the pointer.
            const turned = (pressX - mouse.x) / (wall.focal * wall.columnAngle)
            wall.centreColumn = wall.clampCentre(pressCentre + turned)
        }

        onWheel: wheel => {
            const step = wheel.angleDelta.y > 0 ? -1 : 1
            spin.to = wall.clampCentre(Math.round(wall.centreColumn) + step * 2)
            spin.restart()
        }
    }

    NumberAnimation {
        id: spin
        target: wall
        property: "centreColumn"
        duration: 260
        easing.type: Easing.OutCubic
    }

    focus: true
    Keys.onLeftPressed: { spin.to = wall.clampCentre(Math.round(wall.centreColumn) - 1); spin.restart() }
    Keys.onRightPressed: { spin.to = wall.clampCentre(Math.round(wall.centreColumn) + 1); spin.restart() }
    Keys.onEscapePressed: wall.openedIndex = -1
    Keys.onReturnPressed: if (wall.openedIndex >= 0 && bookcase) bookcase.openSeries(wall.openedIndex)
}
