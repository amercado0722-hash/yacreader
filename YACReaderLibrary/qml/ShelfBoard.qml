import QtQuick

// The board a row of books stands on.
//
// Each delegate draws its own slice, spanning the full width of its grid cell. Adjacent
// cells touch, so the slices join into one unbroken shelf running across the row without
// the view itself having to know that shelves exist - which matters, because rows here
// are made of folders, comics and spacers mixed together.
//
// The series name is printed on the board rather than floating under the cover. A caption
// hanging in space is a museum label; lettering on the shelf edge is what a shelf has.
Item {
    id: board

    property string label: ""
    property color labelColor: "#9a938a"
    property real labelSize: 8
    property real edgeHeight: 4

    // Lit front edge. A single brighter line along the top is most of what tells you a
    // horizontal band is a surface you are looking slightly down at - but it has to be
    // bright enough to read as timber catching the light. Kept too close to the background
    // it did the opposite, and the row of boards looked like the gaps between the rows.
    Rectangle {
        id: litEdge
        height: board.edgeHeight
        anchors { left: parent.left; right: parent.right; top: parent.top }
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#9a8a76" }
            GradientStop { position: 0.45; color: "#6d6155" }
            GradientStop { position: 1.0; color: "#463d33" }
        }
    }

    // The face of the board, falling away from the light.
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: litEdge.bottom; bottom: parent.bottom }
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#3b342b" }
            GradientStop { position: 0.6; color: "#252019" }
            GradientStop { position: 1.0; color: "#171310" }
        }

        Text {
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 8; rightMargin: 8 }
            horizontalAlignment: Text.AlignHCenter
            text: board.label
            color: board.labelColor
            elide: Text.ElideRight
            maximumLineCount: 1
            font.pointSize: board.labelSize
            font.letterSpacing: 0.3
            visible: board.label.length > 0
        }
    }

    // Underside. Without a shadow beneath it the board reads as a stripe painted on the
    // background rather than a plank with air under it.
    Rectangle {
        height: 9
        anchors { left: parent.left; right: parent.right; top: parent.bottom }
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#8c000000" }
            GradientStop { position: 0.5; color: "#2e000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }
    }
}
