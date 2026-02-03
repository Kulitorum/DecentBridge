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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
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
                color: "#00d9ff"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "v" + bridge.version
                font.pixelSize: 14
                color: "#888"
            }
        }

        // Bridge Name Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: nameColumn.height + 32
            color: "#ffffff15"
            radius: 16

            ColumnLayout {
                id: nameColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 8

                Text {
                    text: "Bridge Name"
                    font.pixelSize: 12
                    color: "#888"
                }

                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    text: bridge.bridgeName
                    font.pixelSize: 20
                    color: "#fff"
                    background: Rectangle {
                        color: "#00000033"
                        radius: 8
                    }
                    onEditingFinished: bridge.bridgeName = text
                }
            }
        }

        // Connection Info Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: infoColumn.height + 32
            color: "#ffffff15"
            radius: 16

            ColumnLayout {
                id: infoColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "Connection Info"
                    font.pixelSize: 12
                    color: "#888"
                }

                // IP Address
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
                        color: "#00ff88"
                    }
                }

                // HTTP Port
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
                        color: "#fff"
                    }
                }

                // WebSocket Port
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
                        color: "#fff"
                    }
                }
            }
        }

        // Device Status Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: statusColumn.height + 32
            color: "#ffffff15"
            radius: 16

            ColumnLayout {
                id: statusColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "Device Status"
                    font.pixelSize: 12
                    color: "#888"
                }

                // DE1 Status
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        color: bridge.de1Connected ? "#00ff88" : "#ff4757"
                    }

                    Text {
                        text: "DE1"
                        font.pixelSize: 16
                        color: "#fff"
                    }

                    Text {
                        text: bridge.de1Connected ? bridge.de1Name : "Not connected"
                        font.pixelSize: 14
                        color: bridge.de1Connected ? "#00ff88" : "#888"
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                    }
                }

                // Scale Status
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        color: bridge.scaleConnected ? "#00ff88" : "#ff4757"
                    }

                    Text {
                        text: "Scale"
                        font.pixelSize: 16
                        color: "#fff"
                    }

                    Text {
                        text: bridge.scaleConnected ? bridge.scaleName : "Not connected"
                        font.pixelSize: 14
                        color: bridge.scaleConnected ? "#00ff88" : "#888"
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }

        // Instructions Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: instructionsColumn.height + 32
            color: "#ffffff10"
            radius: 16

            ColumnLayout {
                id: instructionsColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 8

                Text {
                    text: "How to Connect"
                    font.pixelSize: 12
                    color: "#888"
                }

                Text {
                    Layout.fillWidth: true
                    text: "1. Open Streamline on your device"
                    font.pixelSize: 14
                    color: "#ccc"
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: "2. The app will auto-discover this bridge"
                    font.pixelSize: 14
                    color: "#ccc"
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: "3. Or enter the IP address manually"
                    font.pixelSize: 14
                    color: "#ccc"
                    wrapMode: Text.WordWrap
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
