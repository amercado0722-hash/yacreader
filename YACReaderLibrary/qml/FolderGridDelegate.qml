import QtQuick

// Delegate for GridContentModel folder rows; required properties intentionally match its role names.
Rectangle {
    id: cell

    required property int index
    required property string title
    required property url cover_path
    required property double added_date
    required property double updated
    required property double recent_range
    required property bool show_recent
    required property bool is_finished
    required property bool selected
    required property bool is_expanded
    required property int num_children

    signal openRequested()
    signal expandRequested()
    signal contextMenuRequested(point localPosition)
    signal focusRequested()

    property alias interactionItem: realCell

    color: "transparent"

    scale: mouseArea.containsMouse ? 1.025 : 1
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
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true

            onDoubleClicked: cell.openRequested()
            onPressed: mouse => {
                cell.focusRequested()
                if (mouse.button === Qt.RightButton) {
                    cell.contextMenuRequested(Qt.point(mouseX, mouseY))
                    mouse.accepted = false
                }
            }
        }
    }

    // Books stand on the shelf, so their bases line up and their tops do not. Six percent
    // of variation is enough to break the ruled line across a row that makes a grid read
    // as a spreadsheet. Derived from the title so a series is the same height every time
    // the library is opened, rather than jumping about between sessions.
    readonly property real heightVariation: {
        var name = String(cell.title)
        var hash = 0
        for (var i = 0; i < name.length; i++)
            hash = (hash * 31 + name.charCodeAt(i)) % 9973
        return 1 - (hash % 7) / 100
    }

    // The line every book in the row stands on.
    Item {
        id: shelfLine
        height: 0
        anchors { left: realCell.left; right: realCell.right; top: realCell.top; topMargin: coverHeight }
    }

    ShelfBoard {
        id: board
        z: 3
        height: 26
        label: cell.title
        anchors { left: parent.left; right: parent.right; top: shelfLine.top; topMargin: 3 }
    }

    // Where the book meets the board. Darkest at the join and gone within a few pixels,
    // which is what stops it looking like it is hovering a little above the shelf.
    Rectangle {
        z: 4
        height: 5
        anchors { horizontalCenter: coverElement.horizontalCenter; top: shelfLine.top; topMargin: 3 }
        width: coverElement.width + 6
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#b0000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }
    }

    FolderCover {
        id: coverElement
        width: coverWidth
        height: Math.round(coverHeight * cell.heightVariation)
        anchors { horizontalCenter: parent.horizontalCenter; bottom: shelfLine.top }
        coverSource: cell.cover_path
        volumeCount: cell.num_children
        selected: cell.selected
        showShelfShadow: false
        showFinishedMark: cell.is_finished && show_marks
        showRecentIndicator: (((new Date() / 1000) - cell.added_date) < cell.recent_range
                              || ((new Date() / 1000) - cell.updated) < cell.recent_range)
                             && cell.show_recent
    }

    CoverLighting {
        z: 2
        anchors.fill: coverElement
        anchors.rightMargin: coverElement.coverInset
    }

    // Disclosure control. Deliberately a separate hit target from the cover so that
    // double-click-to-open keeps working exactly as it did.
    Rectangle {
        id: expander
        z: 6
        width: 26
        height: 26
        radius: 13
        anchors { right: coverElement.right; bottom: coverElement.bottom; rightMargin: 6; bottomMargin: 6 }
        color: expanderMouseArea.containsMouse ? "#e0000000" : "#a0000000"
        border.width: 1
        border.color: "#40ffffff"
        opacity: mouseArea.containsMouse || expanderMouseArea.containsMouse || cell.is_expanded ? 1 : 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 120 } }

        Text {
            anchors.centerIn: parent
            text: cell.is_expanded ? "✕" : "▾"
            color: "#ffffff"
            font.pointSize: cell.is_expanded ? 9 : 12
        }

        MouseArea {
            id: expanderMouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            onClicked: cell.expandRequested()
        }
    }

}
