import QtQuick

// One light source, falling from the left, applied identically to every cover on screen.
//
// This is the difference between a wall of pictures and a shelf of objects: flat-lit
// rectangles read as images of covers, while a consistent highlight and falloff make the
// same rectangles read as things standing in a room. It is two gradients and no shader.
Item {
    id: lighting

    // The contact shadow a book casts on the shelf immediately under it, darkest where
    // the two meet. Drawn by the caller rather than here, because it belongs to the board.
    property real highlightOpacity: 0.10
    property real shadeOpacity: 0.20

    Rectangle {
        width: Math.max(6, parent.width * 0.14)
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, lighting.highlightOpacity) }
            GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0) }
        }
    }

    Rectangle {
        width: Math.max(8, parent.width * 0.22)
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, lighting.shadeOpacity) }
        }
    }
}
