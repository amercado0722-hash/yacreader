import QtQuick

// One book, seen edge on.
//
// A comic library stores covers and nothing else, so the spine has to be made up. It is
// made up from the title: the same series is always the same colour, and the shelf ends up
// with the band of unrelated colours a real shelf has, rather than a gradient someone
// chose.
Item {
    id: spine

    required property int seriesIndex
    property bool hovered: false

    signal entered()
    signal exited()
    signal picked()

    readonly property color baseColor: bookcase ? bookcase.spineColorAt(seriesIndex) : "#5a5a60"
    readonly property string title: bookcase ? bookcase.titleAt(seriesIndex) : ""
    readonly property int volumes: bookcase ? bookcase.volumesAt(seriesIndex) : 0
    readonly property bool light: baseColor.hslLightness > 0.52

    // How far through it you are, and whether the library knows what it is. A shelf where a
    // series you have read looks exactly like one you have never opened, and where the ones
    // still unidentified after a scrape are indistinguishable from the rest, is a picture of
    // a library rather than one you use.
    readonly property int readState: bookcase ? bookcase.readStateAt(seriesIndex) : 0
    readonly property bool started: readState === 1
    readonly property bool read: readState === 2
    readonly property bool identified: bookcase ? bookcase.isIdentifiedAt(seriesIndex) : true

    Rectangle {
        id: board
        anchors.fill: parent
        color: spine.hovered ? Qt.lighter(spine.baseColor, 1.3) : spine.baseColor

        // Cloth catches the light down one edge and loses it at the other. Without this
        // the spines are flat swatches and the wall looks printed.
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#33000000" }
                GradientStop { position: 0.20; color: "#2effffff" }
                GradientStop { position: 0.60; color: "#00000000" }
                GradientStop { position: 1.0; color: "#6b000000" }
            }
        }

        // Boards are cut and covered before they are printed on, so the head and tail of a
        // spine are darker than its face.
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 3
            color: "#4d000000"
        }

        // The bands top and bottom that almost every printed spine has.
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 10 }
            height: 2
            color: spine.light ? "#33000000" : "#2effffff"
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 10 }
            height: 2
            color: spine.light ? "#33000000" : "#2effffff"
        }

        // A series you have read through is a book that has been off the shelf and back: it
        // sits a shade further into the shadow than the ones either side of it, and carries
        // the mark a reader would actually put on a book - a small pale label near the head
        // of the spine.
        Rectangle {
            anchors.fill: parent
            visible: spine.read && !spine.hovered
            color: "#59100e0c"
        }
        Rectangle {
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 16 }
            visible: spine.read
            width: Math.max(3, spine.width - 8)
            height: Math.max(4, Math.round(spine.width * 0.30))
            radius: 1
            color: spine.light ? "#cc2f2a22" : "#d9e8dfc6"
        }

        // One you are partway through gets the thing that would actually be sticking out of
        // it: a ribbon over the head of the spine, in a colour nothing else on the wall uses.
        // The series you are in the middle of is the one you are most often looking for, so
        // it is the loudest mark here and the rarest.
        Rectangle {
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
            visible: spine.started
            width: Math.max(2, Math.round(spine.width * 0.34))
            height: 15
            color: "#e0574a"
        }

        // Nothing known about it but its file names. A quarter of this library is still in
        // that state after a scrape, so this is deliberately the quietest mark here: a notch
        // at the foot, enough to pick out when you go looking for them and easy to ignore
        // when you are not.
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 4; leftMargin: 3; rightMargin: 4 }
            visible: !spine.identified
            height: 2
            color: spine.light ? "#40000000" : "#3dffffff"
        }

        // The title runs up the spine, which is how it is printed and, usefully, the only
        // way a name fits on something thirty pixels wide. A thin enough book has no room
        // for lettering at all, which is also true of thin enough books.
        Text {
            id: label
            width: parent.height - 28
            height: parent.width
            anchors.centerIn: parent
            rotation: -90
            visible: spine.width >= 17
            text: spine.title
            elide: Text.ElideRight
            maximumLineCount: 1
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: spine.light ? "#1a1713" : "#efe8dc"
            font.pointSize: spine.width >= 30 ? 7 : 6
            font.bold: true
        }
    }

    // A thin sliver of the pages, so a spine reads as the edge of an object rather than a
    // painted stripe.
    Rectangle {
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 2; bottomMargin: 1 }
        width: Math.max(1, Math.round(spine.width * 0.09))
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 1.0; color: "#59d9cfb8" }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: spine.entered()
        onExited: spine.exited()
        onClicked: spine.picked()
    }
}
