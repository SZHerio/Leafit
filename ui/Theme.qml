pragma Singleton

import QtQuick

QtObject {
    property string colorTheme: "light"

    readonly property bool darkMode: colorTheme === "dark"
    readonly property bool sepiaMode: colorTheme === "sepia"

    readonly property string uiFontFamily: Qt.platform.os === "windows"
                                                   ? "Segoe UI Variable"
                                                   : "sans-serif"
    readonly property string readingFontFamily: Qt.platform.os === "windows"
                                                        ? "Georgia"
                                                        : "serif"

    function readingFontFamilyFor(fontKey) {
        if (fontKey === "sans") {
            return uiFontFamily
        }
        if (fontKey === "mono") {
            return Qt.platform.os === "windows" ? "Cascadia Mono" : "monospace"
        }
        return readingFontFamily
    }

    readonly property color windowColor: darkMode ? "#151815" : sepiaMode ? "#e7e5dc" : "#f1f3ef"
    readonly property color surfaceColor: darkMode ? "#202421" : sepiaMode ? "#f5f0e4" : "#ffffff"
    readonly property color surfaceMutedColor: darkMode ? "#282d29" : sepiaMode ? "#e4ded0" : "#e9ede8"
    readonly property color readingColor: darkMode ? "#1b1f1c" : sepiaMode ? "#faf3e3" : "#fbfaf7"
    readonly property color textColor: darkMode ? "#edf0eb" : sepiaMode ? "#302d27" : "#20231f"
    readonly property color mutedTextColor: darkMode ? "#aab1ab" : sepiaMode ? "#6d695f" : "#646a64"
    readonly property color disabledTextColor: darkMode ? "#747c76" : sepiaMode ? "#9b968a" : "#9aa09a"
    readonly property color borderColor: darkMode ? "#373d38" : sepiaMode ? "#d3ccbd" : "#d9ddd7"
    readonly property color strongBorderColor: darkMode ? "#4b534c" : sepiaMode ? "#b8b0a1" : "#bdc4bd"

    readonly property color accentColor: darkMode ? "#7db58e" : sepiaMode ? "#496548" : "#315f43"
    readonly property color accentHoverColor: darkMode ? "#91c49f" : sepiaMode ? "#3b563b" : "#284f38"
    readonly property color accentPressedColor: darkMode ? "#69a47b" : sepiaMode ? "#304831" : "#20412e"
    readonly property color accentSoftColor: darkMode ? "#294032" : sepiaMode ? "#dce5d7" : "#e2ebe4"
    readonly property color onAccentColor: darkMode ? "#102016" : "#ffffff"
    readonly property color primaryActionColor: darkMode ? "#3b7451" : "#315f43"
    readonly property color primaryActionHoverColor: darkMode ? "#477f5c" : "#284f38"
    readonly property color primaryActionPressedColor: darkMode ? "#315f43" : "#20412e"
    readonly property color primaryActionTextColor: "#ffffff"
    readonly property color focusColor: darkMode ? "#d39a68" : sepiaMode ? "#8d5f42" : "#a95f40"
    readonly property color dangerColor: darkMode ? "#ec8b83" : "#b54840"

    readonly property color chromeColor: darkMode ? "#111412" : "#202320"
    readonly property color chromeHoverColor: darkMode ? "#292e2a" : "#343934"
    readonly property color chromePressedColor: darkMode ? "#343a35" : "#414741"
    readonly property color chromeTextColor: "#f2f4f0"
    readonly property color chromeMutedTextColor: "#b7bdb7"
    readonly property color chromeBorderColor: darkMode ? "#303530" : "#383d38"

    readonly property int space2xs: 4
    readonly property int spaceXs: 8
    readonly property int spaceSm: 12
    readonly property int spaceMd: 16
    readonly property int spaceLg: 24
    readonly property int spaceXl: 32
    readonly property int space2xl: 48

    readonly property int radiusSm: 4
    readonly property int radiusMd: 6
    readonly property int radiusLg: 8

    readonly property int compactControlHeight: 32
    readonly property int controlHeight: 36
    readonly property int toolbarHeight: 58
    readonly property int statusBarHeight: 36
    readonly property int readerPageMaxWidth: 820
    readonly property int readerPagePadding: 52
    readonly property int readerNarrowGutter: 16
    readonly property int readerWideGutter: 32

    readonly property int captionFontSize: 12
    readonly property int bodyFontSize: 14
    readonly property int bodyLargeFontSize: 16
    readonly property int titleFontSize: 20
    readonly property int displayFontSize: 28
    readonly property int iconFontSize: 18

    readonly property int motionFast: 100
    readonly property int motionNormal: 160
    readonly property int tooltipDelay: 500
}
