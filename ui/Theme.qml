pragma Singleton

import QtQuick

QtObject {
    property string colorTheme: "light"

    readonly property bool darkMode: colorTheme === "dark"

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

    readonly property color windowColor: darkMode ? "#090909" : "#f4f4f4"
    readonly property color surfaceColor: darkMode ? "#151515" : "#ffffff"
    readonly property color surfaceMutedColor: darkMode ? "#242424" : "#ededed"
    readonly property color readingColor: darkMode ? "#0d0d0d" : "#ffffff"
    readonly property color textColor: darkMode ? "#f5f5f5" : "#111111"
    readonly property color mutedTextColor: darkMode ? "#b3b3b3" : "#666666"
    readonly property color disabledTextColor: darkMode ? "#737373" : "#999999"
    readonly property color borderColor: darkMode ? "#303030" : "#dedede"
    readonly property color strongBorderColor: darkMode ? "#4a4a4a" : "#b7b7b7"

    readonly property color accentColor: darkMode ? "#ffffff" : "#111111"
    readonly property color accentHoverColor: darkMode ? "#efefef" : "#2a2a2a"
    readonly property color accentPressedColor: darkMode ? "#d4d4d4" : "#000000"
    readonly property color accentSoftColor: darkMode ? "#262626" : "#e7e7e7"
    readonly property color accentTextColor: darkMode ? "#080808" : "#ffffff"
    readonly property color primaryActionColor: darkMode ? "#ffffff" : "#111111"
    readonly property color primaryActionHoverColor: darkMode ? "#e5e5e5" : "#2a2a2a"
    readonly property color primaryActionPressedColor: darkMode ? "#c8c8c8" : "#000000"
    readonly property color primaryActionTextColor: darkMode ? "#080808" : "#ffffff"
    readonly property color focusColor: darkMode ? "#ffffff" : "#000000"
    readonly property color dangerColor: darkMode ? "#f5f5f5" : "#111111"
    readonly property color annotationHighlightColor: darkMode ? "#303030" : "#e2e2e2"
    readonly property color searchMatchColor: darkMode ? "#454545" : "#d0d0d0"
    readonly property color currentSearchMatchColor: darkMode ? "#ffffff" : "#111111"
    readonly property color currentSearchTextColor: darkMode ? "#080808" : "#ffffff"

    readonly property color chromeColor: darkMode ? "#000000" : "#ffffff"
    readonly property color chromeHoverColor: darkMode ? "#1c1c1c" : "#f0f0f0"
    readonly property color chromePressedColor: darkMode ? "#2b2b2b" : "#e2e2e2"
    readonly property color chromeTextColor: darkMode ? "#ffffff" : "#000000"
    readonly property color chromeMutedTextColor: darkMode ? "#b6b6b6" : "#555555"
    readonly property color chromeBorderColor: darkMode ? "#2e2e2e" : "#e0e0e0"

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
