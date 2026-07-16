import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    signal passwordSubmitted(string password)

    property bool retry: false

    function requestPassword(isRetry) {
        root.retry = isRetry
        passwordField.clear()
        root.open()
        Qt.callLater(function() {
            passwordField.forceActiveFocus()
        })
    }

    function submit() {
        if (passwordField.text.length === 0) {
            return
        }
        root.passwordSubmitted(passwordField.text)
        root.close()
    }

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: Math.min(420,
                    Math.max(300,
                             (parent ? parent.width : 452) - Theme.spaceXl))
    padding: Theme.spaceLg
    modal: true
    dim: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    Overlay.modal: Rectangle {
        color: Theme.darkMode ? "#99000000" : "#66000000"
    }

    background: Rectangle {
        color: Theme.surfaceColor
        radius: Theme.radiusLg
        border.color: Theme.borderColor
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: Theme.spaceMd

        Label {
            Layout.fillWidth: true
            text: qsTr("Protected PDF")
            color: Theme.textColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.titleFontSize
            font.weight: Font.DemiBold
        }

        Label {
            Layout.fillWidth: true
            text: root.retry
                  ? qsTr("The password is incorrect. Try again.")
                  : qsTr("Enter the password to open this document.")
            color: root.retry ? Theme.textColor : Theme.mutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.bodyFontSize
            wrapMode: Text.Wrap
        }

        SZHTextField {
            id: passwordField

            Layout.fillWidth: true
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText
            onAccepted: root.submit()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceXs

            Item {
                Layout.fillWidth: true
            }

            SZHButton {
                text: qsTr("Cancel")
                variant: "secondary"
                onClicked: root.close()
            }

            SZHButton {
                text: qsTr("Open")
                enabled: passwordField.text.length > 0
                onClicked: root.submit()
            }
        }
    }

    onClosed: passwordField.clear()
}
