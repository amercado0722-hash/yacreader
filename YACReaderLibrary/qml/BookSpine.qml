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

    Rectangle {
        id: board
        anchors.fill: parent
        color: spine.hovered ? Qt.lighter(spine.baseColor, 1.25) : spine.baseColor

        // Cloth catches the light down one edge and loses it at the other. Without this
        // the spines are flat swatches and the wall looks printed.
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 0.18; color: "#26ffffff" }
                GradientStop { position: 0.62; color: "#00000000" }
                GradientStop { position: 1.0; color: "#59000000" }
            }
        }

        // The bands top and bottom that almost every printed spine has.
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 9 }
            height: 2
            color: "#40000000"
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 9 }
            height: 2
            color: "#40000000"
        }

        // The title runs up the spine, which is how it is printed and, usefully, the only
        // way a name fits on something thirty pixels wide.
        Text {
            id: label
            width: parent.height - 26
            height: parent.width
            anchors.centerIn: parent
            rotation: -90
            text: spine.title
            elide: Text.ElideRight
            maximumLineCount: 1
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: spine.baseColor.hslLightness > 0.55 ? "#1a1713" : "#f2ede4"
            font.pointSize: 7
            font.bold: true
        }
    }

    // A thin sliver of the pages, so a spine reads as the edge of an object rather than a
    // painted stripe.
    Rectangle {
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 2; bottomMargin: 2 }
        width: 2
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 1.0; color: "#66d9cfb8" }
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
