import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Pdf

ApplicationWindow {
    id: root

    width: 960
    height: 720
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: reader.title.length > 0 ? reader.title + qsTr(" - Leaflit") : qsTr("Leaflit")

    property bool darkTheme: false
    property int readerFontSize: 18
    property bool showingPdf: reader.pdfMode

    readonly property color pageColor: darkTheme ? "#161a1d" : "#f7f5ef"
    readonly property color panelColor: darkTheme ? "#20262a" : "#ffffff"
    readonly property color textColor: darkTheme ? "#edf2ef" : "#202124"
    readonly property color mutedColor: darkTheme ? "#aab4ad" : "#686f69"
    readonly property color lineColor: darkTheme ? "#333b40" : "#ded9cf"
    readonly property color accentColor: "#2f8f5b"

    color: pageColor

    FileDialog {
        id: openDialog

        title: qsTr("Open book")
        nameFilters: [
            qsTr("Supported books (*.txt *.fb2 *.epub *.pdf *.html *.htm *.md *.markdown *.docx)"),
            qsTr("Text files (*.txt *.md *.markdown *.html *.htm)"),
            qsTr("Electronic books (*.fb2 *.epub)"),
            qsTr("PDF files (*.pdf)"),
            qsTr("Word documents (*.docx)")
        ]

        onAccepted: reader.openFile(selectedFile)
    }

    PdfDocument {
        id: pdfDocument

        source: root.showingPdf ? reader.pdfSource : ""
    }

    Connections {
        target: reader

        function onPdfSourceChanged() {
            pdfViewer.renderScale = 1
        }
    }

    header: ToolBar {
        implicitHeight: 56

        background: Rectangle {
            color: root.panelColor
            border.color: root.lineColor
            border.width: 1
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Label {
                text: qsTr("Leaflit")
                color: root.textColor
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Button {
                text: qsTr("Open")
                onClicked: openDialog.open()
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                text: qsTr("-")
                enabled: root.showingPdf ? pdfViewer.renderScale > 0.4 : root.readerFontSize > 12
                onClicked: {
                    if (root.showingPdf) {
                        pdfViewer.renderScale = Math.max(0.4, pdfViewer.renderScale - 0.1)
                    } else {
                        root.readerFontSize -= 1
                    }
                }
            }

            Label {
                text: root.showingPdf ? Math.round(pdfViewer.renderScale * 100) + qsTr("%") :
                                        root.readerFontSize + qsTr(" px")
                color: root.mutedColor
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: 64
            }

            ToolButton {
                text: qsTr("+")
                enabled: root.showingPdf ? pdfViewer.renderScale < 3 : root.readerFontSize < 36
                onClicked: {
                    if (root.showingPdf) {
                        pdfViewer.renderScale = Math.min(3, pdfViewer.renderScale + 0.1)
                    } else {
                        root.readerFontSize += 1
                    }
                }
            }

            ToolButton {
                text: qsTr("Fit")
                visible: root.showingPdf
                onClicked: pdfViewer.scaleToWidth(pdfHost.width - 32, pdfHost.height)
            }

            Switch {
                text: qsTr("Dark")
                checked: root.darkTheme
                onToggled: root.darkTheme = checked
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 34
        color: root.panelColor
        border.color: root.lineColor
        border.width: 1

        Label {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            text: reader.errorMessage.length > 0 ? reader.errorMessage :
                  reader.sourcePath.length > 0 ? reader.formatName + qsTr(" - ") + reader.sourcePath : qsTr("No file open")
            color: reader.errorMessage.length > 0 ? "#c84c4c" : root.mutedColor
            elide: Text.ElideMiddle
            verticalAlignment: Text.AlignVCenter
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.pageColor

        ScrollView {
            anchors.fill: parent
            anchors.margins: 20
            clip: true
            visible: !root.showingPdf

            TextArea {
                id: readerText

                readOnly: true
                selectByMouse: true
                text: reader.text
                wrapMode: TextEdit.Wrap
                color: root.textColor
                placeholderText: qsTr("No file open")
                placeholderTextColor: root.mutedColor
                font.pixelSize: root.readerFontSize
                leftPadding: 24
                rightPadding: 24
                topPadding: 22
                bottomPadding: 22

                background: Rectangle {
                    color: root.panelColor
                    border.color: root.lineColor
                    radius: 8
                }
            }
        }

        Rectangle {
            id: pdfHost

            anchors.fill: parent
            anchors.margins: 20
            visible: root.showingPdf
            color: root.panelColor
            border.color: root.lineColor
            radius: 8
            clip: true

            PdfMultiPageView {
                id: pdfViewer

                anchors.fill: parent
                anchors.margins: 12
                document: pdfDocument
            }
        }
    }
}
