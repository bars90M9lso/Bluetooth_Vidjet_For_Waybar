import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

ApplicationWindow {
    id: window
    visible: true
    width: 500
    height: 250
    minimumWidth: 500
    maximumWidth: 500
    minimumHeight: 250
    maximumHeight: 250
    title: "BlVidject"

    ListView 
    {
        anchors.fill: parent
        anchors.margins: 20

        model: bluetoothManager

        delegate: Rectangle 
        {
            width: ListView.view.width
            height: 70
            radius: 8

            color: model.connected ? "#1fb92f" : "#303030"
            
            Column 
            {
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: 12
                }

                spacing: 4

                Text {
                    text: model.deviceName
                    color: "white"
                    font.pixelSize: 16
                }

                Text {
                    text: model.deviceAddress
                    color: "#aaaaaa"
                    font.pixelSize: 12
                }
            }

            TapHandler 
            {
                onTapped: { bluetoothManager.ConnectToDevice(model.objectPath) }
            }
        }


    }

    Button 
    {
        enabled: !window.isClosing
        
        text: bluetoothManager ? (bluetoothManager.bluetoothEnabled ? "Выключить Bluetooth" : "Включить Bluetooth") : "Bluetooth недоступен"
        onClicked: 
        {
            if (bluetoothManager) 
            {
                bluetoothManager.SetBluetoothEnabled(!bluetoothManager.bluetoothEnabled)
            }
        }
    }

    Button 
    {
        enabled: !window.isClosing
        x: 300
        y: 200
        
        text: bluetoothManager ? (bluetoothManager.discoveryActive ? "Остановить поиск" : "Начать поиск") : "Bluetooth недоступен"
        onClicked: 
        {
            if (bluetoothManager) 
            {
                if (bluetoothManager.discoveryActive) 
                {
                    bluetoothManager.StopDiscovery();
                } 
                else 
                {
                    bluetoothManager.StartDiscovery();
                }
            }
        }
    }


    property bool isClosing: false
    onVisibleChanged: { if (!visible) isClosing = true}
}


