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

    Button {
    text: bluetoothManager.bluetoothEnabled
          ? "Выключить Bluetooth"
          : "Включить Bluetooth"

    onClicked: {
        bluetoothManager.SetBluetoothEnabled(
            !bluetoothManager.bluetoothEnabled
        )
    }
}

}
