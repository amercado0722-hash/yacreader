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

    FolderCover {
        id: coverElement
        width: coverWidth
        height: coverHeight
        anchors { horizontalCenter: parent.horizontalCenter; top: realCell.top }
        coverSource: cell.cover_path
        selected: cell.selected
        showFinishedMark: cell.is_finished && show_marks
        showRecentIndicator: (((new Date() / 1000) - cell.added_date) < cell.recent_range
                              || ((new Date() / 1000) - cell.updated) < cell.recent_range)
                             && cell.show_recent
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

    Text {
        z: 4
        anchors { top: coverElement.bottom; left: realCell.left; leftMargin: 4; rightMargin: 4; topMargin: 10 }
        width: itemWidth - 8
        maximumLineCount: 2
        wrapMode: Text.WordWrap
        text: cell.title
        elide: Text.ElideRight
        color: itemTitleColor
        font.letterSpacing: fontSpacing
        font.pointSize: fontSize
        font.family: fontFamily
    }
}
