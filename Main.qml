import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic

ApplicationWindow 
{
    id: window
    visible: false
    color: '#7d000000'
    width: 500
    height: 250
    minimumWidth: 500
    maximumWidth: 500
    minimumHeight: 250
    maximumHeight: 250
    title: "BlVidject"
    property bool cancellingFromBlueZ: false
    
    Timer {
        id: closeTimer
        interval: 500
        repeat: false

        onTriggered: {
            if (!window.active) {
                window.close()
            }
        }
    }

    onActiveChanged: {
        if (!active) {
            closeTimer.start()
        } else {
            closeTimer.stop()
        }
    }

    Dialog 
    {
        id: pinDialog

        title: "Сопряжение Bluetooth"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton

        property string devicePath: ""

        width: 320

        Column {
            width: parent.width
            spacing: 12

            Label {
                width: parent.width
                text: "Введите PIN-код для Bluetooth-устройства:"
                wrapMode: Text.WordWrap
            }

            TextField {
                id: pinInput

                width: parent.width
                placeholderText: "PIN-код"
                inputMethodHints: Qt.ImhDigitsOnly
                maximumLength: 16
                focus: true
            }

            Row {
                spacing: 8
                anchors.right: parent.right

                Button {
                    text: "Отмена"

                    onClicked: {
                        pinDialog.close()
                    }
                }

                Button {
                    text: "Подтвердить"
                    enabled: pinInput.text.length > 0

                    onClicked: {
                        bluetoothAgent.ReplyPinCode(pinInput.text)
                        pinInput.clear()
                        pinDialog.close()
                    }
                }
            }
        }
    }

    Dialog {
        id: passkeyDialog

        title: "Ввод ключа Bluetooth"
        modal: true
        anchors.centerIn: parent

        standardButtons: Dialog.Ok | Dialog.Cancel

        property string devicePath: ""

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text: "Введите шестизначный ключ:"
            }

            TextField {
                id: passkeyInput

                Layout.fillWidth: true
                placeholderText: "000000"
                maximumLength: 6
                inputMethodHints: Qt.ImhDigitsOnly
                validator: RegularExpressionValidator {
                    regularExpression: /^\d{6}$/
                }
            }
        }

        onAccepted: {
            bluetoothAgent.ReplyPasskey(passkeyInput.text)
        }

        onRejected: {
            if (!window.cancellingFromBlueZ)
                bluetoothAgent.RejectPasskey()
        }
    }

    function onCancelRequested() 
    {
        console.log("QML: получен CancelRequested")
        cancellingFromBlueZ = true

        if (confirmationDialog.opened)
        {
            console.log("Закрытие окна подверждения")
            confirmationDialog.close()
        }

        if (pinDialog.opened)
        {
            console.log("Закрытие окна ввода ")
            pinDialog.close()
        }

        if (passkeyDialog.opened)
        {
            console.log("Закрытие окна ввода числового ключа")
            passkeyDialog.close()
        } 
        
        cancellingFromBlueZ = false
    }

    function showPasskeyDialog(devicePath) {
        passkeyDialog.devicePath = devicePath
        passkeyInput.clear()
        passkeyDialog.open()
    }

    function showConfirmation(devicePath, passkey) {
        console.log("Запрос подтверждения:", devicePath, passkey)

        const card = findDeviceCard(devicePath)

        if (!card) {
            console.warn("Карточка устройства не найдена:", devicePath)
            return
        }

        card.requestedPasskey = passkey
        card.awaitingConfirmation = true

        console.log("Подтверждение показано в карточке устройства")
    }

    function findDeviceCard(devicePath) {
        console.log("Ищем устройство:", devicePath)
        console.log("Количество элементов:", deviceList.count)

        for (let i = 0; i < deviceList.count; ++i) {
            let item = deviceList.itemAtIndex(i)

            if (item) {
                console.log(
                    "Карточка:", i,
                    "путь:", item.devicePath
                )

                if (item.devicePath === devicePath)
                    return item
            }
        }

        return null
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Список устройств
        ListView {
            id: deviceList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6

            model: bluetoothManager

            delegate: Rectangle 
            {
                id: deviceCard

                width: deviceList.width
                height: 60
                radius: 8
                property bool awaitingConfirmation: false
                property int requestedPasskey: 0
                property string devicePath: model.objectPath
                color: model.connected ? "#203d32" : "#292d36"
                border.width: 1
                border.color: model.connected ? "#3b8063" : "#3a3f4b"

                // Список устройств
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Text {
                        text: "ᛒ"
                        color: "#80bfff"
                        font.pixelSize: 22
                    }

                    // Список устройств
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: model.deviceName || "Неизвестное устройство"
                            color: "#f0f2f5"
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: model.deviceAddress
                            color: "#a6adbb"
                            font.pixelSize: 10
                        }

                        Text {
                            visible: !awaitingConfirmation
                            text: model.connected ? "● Подключено" : "Не подключено"
                            color: model.connected ? "#71d6a3" : "#a6adbb"
                            font.pixelSize: 9
                        }
                    }

                    // Меню подверждения устройство 
                    ColumnLayout {
                        visible: awaitingConfirmation
                        spacing: 3
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            text: requestedPasskey.toString().padStart(6, "0")
                            color: "white"
                            font.pixelSize: 14
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        
                        RowLayout {
                            spacing: 5

                            Button {
                                text: "✓"
                                font.pixelSize: 11
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 26
                                padding: 2

                                background: Rectangle {
                                    radius: 5
                                    color: "#343a46"
                                }

                                onClicked: {
                                    bluetoothAgent.ReplyConfirmation(true)
                                    deviceCard.awaitingConfirmation = false
                                }
                            }

                            Button {
                                text: "✕"
                                font.pixelSize: 11
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 26
                                padding: 2

                                background: Rectangle {
                                    radius: 5
                                    color: "#343a46"
                                }

                                onClicked: {
                                    bluetoothAgent.ReplyConfirmation(false)
                                    deviceCard.awaitingConfirmation = false
                                }
                            }
                        }

                    }
                }


                // Контекстное меню
                Menu {
                    id: deviceMenu

                    MenuItem {
                        text: model.connected ? "Отключить" : "Подключить"

                        onTriggered: {
                            bluetoothManager.ConnectToDevice(model.objectPath) 
                        }
                    }

                    MenuItem {
                        text: "Удалить сопряжение"

                        onTriggered: {
                            bluetoothManager.RemoveDevice(model.objectPath) 
                        }
                    }
                }

                // Обрабочик меню
                TapHandler {
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onTapped: {
                        deviceMenu.popup()
                    }
                }
            }
        }

        // Нижняя панель управления
        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            
            Button {
                text: bluetoothManager ? (bluetoothManager.bluetoothEnabled
                    ? "ᛒ"
                    : "ᛡ") : "Закрытие"
                
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                
                background: Rectangle {
                    radius: 5
                    color: "#343a46"
                }

                onClicked: {
                    bluetoothManager.SetBluetoothEnabled(
                        !bluetoothManager.bluetoothEnabled
                    )
                }
            }

            Button {
                text: bluetoothManager ? (bluetoothManager.discoveryActive
                    ? "🛑"
                    : "🔎") : "Закрытие"
            
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                
                background: Rectangle {
                    radius: 5
                    color: "#343a46"
                }

                onClicked: {
                    if (bluetoothManager.discoveryActive)
                        bluetoothManager.StopDiscovery()
                    else
                        bluetoothManager.StartDiscovery()
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: bluetoothManager ? (bluetoothManager.discoveryActive
                    ? "Поиск устройств…"
                    : "BlVidject") : "Закрытие"

                color: "#a6adbb"
                font.pixelSize: 11
            }
        }
    }

    property bool isClosing: false
    onVisibleChanged: { if (!visible) isClosing = true}
}


