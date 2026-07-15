pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var libraryModel
    required property string errorMessage

    signal addRequested
    signal openRequested(url sourceUrl)

    function coverColor(row) {
        const colors = ["#315f43", "#914a3b", "#9a7225", "#536b85", "#6f5b7d", "#57705a"]
        return colors[row % colors.length]
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowColor

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionNormal
            }
        }
    }

    RowLayout {
        id: libraryToolbar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Theme.spaceLg
        anchors.leftMargin: root.width < 900 ? Theme.spaceLg : Theme.spaceXl
        anchors.rightMargin: root.width < 900 ? Theme.spaceLg : Theme.spaceXl
        spacing: Theme.spaceMd

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2xs

            Label {
                text: qsTr("Library")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.displayFontSize
                font.weight: Font.DemiBold
            }

            Label {
                text: qsTr("%n book(s)", "", root.libraryModel.totalCount)
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
            }
        }

        LeaflitTextField {
            Layout.preferredWidth: root.width < 900 ? 220 : 300
            placeholderText: qsTr("Search library")
            text: root.libraryModel.filterText
            onTextEdited: root.libraryModel.filterText = text
        }

        LeaflitButton {
            text: qsTr("Add book")
            symbol: "+"
            onClicked: root.addRequested()
        }
    }

    Label {
        id: errorLabel

        anchors.top: libraryToolbar.bottom
        anchors.left: libraryToolbar.left
        anchors.right: libraryToolbar.right
        anchors.topMargin: Theme.spaceSm
        visible: root.errorMessage.length > 0
        text: root.errorMessage
        color: Theme.dangerColor
        font.family: Theme.uiFontFamily
        font.pixelSize: Theme.captionFontSize
        elide: Text.ElideRight
    }

    GridView {
        id: bookGrid

        readonly property real availableGridWidth: Math.max(1, width - leftMargin - rightMargin)
        readonly property int columnCount: Math.max(1, Math.floor(availableGridWidth / 188))

        anchors.top: errorLabel.visible ? errorLabel.bottom : libraryToolbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: Theme.spaceLg
        leftMargin: root.width < 900 ? Theme.spaceLg : Theme.spaceXl
        rightMargin: root.width < 900 ? Theme.spaceLg : Theme.spaceXl
        bottomMargin: Theme.spaceLg
        clip: true
        model: root.libraryModel
        cellWidth: Math.floor(availableGridWidth / columnCount)
        cellHeight: 286
        boundsBehavior: Flickable.StopAtBounds
        visible: root.libraryModel.count > 0
        ScrollBar.vertical: LeaflitScrollBar {
        }

        header: Item {
            width: bookGrid.availableGridWidth
            height: root.libraryModel.filterText.length === 0 ? 218 : 52

            ColumnLayout {
                anchors.fill: parent
                anchors.bottomMargin: Theme.spaceLg
                spacing: Theme.spaceSm

                Label {
                    visible: root.libraryModel.filterText.length === 0
                    text: qsTr("Continue reading")
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyLargeFontSize
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    id: continueCard

                    readonly property var book: root.libraryModel.recentBook

                    visible: root.libraryModel.filterText.length === 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 132
                    color: Theme.surfaceColor
                    radius: Theme.radiusMd
                    border.color: Theme.borderColor
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spaceMd
                        spacing: Theme.spaceMd

                        Rectangle {
                            Layout.preferredWidth: 68
                            Layout.fillHeight: true
                            color: root.coverColor(0)
                            radius: Theme.radiusSm

                            Label {
                                anchors.centerIn: parent
                                width: parent.width - Theme.spaceSm * 2
                                text: continueCard.book.title || ""
                                color: "#ffffff"
                                font.family: Theme.readingFontFamily
                                font.pixelSize: Theme.captionFontSize
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                maximumLineCount: 4
                                elide: Text.ElideRight
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spaceXs

                            Label {
                                Layout.fillWidth: true
                                text: continueCard.book.title || ""
                                color: Theme.textColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.bodyLargeFontSize
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Label {
                                text: (continueCard.book.formatName || "")
                                      + qsTr("  \u00b7  ")
                                      + Math.round((continueCard.book.progress || 0) * 100)
                                      + qsTr("%")
                                color: Theme.mutedTextColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.captionFontSize
                            }

                            LeaflitProgressBar {
                                Layout.preferredWidth: Math.min(320, parent.width)
                                value: continueCard.book.progress || 0
                            }
                        }

                        LeaflitButton {
                            text: qsTr("Continue")
                            enabled: continueCard.book.fileAvailable || false
                            onClicked: root.openRequested(continueCard.book.sourceUrl)
                        }
                    }
                }

                Label {
                    text: root.libraryModel.filterText.length === 0
                          ? qsTr("All books")
                          : qsTr("Search results")
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyLargeFontSize
                    font.weight: Font.DemiBold
                }
            }
        }

        delegate: LibraryBookDelegate {
            width: bookGrid.cellWidth - Theme.spaceSm
            height: bookGrid.cellHeight - Theme.spaceSm
            coverColor: root.coverColor(index)
            onOpenRequested: sourceUrl => root.openRequested(sourceUrl)
            onRemoveRequested: row => root.libraryModel.removeBook(row)
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(420, root.width - Theme.space2xl * 2)
        visible: root.libraryModel.totalCount === 0
        spacing: Theme.spaceMd

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Your library is empty")
            color: Theme.textColor
            font.family: Theme.readingFontFamily
            font.pixelSize: Theme.displayFontSize
            font.weight: Font.DemiBold
        }

        LeaflitButton {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Add book")
            symbol: "+"
            onClicked: root.addRequested()
        }
    }

    Label {
        anchors.centerIn: parent
        visible: root.libraryModel.totalCount > 0 && root.libraryModel.count === 0
        text: qsTr("No books found")
        color: Theme.mutedTextColor
        font.family: Theme.uiFontFamily
        font.pixelSize: Theme.bodyLargeFontSize
    }
}
