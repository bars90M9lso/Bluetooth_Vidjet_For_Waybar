#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <gio/gio.h>
#include <QObject>

class BluetoothManager : public QObject
{
    Q_OBJECT

    public:
        explicit BluetoothManager(QObject *parent = nullptr);
        ~BluetoothManager() override;

        bool IsBluetoothEnabled();
        Q_PROPERTY(bool bluetoothEnabled READ IsBluetoothEnabled NOTIFY bluetoothEnabledChanged)
        Q_INVOKABLE void SetBluetoothEnabled(bool enabled);

    private:
        GDBusConnection* connection = nullptr;
        bool bluetoothEnabled = false;
        guint signalSubscriptionId = 0;

        GDBusConnection* ConnectToDBus();
        guint GetSignalSubscriptionId();
        bool ReadBluetoothEnabled();

    signals:
        void bluetoothEnabledChanged();

};

#endif // BLUETOOTHMANAGER_H
