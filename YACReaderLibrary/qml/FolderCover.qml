import QtQuick
import QtQuick.Effects

Item {
    id: root

    required property url coverSource
    property bool selected: false
    property bool showRecentIndicator: false
    property bool showFinishedMark: false
    property real cornerRadius: 10

    Rectangle {
        anchors.fill: parent
        transform: Rotation { origin.x: root.width / 2; origin.y: root.height / 2; angle: -4 }
        radius: root.cornerRadius
        color: placeholderFolder1Color
        border.color: placeholderFolder1BorderColor
        border.width: 1
    }

    Rectangle {
        anchors.fill: parent
        transform: Rotation { origin.x: root.width / 2; origin.y: root.height / 2; angle: 3 }
        radius: root.cornerRadius
        color: placeholderFolder2Color
        border.color: placeholderFolder2BorderColor
        border.width: 1
    }

    Image {
        id: coverImage
        anchors.fill: parent
        source: root.coverSource
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
        asynchronous: true
        cache: true
        visible: false
    }

    Item {
        id: coverMask
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        visible: false

        Rectangle {
            anchors.fill: parent
            radius: root.cornerRadius
            color: "black"
        }
    }

    MultiEffect {
        anchors.fill: coverImage
        source: coverImage
        maskEnabled: true
        maskSource: coverMask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        color: "transparent"
        border.color: folderCoverBorderColor
        border.width: 1
    }

    Rectangle {
        width: 10
        height: 10
        radius: 5
        anchors { left: parent.left; top: parent.top; topMargin: 10; leftMargin: 10 }
        color: newItemColor
        visible: root.showRecentIndicator
    }

    Image {
        z: 2
        width: 23
        height: 23
        source: "tick.svg"
        visible: root.showFinishedMark
        anchors { right: parent.right; top: parent.top; topMargin: 9; rightMargin: 9 }
        asynchronous: true
    }

    Rectangle {
        z: 2
        anchors.fill: parent
        anchors.margins: -3
        radius: root.cornerRadius + 3
        color: "transparent"
        border.color: cellSelectedBorderColor
        border.width: 3
        opacity: root.selected ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 150 } }
    }
}
