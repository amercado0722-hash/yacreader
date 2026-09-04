import QtQuick

// A wall of shelves curving away around the viewer, with the books standing spine out.
//
// There is no 3D engine here and no shaders. A cylinder seen from its own axis only does
// two things to what is fixed to it: the surface turns away from you towards the edges, so
// what is on it squeezes horizontally, and the shelf lines splay apart because you are
// looking along them rather than at them. Both are arithmetic, which is why this is QML
// and not a new Qt module.
//
// The wall is measured in pixels along its own surface rather than in columns. Books are
// not all the same thickness - a forty volume series is a fat spine and a one-shot is a
// sliver - and a shelf of identical bars reads as a chart, not as books. Each shelf is
// therefore its own run of books at its own widths, laid end to end, and the whole wall
// turns by one shared distance.
//
// Only what can be seen exists. The library behind this is nineteen hundred series; the
// wall shows perhaps a hundred at a time and asks the view for those by index rather than
// binding a model and instantiating the lot.
Item {
    id: wall

    // How far the wall has been turned, in pixels along the shelf surface. Continuous, so
    // spinning does not snap from one book to the next.
    property real wallOffset: 0
    property int shelfCount: 4
    // Slots kept alive per shelf. Enough to reach the dark at both edges at ordinary book
    // widths; a run of unusually thin ones simply stops short, in near black, unseen.
    readonly property int slotsPerRow: 84

    // The radius of the cylinder in pixels. Tied to the width of the view so the wall
    // reaches the edges of any window rather than only the one it was tuned in.
    readonly property real focal: Math.max(360, width * 0.5)
    // How much of the wall's surface is on screen at once, in those same pixels. Slots stop
    // being drawn at about 1.25 radians either side of centre, so this is that arc.
    readonly property real visibleArc: focal * 2.4
    // Sized to the window rather than fixed, because the rows splay outwards and the
    // outermost ones were running off the top and bottom of the view. The allowance has to
    // cover the splay at its worst, not the spacing at the middle of the wall: the top and
    // bottom shelves were sitting comfortably in the centre of the view and then walking
    // off the top and bottom edges by the time they reached the sides.
    readonly property real shelfSpacing: Math.max(60, height / (shelfCount + 1.45))
    readonly property real spineHeight: shelfSpacing * 0.78

    // How hard the shelf lines splay apart towards the edges. The true figure for this
    // projection is 1/cos, which at the middle of the view is almost nothing; a wall that
    // reads as flat across the centre is the honest answer and the wrong one, so the bend
    // is exaggerated.
    readonly property real splayStrength: 0.55

    // Assigned rather than bound. seriesCount() is a plain invokable with no change signal
    // behind it, so a binding on it is evaluated once - at load, when the library has not
    // been handed over yet and the answer is zero - and never again. That left the wall
    // convinced it had no books.
    property int seriesCount: 0

    // Assigned in reset() for the same reason as seriesCount: it comes from a plain
    // invokable, so a binding on it would answer once, at load, and never again.
    property string filterText: ""

    // One entry per shelf: the series standing on it, their thicknesses, and the running
    // distance to each one along the wall. Built once when the library changes, because
    // finding what is in front of you is then a search over this rather than a walk over
    // nineteen hundred series every frame.
    property var shelves: []

    property int hoveredIndex: -1
    property int openedIndex: -1

    Connections {
        target: bookcase
        function onSeriesChanged() { wall.reset() }
        // The volumes of the opened series arrive after the click, because loading them is
        // a query against the library rather than something the scene already holds.
        function onVolumesChanged() { shelf.reload() }
        // Closing can come from outside the scene - the window's Escape - so the wall
        // follows the view rather than the view following the wall.
        function onSeriesClosed() { wall.openedIndex = -1 }
    }

    function open(index) {
        if (index < 0 || index >= seriesCount)
            return
        openedIndex = index
        if (bookcase)
            bookcase.openSeries(index)
        shelf.forceActiveFocus()
    }

    function close() {
        if (bookcase)
            bookcase.closeSeries()
        else
            openedIndex = -1
        wall.forceActiveFocus()
    }

    // Thickness from the number of volumes, on a log curve: the library runs from single
    // volumes to a couple of hundred, and a linear rule would leave almost everything at
    // the thin end with a few absurd slabs.
    function bookWidth(volumes) {
        const v = Math.max(1, Math.min(60, volumes))
        return 12 + 32 * (Math.log(v) / Math.log(60))
    }

    function reset() {
        seriesCount = bookcase ? bookcase.seriesCount() : 0
        filterText = bookcase ? bookcase.filterText() : ""
        wallOffset = 0
        hoveredIndex = -1
        openedIndex = -1

        const perShelf = Math.max(1, Math.ceil(seriesCount / shelfCount))
        const built = []
        for (let r = 0; r < shelfCount; ++r) {
            const offsets = []
            const widths = []
            const first = r * perShelf
            const last = Math.min(seriesCount, first + perShelf)
            let run = 0
            for (let i = first; i < last; ++i) {
                const w = bookWidth(bookcase ? bookcase.volumesAt(i) : 1)
                offsets.push(run)
                widths.push(w)
                run += w
            }
            built.push({ first: first, count: offsets.length, offsets: offsets, widths: widths, total: Math.max(1, run) })
        }
        shelves = built
    }

    // Where the wall has been turned to on one shelf, wrapped into that shelf's own length.
    // A cylinder has no ends, so each run of books joins back to its own beginning.
    function shelfPosition(shelf) {
        if (!shelf || shelf.count === 0)
            return 0
        return ((wallOffset % shelf.total) + shelf.total) % shelf.total
    }

    // The book currently in front of the viewer on a shelf. Everything else on that shelf
    // is placed by counting outwards from this one.
    function centreBook(shelf, position) {
        if (!shelf || shelf.count === 0)
            return 0
        let lo = 0
        let hi = shelf.count - 1
        while (lo < hi) {
            const mid = (lo + hi + 1) >> 1
            if (shelf.offsets[mid] <= position)
                lo = mid
            else
                hi = mid - 1
        }
        return lo
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
            GradientStop { position: 0.5; color: "#1c1a19" }
            GradientStop { position: 1.0; color: "#000000" }
        }
    }

    Item {
        id: scene
        anchors.fill: parent

        Repeater {
            model: wall.shelfCount

            Item {
                id: shelfRow

                required property int index

                readonly property var shelf: index < wall.shelves.length ? wall.shelves[index] : null

                // A run only joins back to its own beginning if it is longer than the arc
                // you can see. Searching narrows the wall to a handful of series, and a
                // shelf of four books wrapped round a cylinder is the same four books
                // repeated twenty times - which is what it looked like.
                readonly property bool wraps: shelf !== null && shelf.total > wall.visibleArc
                readonly property real position: shelf === null ? 0 : (wraps ? wall.shelfPosition(shelf) : shelf.total / 2)
                readonly property int centre: wall.centreBook(shelf, position)
                readonly property real rowOffset: index - (wall.shelfCount - 1) / 2

                anchors.fill: parent

                Repeater {
                    model: wall.slotsPerRow

                    Item {
                        id: slot

                        required property int index

                        readonly property var shelf: shelfRow.shelf
                        readonly property int step: index - Math.floor(wall.slotsPerRow / 2)
                        readonly property int rawIndex: shelfRow.centre + step
                        readonly property bool present: shelf !== null && shelf.count > 0 && (shelfRow.wraps || (rawIndex >= 0 && rawIndex < shelf.count))
                        // Which book on this shelf, wrapping round the cylinder - or, on a
                        // run too short to wrap, simply nothing beyond the ends.
                        readonly property int slotIndex: present ? (shelfRow.wraps ? ((rawIndex % shelf.count) + shelf.count) % shelf.count : rawIndex) : 0
                        readonly property int seriesIndex: present ? shelf.first + slotIndex : -1
                        readonly property real bookWidth: present ? shelf.widths[slotIndex] : 0

                        // Distance from the viewer along the wall, wrapped into the shorter
                        // way round so a book near the join appears on whichever side it is
                        // actually nearest, rather than a whole lap away.
                        readonly property real rawDistance: present ? shelf.offsets[slotIndex] + bookWidth / 2 - shelfRow.position : 0
                        readonly property real distance: present ? (shelfRow.wraps ? rawDistance - shelf.total * Math.round(rawDistance / shelf.total) : rawDistance) : 0

                        readonly property real theta: distance / wall.focal
                        readonly property real cosTheta: Math.cos(theta)
                        // The surface turning away from the viewer.
                        readonly property real squeeze: Math.max(0.04, cosTheta)
                        // Shelf lines splaying apart towards the edges, because at the
                        // sides you are looking along them rather than at them.
                        readonly property real splay: 1 + wall.splayStrength * (1 - cosTheta)

                        // Books are not milled to a common height either. A little variety,
                        // fixed per series so it never shifts, and they stand on the board
                        // rather than floating from a shared centre.
                        readonly property real heightScale: present ? 0.84 + 0.16 * ((seriesIndex * 2654435761 % 997) / 997) : 1
                        readonly property real bookHeight: wall.spineHeight * heightScale
                        readonly property real shelfLine: scene.height / 2 + shelfRow.rowOffset * wall.shelfSpacing * splay + wall.spineHeight / 2

                        // Stopped short of the point where the wall turns edge on. The last
                        // stretch of a cylinder contributes a few pixels of squeezed book at
                        // a third opacity, and costs the splay that was throwing the outer
                        // shelves off the screen. The wall now fades into the dark a little
                        // before the frame, which is what one looks like anyway.
                        // The first book of a section carries the sign for it, and the sign
                        // is wider than the book. Lifting that slot above the rest is what
                        // lets it hang over its neighbours: z applies between siblings, so a
                        // label inside an ordinary slot would be painted over by the next
                        // book along however high its own z was.
                        readonly property bool sectionStart: present && bookcase ? bookcase.startsSectionAt(seriesIndex) : false

                        visible: present && cosTheta > 0.3 && Math.abs(theta) < 1.25
                        width: bookWidth
                        height: bookHeight
                        x: scene.width / 2 + wall.focal * Math.sin(theta) - width / 2
                        y: shelfLine - bookHeight
                        z: Math.round(200 * cosTheta) + (sectionStart ? 400 : 0)
                        opacity: Math.max(0, Math.min(1, 0.25 + 0.75 * cosTheta))

                        transform: Scale {
                            origin.x: slot.width / 2
                            // Horizontal only. The splay moves the line this book stands
                            // on; it must not stretch the book, which was making the ones
                            // at the edges visibly taller than the ones in the middle.
                            origin.y: slot.height
                            xScale: slot.squeeze
                            yScale: 1
                        }

                        // The back of the case, standing behind this book and rising to the
                        // shelf above. Without it the books hang in empty black and the wall
                        // reads as rows of floating spines rather than as a piece of
                        // furniture with depth to it. Drawn first, so the book covers it.
                        Rectangle {
                            width: slot.bookWidth + 2
                            x: -1
                            // Stops short of the shelf above by more than that shelf's board
                            // is thick. Rows are drawn top down, so a panel that reached all
                            // the way up would be painted over the underside of the board it
                            // hangs from.
                            height: wall.shelfSpacing * slot.splay - 18
                            y: slot.height - height
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#070605" }
                                GradientStop { position: 0.72; color: "#130f0b" }
                                GradientStop { position: 1.0; color: "#241d15" }
                            }
                        }

                        // The board this book stands on. Each book draws its own slice, a
                        // shade wider than itself so the slices meet, and the whole reads
                        // as one plank curving away rather than a row of tiles.
                        Rectangle {
                            width: slot.bookWidth + 2
                            height: 13
                            x: -1
                            anchors.top: parent.bottom
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#7d6f60" }
                                GradientStop { position: 0.14; color: "#4a4137" }
                                GradientStop { position: 0.55; color: "#2a241d" }
                                GradientStop { position: 1.0; color: "#100d0a" }
                            }
                        }

                        // The sign for a section, on the back of the case above the books
                        // where a shelf talker goes. Only near the middle of the view: the
                        // wall squeezes what is on it towards the edges, and a name squeezed
                        // to a tenth of its width is a smear rather than a label.
                        Item {
                            visible: slot.sectionStart && slot.cosTheta > 0.72
                            height: 15
                            // Hung from the top of the back panel rather than measured up
                            // from the books, because the gap between the two is whatever
                            // the window height leaves it and a sign measured from below
                            // rises through the shelf above in a short window.
                            y: slot.height - (wall.shelfSpacing * slot.splay - 18) + 2
                            x: -1
                            width: Math.max(60, sectionLabel.implicitWidth + 16)

                            Rectangle {
                                anchors.fill: parent
                                color: "#d8c9a8"
                                radius: 2
                            }

                            Text {
                                id: sectionLabel
                                anchors.centerIn: parent
                                text: slot.sectionStart && bookcase ? bookcase.sectionNameAt(slot.seriesIndex) : ""
                                color: "#241d15"
                                font.pointSize: 7
                                font.bold: true
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 0.8
                            }
                        }

                        // And a divider standing at the start of the section, so the join is
                        // visible even when the sign is too far round the wall to read.
                        Rectangle {
                            visible: slot.sectionStart
                            width: 2
                            x: -2
                            anchors { top: parent.top; bottom: parent.bottom; topMargin: -6 }
                            color: "#a08a5e"
                        }

                        BookSpine {
                            anchors.fill: parent
                            visible: slot.seriesIndex >= 0
                            seriesIndex: Math.max(0, slot.seriesIndex)
                            hovered: wall.hoveredIndex === slot.seriesIndex

                            onEntered: wall.hoveredIndex = slot.seriesIndex
                            onExited: if (wall.hoveredIndex === slot.seriesIndex) wall.hoveredIndex = -1
                            onPicked: wall.open(slot.seriesIndex)
                        }
                    }
                }
            }
        }
    }

    // The series taken off the wall, opened out into its own shelf of volumes. It covers
    // the wall rather than replacing it, and the wall keeps its position underneath, so
    // closing the series puts you back exactly where you were rather than at the beginning.
    VolumeShelf {
        id: shelf

        visible: wall.openedIndex >= 0
        z: 500
        opacity: visible ? 1 : 0
        scale: visible ? 1 : 1.04

        Behavior on opacity { NumberAnimation { duration: 160 } }
        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        onClosed: wall.close()
    }

    // A search that matches nothing otherwise empties the wall to plain black, which looks
    // like a view that has broken rather than an answer.
    Text {
        anchors.centerIn: parent
        z: 400
        visible: wall.seriesCount === 0
        text: wall.filterText.length > 0 ? qsTr("No series matching \"%1\"").arg(wall.filterText) : qsTr("No series here")
        color: typeof bookcaseTextColor !== "undefined" ? bookcaseTextColor : "#ebebeb"
        opacity: 0.5
        font.pointSize: 13
    }

    // Which section you are passing through, while the wall is moving.
    //
    // Turning a wall of nineteen hundred books by dragging it is fast enough that the spine
    // lettering is unreadable while it happens, so there is no way to tell how far you have
    // gone. The section name can be read at any speed the wall will turn at, and it fades
    // out the moment you stop, so it costs nothing when you are looking at the books rather
    // than travelling past them.
    Item {
        anchors.fill: parent
        z: 400
        visible: wall.openedIndex < 0 && wall.seriesCount > 0

        readonly property bool turning: dragArea.pressed || spin.running
        // Re-evaluated as the wall turns: centreSeries reads wallOffset, so these bindings
        // follow it without having to be poked.
        readonly property int centre: wall.centreSeries()
        readonly property string centreTitle: bookcase ? bookcase.titleAt(centre) : ""
        readonly property string section: bookcase ? bookcase.sectionNameAt(centre) : ""

        opacity: turning ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -parent.height * 0.06
            text: parent.section
            color: "#ffffff"
            opacity: 0.12
            font.pointSize: Math.max(28, Math.round(parent.height * 0.13))
            font.bold: true
            font.capitalization: Font.AllUppercase
            font.letterSpacing: Math.round(parent.height * 0.012)
        }

        Text {
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 28 }
            width: parent.width * 0.6
            text: parent.centreTitle
            color: typeof bookcaseTextColor !== "undefined" ? bookcaseTextColor : "#ebebeb"
            opacity: 0.55
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: 11
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        z: -1
        propagateComposedEvents: true
        // Off entirely while a series is open. It fills the whole view, and leaving it live
        // under the shelf meant a wheel or a drag that the shelf did not take was turning
        // the wall behind it, out of sight.
        enabled: wall.openedIndex < 0

        property real pressX: 0
        property real pressOffset: 0

        onPressed: mouse => {
            pressX = mouse.x
            pressOffset = wall.wallOffset
            wall.forceActiveFocus()
        }

        onPositionChanged: mouse => {
            if (!pressed)
                return
            // Dragging turns the wall directly under the pointer.
            wall.wallOffset = pressOffset + (pressX - mouse.x)
        }

        onWheel: wheel => {
            const step = wheel.angleDelta.y > 0 ? -1 : 1
            spin.to = wall.wallOffset + step * 96
            spin.restart()
        }
    }

    NumberAnimation {
        id: spin
        target: wall
        property: "wallOffset"
        duration: 260
        easing.type: Easing.OutCubic
    }

    focus: true
    Keys.onLeftPressed: { spin.to = wall.wallOffset - 32; spin.restart() }
    Keys.onRightPressed: { spin.to = wall.wallOffset + 32; spin.restart() }
    // Only taken when there is something to put back. Accepting it unconditionally swallowed
    // the key the window uses to leave fullscreen, so from this view there was no way out.
    Keys.onEscapePressed: event => {
        if (wall.openedIndex >= 0) {
            wall.close()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    Keys.onReturnPressed: if (wall.openedIndex < 0) wall.open(wall.hoveredIndex >= 0 ? wall.hoveredIndex : wall.centreSeries())

    // Whichever series is dead centre on the middle shelf, for opening one from the keyboard.
    function centreSeries() {
        const middle = Math.floor(shelfCount / 2)
        const s = middle < shelves.length ? shelves[middle] : null
        if (!s || s.count === 0)
            return -1
        // Same rule the shelves themselves use: a run too short to wrap is simply centred.
        const position = s.total > visibleArc ? shelfPosition(s) : s.total / 2
        return s.first + centreBook(s, position)
    }
}
