import QtQuick

// Delegate for GridContentModel rows of kind ExpandedComicItem: the volumes of a folder
// the user opened in place. Required properties intentionally match the model role names.
//
// It is deliberately simpler than ComicGridDelegate. These rows have no row in the comic
// model, so none of the selection, rating or drag machinery that delegate relies on can
// be used here; it offers opening and nothing that would silently act on the wrong comic.
Rectangle {
    id: cell

    required property int index
    required property string title
    required property url cover_path
    required property var volume_label
    required property var read_column
    required property var has_been_opened

    signal openRequested()
    signal focusRequested()

    property alias interactionItem: realCell

    color: "transparent"

    scale: mouseArea.containsMouse ? 1.03 : 1
    Behavior on scale { NumberAnimation { duration: 90 } }

    Rectangle {
        id: realCell
        width: itemWidth
        height: itemHeight
        color: "transparent"
        anchors.horizontalCenter: parent.horizontalCenter

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true

            onDoubleClicked: cell.openRequested()
            onPressed: cell.focusRequested()
        }
    }

    // Matches the fore edge on the tiles these sit among, so an expanded volume reads
    // as the same kind of object as the ones around it.
    readonly property real pageEdge: 4

    Rectangle {
        width: cell.pageEdge + 1
        anchors { left: coverElement.right; top: coverElement.top; bottom: coverElement.bottom; topMargin: 2; bottomMargin: 2 }
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#e6ddc9" }
            GradientStop { position: 0.4; color: "#cbc2ad" }
            GradientStop { position: 1.0; color: "#8d8674" }
        }
        opacity: cell.read_column === true ? 0.5 : 1
    }

    Item {
        id: coverElement
        width: coverWidth - cell.pageEdge
        height: coverHeight
        anchors { horizontalCenter: parent.horizontalCenter; horizontalCenterOffset: -cell.pageEdge / 2; top: realCell.top }

        Image {
            id: coverImage
            anchors.fill: parent
            source: cell.cover_path
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            sourceSize.width: coverWidth * 2
        }

        // A volume that has been read is dimmed, so gaps in a series stand out while
        // scanning rather than needing to be read off each tile.
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: cell.read_column === true ? 0.55 : 0
            visible: opacity > 0
        }

        Rectangle {
            id: volumeBadge
            visible: String(cell.volume_label).length > 0
            anchors { left: parent.left; top: parent.top; leftMargin: 4; topMargin: 4 }
            width: Math.max(22, badgeText.implicitWidth + 10)
            height: 20
            radius: 4
            color: "#c0000000"
            border.width: 1
            border.color: "#40ffffff"

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: String(cell.volume_label)
                color: "#ffffff"
                font.pointSize: fontSize
                font.family: fontFamily
                font.bold: true
            }
        }

        Rectangle {
            visible: cell.has_been_opened === true && cell.read_column !== true
            anchors { right: parent.right; top: parent.top; rightMargin: 4; topMargin: 4 }
            width: 10
            height: 10
            radius: 5
            color: "#ffb300"
        }
    }

    Text {
        z: 4
        anchors { top: coverElement.bottom; left: realCell.left; leftMargin: 4; rightMargin: 4; topMargin: 10 }
        width: itemWidth - 8
        maximumLineCount: 2
        wrapMode: Text.WordWrap
        text: cell.title
        elide: Text.ElideRight
        color: itemTitleColor
        opacity: 0.75
        font.letterSpacing: fontSpacing
        font.pointSize: fontSize
        font.family: fontFamily
    }
}
