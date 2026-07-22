import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var book

    signal openRequested(url sourceUrl)
    signal relinkRequested(url sourceUrl)

    implicitHeight: 116
    color: Theme.surfaceColor
    radius: Theme.radiusMd
    border.color: Theme.borderColor
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceSm
        spacing: Theme.spaceMd

        LibraryBookCover {
            Layout.preferredWidth: 62
            Layout.fillHeight: true
            title: root.book.title || ""
            formatName: root.book.formatName || ""
            coverUrl: root.book.coverUrl || ""
            fallbackColor: "#222222"
            compact: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2xs

            Label {
                Layout.fillWidth: true
                text: root.book.title || ""
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyLargeFontSize
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.book.author || ""
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideRight
            }

            Label {
                text: root.book.cloudPlaceholder
                      ? qsTr("Online-only  \u00b7  Downloads when opened")
                      : root.book.fileAvailable
                        ? (root.book.formatName || "")
                          + qsTr("  \u00b7  ")
                          + Math.round((root.book.progress || 0) * 100)
                          + qsTr("%")
                        : qsTr("File unavailable")
                color: root.book.fileAvailable ? Theme.mutedTextColor : Theme.dangerColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
            }

            SZHProgressBar {
                visible: root.book.fileAvailable || false
                Layout.preferredWidth: Math.min(320, parent.width)
                value: root.book.progress || 0
                accessibleName: qsTr("Reading progress")
            }
        }

        SZHButton {
            text: root.book.fileAvailable ? qsTr("Continue") : qsTr("Locate")
            variant: root.book.fileAvailable ? "primary" : "secondary"
            onClicked: {
                if (root.book.fileAvailable) {
                    root.openRequested(root.book.sourceUrl)
                } else {
                    root.relinkRequested(root.book.sourceUrl)
                }
            }
        }
    }
}
