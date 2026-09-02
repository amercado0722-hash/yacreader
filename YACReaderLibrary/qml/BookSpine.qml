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
