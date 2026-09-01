import QtQuick
import QtQuick.Effects

Item {
    id: root

    required property url coverSource
    // Covers of the stack, from front to back. The first entry is the front
    // cover, so only the two next ones are stacked behind it. Used when several
    // comics are selected at once, where a scatter of the actual covers is the
    // point; a series in the grid leaves this empty and gets the book below.
    property var stackedCoverSources: []
    // How many comics the folder holds. A series is a physical object with a
    // thickness, and a shelf where a five volume run and a forty volume run look
    // identical tells you nothing you can see.
    property int volumeCount: 0
    property bool selected: false
    property bool showRecentIndicator: false
    property bool showFinishedMark: false
    property bool showShelfShadow: true
    property real cornerRadius: 10

    readonly property int stackedCount: stackedCoverSources.length
    readonly property url midCoverSource: stackedCount > 1 ? stackedCoverSources[1] : ""
    readonly property url backCoverSource: stackedCount > 2 ? stackedCoverSources[2] : ""

    readonly property bool showsBook: stackedCount === 0 && volumeCount > 0

    // Logarithmic, because volume counts are not spread evenly: half of a library
    // sits under five volumes while the longest runs reach into the hundreds, so a
    // linear depth would leave almost everything looking identically thin.
    readonly property real pageDepth: {
        if (!showsBook || volumeCount <= 1)
            return 0
        var capped = Math.min(volumeCount, 60)
        return 3 + 13 * (Math.log(capped) / Math.log(60))
    }

    readonly property real coverInset: showsBook ? pageDepth : 0

    // A cover image with rounded corners and an outline. The mask is inset by
    // one pixel so that the alpha fades out inside the item, which keeps the
    // edges smooth when the cover is rotated.
    component RoundedCover: Item {
        id: cover

        required property url coverSource
        property real cornerRadius: 10
        property color outlineColor: "transparent"

        Image {
            id: coverImage
            anchors.fill: parent
            source: cover.coverSource
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
                anchors.margins: 1
                radius: cover.cornerRadius
                color: "black"
            }
        }

        MultiEffect {
            anchors.fill: parent
            source: coverImage
            maskEnabled: true
            maskSource: coverMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Rectangle {
            anchors.fill: parent
            radius: cover.cornerRadius
            color: "transparent"
            border.color: cover.outlineColor
            border.width: 1
        }
    }

    // The shadow a book casts on the shelf it is standing on. Pooled underneath
    // rather than spread evenly around the cover, because an even glow reads as a
    // card floating above the page instead of an object resting on something.
    Rectangle {
        // Only where a book is being drawn. Where the component is showing a scatter of
        // several selected covers instead, a shadow under one edge of it means nothing.
        visible: root.showShelfShadow && root.showsBook
        z: -2
        height: 7
        anchors {
            left: parent.left
            right: parent.right
            top: parent.bottom
            topMargin: -2
            leftMargin: 3
            rightMargin: 1
        }
        radius: 3
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#66000000" }
            GradientStop { position: 0.45; color: "#26000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }
    }

    // ---- the page block: what you actually see of a book that is not its cover

    Rectangle {
        id: pageBlock
        visible: root.showsBook && root.pageDepth > 0
        z: -1
        width: root.pageDepth + 2
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            topMargin: 4
            bottomMargin: 4
        }
        radius: 2
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#e6ddc9" }
            GradientStop { position: 0.35; color: "#cfc6b1" }
            GradientStop { position: 1.0; color: "#8d8674" }
        }

        // Individual leaves. Without these the block is a beige bar; with them it
        // reads as paper at a glance, even at thumbnail size.
        Repeater {
            model: Math.max(0, Math.min(7, Math.floor(root.pageDepth / 2)))

            Rectangle {
                required property int index
                width: 1
                color: "#40000000"
                anchors { top: parent.top; bottom: parent.bottom; topMargin: 1; bottomMargin: 1 }
                x: 2 + index * ((pageBlock.width - 3) / Math.max(1, Math.min(7, Math.floor(root.pageDepth / 2))))
            }
        }
    }

    // ---- the scattered stack, for a multiple selection rather than a series

    Rectangle {
        anchors.fill: parent
        rotation: -4
        radius: root.cornerRadius
        color: placeholderFolder1Color
        border.color: placeholderFolder1BorderColor
        border.width: 1
        visible: !root.showsBook && root.backCoverSource.toString().length === 0
    }

    RoundedCover {
        anchors.fill: parent
        rotation: -4
        opacity: 0.5
        coverSource: root.backCoverSource
        cornerRadius: root.cornerRadius
        outlineColor: placeholderFolder1BorderColor
        visible: root.backCoverSource.toString().length > 0
    }

    Rectangle {
        anchors.fill: parent
        rotation: 3
        radius: root.cornerRadius
        color: placeholderFolder2Color
        border.color: placeholderFolder2BorderColor
        border.width: 1
        visible: !root.showsBook && root.midCoverSource.toString().length === 0
    }

    RoundedCover {
        anchors.fill: parent
        rotation: 3
        opacity: 0.75
        coverSource: root.midCoverSource
        cornerRadius: root.cornerRadius
        outlineColor: placeholderFolder2BorderColor
        visible: root.midCoverSource.toString().length > 0
    }

    // ---- the front cover, inset to leave the page block showing

    RoundedCover {
        id: frontCover
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
            right: parent.right
            rightMargin: root.coverInset
        }
        coverSource: root.coverSource
        cornerRadius: root.cornerRadius
        outlineColor: folderCoverBorderColor
    }

    Rectangle {
        width: 10
        height: 10
        radius: 5
        anchors { left: frontCover.left; top: frontCover.top; topMargin: 10; leftMargin: 10 }
        color: newItemColor
        visible: root.showRecentIndicator
    }

    Image {
        z: 2
        width: 23
        height: 23
        source: "tick.svg"
        visible: root.showFinishedMark
        anchors { right: frontCover.right; top: frontCover.top; topMargin: 9; rightMargin: 9 }
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
