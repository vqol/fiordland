pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import Caelestia
import Caelestia.Config
import Caelestia.I18n
import Caelestia.Services
import qs.components
import qs.components.containers
import qs.components.controls
import qs.components.effects
import qs.services
import qs.utils

Scope {
    id: root

    readonly property int type: BluetoothAgent.type
    readonly property bool takesInput: type === BluetoothAgent.PinCode || type === BluetoothAgent.Passkey
    readonly property bool showsCode: type === BluetoothAgent.Confirm || type === BluetoothAgent.DisplayPasskey || type === BluetoothAgent.DisplayPinCode
    readonly property string device: BluetoothAgent.deviceName || Tr.tr("A device")

    function serviceName(uuid: string): string {
        const id16 = uuid.slice(4, 8).toLowerCase();
        switch (id16) {
        case "110a":
        case "110b":
        case "110d":
            return Tr.trCtx("audio", "bluetooth service");
        case "110c":
        case "110e":
        case "110f":
            return Tr.trCtx("media controls", "bluetooth service");
        case "1108":
        case "111e":
        case "111f":
        case "1112":
            return Tr.trCtx("calls", "bluetooth service");
        case "1124":
        case "1812":
            return Tr.trCtx("keyboard and mouse input", "bluetooth service");
        case "1105":
        case "1106":
            return Tr.trCtx("file transfer", "bluetooth service");
        case "1115":
        case "1116":
            return Tr.trCtx("networking", "bluetooth service");
        default:
            return Tr.trCtx("a Bluetooth service", "bluetooth service");
        }
    }

    function title(): string {
        switch (type) {
        case BluetoothAgent.Confirm:
            return Tr.tr("Pair with %1?").arg(device);
        case BluetoothAgent.Authorize:
            return Tr.tr("%1 wants to pair").arg(device);
        case BluetoothAgent.AuthorizeService:
            return Tr.tr("%1 wants to connect").arg(device);
        case BluetoothAgent.PinCode:
            return Tr.tr("Enter the PIN for %1").arg(device);
        case BluetoothAgent.Passkey:
            return Tr.tr("Enter the code for %1").arg(device);
        case BluetoothAgent.DisplayPasskey:
        case BluetoothAgent.DisplayPinCode:
            return Tr.tr("Type this code on %1").arg(device);
        default:
            return "";
        }
    }

    function body(): string {
        switch (type) {
        case BluetoothAgent.Confirm:
            return Tr.tr("Make sure %1 shows the same code.").arg(device);
        case BluetoothAgent.Authorize:
            return Tr.tr("Allow it to pair with this computer?");
        case BluetoothAgent.AuthorizeService:
            return Tr.tr("It isn't paired with this computer. Allow it to use %1?").arg(serviceName(BluetoothAgent.service));
        case BluetoothAgent.PinCode:
            return Tr.tr("Often 0000 or 1234. If it has a keyboard, type the same PIN on it afterwards.");
        case BluetoothAgent.Passkey:
            return Tr.tr("Type the 6 digit code shown on %1.").arg(device);
        case BluetoothAgent.DisplayPasskey:
        case BluetoothAgent.DisplayPinCode:
            return Tr.tr("Then press Enter on it.");
        default:
            return "";
        }
    }

    function acceptLabel(): string {
        if (type === BluetoothAgent.Authorize || type === BluetoothAgent.AuthorizeService)
            return Tr.trCtx("Allow", "button");
        return Tr.trCtx("Pair", "button");
    }

    Connections {
        function onFailed(deviceName: string, reason: int): void {
            const name = deviceName || Tr.tr("the device");
            const message = reason === BluetoothAgent.TimedOut ? Tr.tr("Nobody answered the pairing request in time.") : Tr.tr("The other device cancelled pairing or stopped responding. Try again.");
            Toaster.toast(Tr.tr("Couldn't pair with %1").arg(name), message, "bluetooth_disabled", Toast.Warning);
        }

        target: BluetoothAgent
    }

    LazyLoader {
        active: root.type !== BluetoothAgent.None

        StyledWindow {
            id: win

            screen: Quickshell.screens.find(s => s.name === Hypr.focusedMonitor?.name) ?? Screens.screens[0] ?? null
            name: "bluetooth-agent"
            WlrLayershell.layer: WlrLayer.Overlay
            WlrLayershell.exclusionMode: ExclusionMode.Ignore
            WlrLayershell.keyboardFocus: BluetoothAgent.needsReply ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None

            implicitWidth: card.implicitWidth + Tokens.padding.large * 2
            implicitHeight: card.implicitHeight + Tokens.padding.large * 2

            StyledRect {
                id: card

                function submit(): void {
                    if (root.takesInput) {
                        if (!BluetoothAgent.submit(input.text))
                            input.isError = true;
                    } else if (BluetoothAgent.needsReply) {
                        BluetoothAgent.accept();
                    }
                }

                anchors.centerIn: parent
                implicitWidth: Math.round(Tokens.sizes.bar.networkWidth * 1.3)
                implicitHeight: layout.implicitHeight + Tokens.padding.large * 3
                radius: Tokens.rounding.extraLarge
                color: Colours.palette.m3surfaceContainerHigh

                scale: 0.9
                opacity: 0
                Component.onCompleted: {
                    scale = 1;
                    opacity = 1;
                    if (root.takesInput)
                        input.forceActiveFocus();
                    else
                        focus = true;
                }

                Keys.onEscapePressed: BluetoothAgent.reject()
                Keys.onReturnPressed: submit()
                Keys.onEnterPressed: submit()

                Elevation {
                    anchors.fill: parent
                    radius: parent.radius
                    z: -1
                    level: 3
                }

                ColumnLayout {
                    id: layout

                    anchors.fill: parent
                    anchors.margins: Tokens.padding.large * 1.5
                    spacing: Tokens.spacing.medium

                    RowLayout {
                        spacing: Tokens.spacing.medium

                        StyledRect {
                            implicitWidth: implicitHeight
                            implicitHeight: icon.implicitHeight + Tokens.padding.small * 2
                            radius: Tokens.rounding.full
                            color: Colours.palette.m3secondaryContainer

                            MaterialIcon {
                                id: icon

                                anchors.centerIn: parent
                                text: Icons.getBluetoothIcon(BluetoothAgent.deviceIcon)
                                color: Colours.palette.m3onSecondaryContainer
                                fontStyle: Tokens.font.icon.medium
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            StyledText {
                                Layout.fillWidth: true
                                text: root.title()
                                font: Tokens.font.body.large
                                elide: Text.ElideRight
                            }

                            StyledText {
                                Layout.fillWidth: true
                                visible: BluetoothAgent.deviceAddress !== "" && BluetoothAgent.deviceAddress !== BluetoothAgent.deviceName
                                text: BluetoothAgent.deviceAddress
                                color: Colours.palette.m3outline
                                font: Tokens.font.label.small
                            }
                        }
                    }

                    StyledText {
                        Layout.fillWidth: true
                        text: root.body()
                        color: Colours.palette.m3onSurfaceVariant
                        font: Tokens.font.body.small
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: Tokens.spacing.small
                        visible: root.showsCode
                        spacing: Tokens.spacing.small

                        Repeater {
                            model: BluetoothAgent.code.split("")

                            StyledText {
                                required property string modelData
                                required property int index

                                text: modelData
                                font: Tokens.font.headline.medium
                                color: root.type === BluetoothAgent.DisplayPasskey && index < BluetoothAgent.entered ? Colours.palette.m3primary : Colours.palette.m3onSurface
                            }
                        }
                    }

                    StyledTextField {
                        id: input

                        Layout.fillWidth: true
                        Layout.topMargin: Tokens.spacing.small
                        visible: root.takesInput
                        placeholderText: root.type === BluetoothAgent.Passkey ? Tr.tr("6 digit code") : Tr.tr("PIN")
                        errorText: root.type === BluetoothAgent.Passkey ? Tr.tr("Enter up to 6 digits") : Tr.tr("Enter 1 to 16 characters")
                        leadingIcon: "password"
                        maximumLength: root.type === BluetoothAgent.Passkey ? 6 : 16
                        inputMethodHints: root.type === BluetoothAgent.Passkey ? Qt.ImhDigitsOnly : (Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText)

                        onTextEdited: isError = false
                        onAccepted: card.submit()
                        Keys.onEscapePressed: BluetoothAgent.reject()
                    }

                    RowLayout {
                        Layout.topMargin: Tokens.spacing.small
                        Layout.alignment: Qt.AlignRight
                        spacing: Tokens.spacing.medium

                        TextButton {
                            text: BluetoothAgent.needsReply ? (root.type === BluetoothAgent.Authorize || root.type === BluetoothAgent.AuthorizeService ? Tr.trCtx("Deny", "button") : Tr.trCtx("Cancel", "button")) : Tr.trCtx("Hide", "button")
                            type: TextButton.Text
                            onClicked: BluetoothAgent.reject()
                        }

                        TextButton {
                            visible: BluetoothAgent.needsReply
                            text: root.acceptLabel()
                            type: TextButton.Filled
                            onClicked: card.submit()
                        }
                    }
                }

                Behavior on scale {
                    Anim {}
                }

                Behavior on opacity {
                    Anim {
                        type: Anim.DefaultEffects
                    }
                }
            }
        }
    }
}
