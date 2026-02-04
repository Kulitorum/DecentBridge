import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 480
    height: 800
    title: "DecentBridge"
    color: "#1a1a2e"

    // Colors matching the web dashboard
    readonly property color cardBg: "#1affffff"
    readonly property color metricBg: "#33000000"
    readonly property color accentBlue: "#00d9ff"
    readonly property color accentGreen: "#00ff88"
    readonly property color disconnectedRed: "#ff4757"
    readonly property color textPrimary: "#ffffff"
    readonly property color textSecondary: "#888888"

    function stateColor(state) {
        switch (state) {
            case "Sleep": case "GoingToSleep": return "#4a4a6a"
            case "Idle": return "#2d5a27"
            case "Espresso": return "#8b4513"
            case "Steam": return "#4a6fa5"
            case "HotWater": case "HotWaterRinse": return "#5a3d7a"
            default: return "#4a4a6a"
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        contentWidth: width
        contentHeight: mainColumn.height + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 1500

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            id: mainColumn
            width: parent.width - 48
            x: 24
            y: 24
            spacing: 20

            // Header
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "DecentBridge"
                    font.pixelSize: 36
                    font.bold: true
                    color: accentBlue
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: versionText.width + 16
                    height: versionText.height + 8
                    radius: height / 2
                    color: "#1affffff"

                    Text {
                        id: versionText
                        anchors.centerIn: parent
                        text: "v" + bridge.version
                        font.pixelSize: 12
                        color: textSecondary
                    }
                }
            }

            // ── Machine Card ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: machineColumn.height + 32
                color: cardBg
                radius: 16

                ColumnLayout {
                    id: machineColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 15

                    // Title row with status dot
                    RowLayout {
                        spacing: 10

                        Rectangle {
                            width: 12; height: 12; radius: 6
                            color: bridge.de1Connected ? accentGreen : disconnectedRed
                            layer.enabled: bridge.de1Connected
                            layer.effect: Item {} // glow placeholder
                        }

                        Text {
                            text: "DE1 Espresso Machine"
                            font.pixelSize: 16
                            font.bold: true
                            color: accentBlue
                        }
                    }

                    // State badge
                    Rectangle {
                        width: stateBadgeText.width + 32
                        height: stateBadgeText.height + 16
                        radius: 20
                        color: stateColor(bridge.machineState)

                        Text {
                            id: stateBadgeText
                            anchors.centerIn: parent
                            text: bridge.de1Connected ? bridge.machineState : "--"
                            font.pixelSize: 16
                            font.bold: true
                            color: textPrimary
                        }
                    }

                    // Metrics grid (2x2)
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 12
                        columnSpacing: 12

                        // Group Temp (highlight)
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: metricBg
                            radius: 12

                            Column {
                                anchors.centerIn: parent
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: bridge.de1Connected ? bridge.groupTemp.toFixed(1) + "\u00B0" : "--"
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: accentGreen
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Group Temp"
                                    font.pixelSize: 12
                                    color: textSecondary
                                }
                            }
                        }

                        // Steam Temp
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: metricBg
                            radius: 12

                            Column {
                                anchors.centerIn: parent
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: bridge.de1Connected ? bridge.steamTemp.toFixed(1) + "\u00B0" : "--"
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: textPrimary
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Steam Temp"
                                    font.pixelSize: 12
                                    color: textSecondary
                                }
                            }
                        }

                        // Pressure
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: metricBg
                            radius: 12

                            Column {
                                anchors.centerIn: parent
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: bridge.de1Connected ? bridge.pressure.toFixed(1) : "--"
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: textPrimary
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Pressure (bar)"
                                    font.pixelSize: 12
                                    color: textSecondary
                                }
                            }
                        }

                        // Flow
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: metricBg
                            radius: 12

                            Column {
                                anchors.centerIn: parent
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: bridge.de1Connected ? bridge.flow.toFixed(1) : "--"
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: textPrimary
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Flow (ml/s)"
                                    font.pixelSize: 12
                                    color: textSecondary
                                }
                            }
                        }
                    }

                    // State control buttons
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            text: "Idle"
                            onClicked: bridge.setMachineState("idle")
                            background: Rectangle { radius: 8; color: "#2d5a27" }
                            contentItem: Text { text: parent.text; color: "#fff"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            padding: 12
                        }
                        Button {
                            text: "Espresso"
                            onClicked: bridge.setMachineState("espresso")
                            background: Rectangle { radius: 8; color: "#8b4513" }
                            contentItem: Text { text: parent.text; color: "#fff"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            padding: 12
                        }
                        Button {
                            text: "Steam"
                            onClicked: bridge.setMachineState("steam")
                            background: Rectangle { radius: 8; color: "#4a6fa5" }
                            contentItem: Text { text: parent.text; color: "#fff"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            padding: 12
                        }
                        Button {
                            text: "Hot Water"
                            onClicked: bridge.setMachineState("hotWater")
                            background: Rectangle { radius: 8; color: "#5a3d7a" }
                            contentItem: Text { text: parent.text; color: "#fff"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            padding: 12
                        }
                        Button {
                            text: "Sleep"
                            onClicked: bridge.setMachineState("sleep")
                            background: Rectangle { radius: 8; color: "#4a4a6a" }
                            contentItem: Text { text: parent.text; color: "#fff"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            padding: 12
                        }
                    }
                }
            }

            // ── Scale Card ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: scaleColumn.height + 32
                color: cardBg
                radius: 16

                ColumnLayout {
                    id: scaleColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 15

                    // Title row with status dot and scale name
                    RowLayout {
                        spacing: 10

                        Rectangle {
                            width: 12; height: 12; radius: 6
                            color: bridge.scaleConnected ? accentGreen : disconnectedRed
                        }

                        Text {
                            text: "Scale"
                            font.pixelSize: 16
                            font.bold: true
                            color: accentBlue
                        }

                        Text {
                            text: bridge.scaleConnected ? bridge.scaleName : "(not connected)"
                            font.pixelSize: 14
                            color: textSecondary
                        }
                    }

                    // Scale metrics (1x2)
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 12

                        // Weight (highlight)
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: metricBg
                            radius: 12

                            Column {
                                anchors.centerIn: parent
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: bridge.scaleConnected ? bridge.scaleWeight.toFixed(1) : "--"
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: accentGreen
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Weight (g)"
                                    font.pixelSize: 12
                                    color: textSecondary
                                }
                            }
                        }

                        // Flow
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: metricBg
                            radius: 12

                            Column {
                                anchors.centerIn: parent
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: bridge.scaleConnected ? bridge.scaleFlow.toFixed(1) : "--"
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: textPrimary
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Flow (g/s)"
                                    font.pixelSize: 12
                                    color: textSecondary
                                }
                            }
                        }
                    }

                    // Scale buttons
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            text: "Tare"
                            onClicked: bridge.tareScale()
                            background: Rectangle { radius: 8; color: accentBlue }
                            contentItem: Text { text: parent.text; color: "#000"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            padding: 12
                        }
                        Button {
                            text: "Disconnect"
                            onClicked: bridge.disconnectScale()
                            background: Rectangle { radius: 8; color: disconnectedRed }
                            contentItem: Text { text: parent.text; color: "#fff"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            padding: 12
                        }
                        Button {
                            text: bridge.scanning ? "Scanning..." : "Scan for Scale"
                            enabled: !bridge.scanning
                            onClicked: bridge.startScan()
                            background: Rectangle { radius: 8; color: enabled ? "#ff9f43" : "#666666" }
                            contentItem: Text { text: parent.text; color: enabled ? "#000" : "#999"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            padding: 12
                        }
                    }

                    // Scan status
                    RowLayout {
                        visible: bridge.scanning
                        spacing: 8

                        BusyIndicator {
                            running: bridge.scanning
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                        }

                        Text {
                            text: "Scanning for Bluetooth scales..."
                            font.pixelSize: 14
                            color: textPrimary
                        }
                    }

                    // Discovered scales list
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        visible: bridge.discoveredScales.length > 0

                        Text {
                            text: bridge.discoveredScales.length + " scale(s) found"
                            font.pixelSize: 14
                            color: textSecondary
                            visible: !bridge.scanning && bridge.discoveredScales.length > 0
                        }

                        Repeater {
                            model: bridge.discoveredScales

                            Rectangle {
                                Layout.fillWidth: true
                                height: scaleItemRow.height + 20
                                color: "#1affffff"
                                radius: 8

                                RowLayout {
                                    id: scaleItemRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: 10
                                    spacing: 8

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: modelData.name
                                            font.pixelSize: 14
                                            color: textPrimary
                                        }
                                        Text {
                                            text: modelData.scaleType
                                            font.pixelSize: 12
                                            color: textSecondary
                                        }
                                    }

                                    Button {
                                        text: "Connect"
                                        onClicked: bridge.connectToScale(modelData.address)
                                        background: Rectangle { radius: 8; color: accentBlue }
                                        contentItem: Text { text: parent.text; color: "#000"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        padding: 8
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Connection Info Card ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: infoColumn.height + 32
                color: cardBg
                radius: 16

                ColumnLayout {
                    id: infoColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 12

                    Text {
                        text: "Connection Info"
                        font.pixelSize: 12
                        color: textSecondary
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            text: "IP Address:"
                            font.pixelSize: 16
                            color: "#aaa"
                        }
                        Text {
                            text: bridge.ipAddress
                            font.pixelSize: 20
                            font.bold: true
                            color: accentGreen
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            text: "HTTP:"
                            font.pixelSize: 16
                            color: "#aaa"
                        }
                        Text {
                            text: "http://" + bridge.ipAddress + ":" + bridge.httpPort
                            font.pixelSize: 14
                            color: textPrimary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            text: "WebSocket:"
                            font.pixelSize: 16
                            color: "#aaa"
                        }
                        Text {
                            text: "ws://" + bridge.ipAddress + ":" + bridge.wsPort
                            font.pixelSize: 14
                            color: textPrimary
                        }
                    }
                }
            }

            // ── Footer ──
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 10
                spacing: 4

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "API Documentation"
                    font.pixelSize: 14
                    font.bold: true
                    color: accentBlue

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("http://" + bridge.ipAddress + ":" + bridge.httpPort + "/api/docs")
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "WebSocket: ws://" + bridge.ipAddress + ":" + bridge.wsPort + "/ws/v1/scale/snapshot"
                    font.pixelSize: 12
                    color: "#666"
                }
            }
        }
    }
}
