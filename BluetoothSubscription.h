#ifndef BLUETOOTHSUBSCRIPTION_H
#define BLUETOOTHSUBSCRIPTION_H

#include <gio/gio.h>
#include <QObject>
#include <QString>

class BluetoothSubscription : public QObject
{
    Q_OBJECT

    public:
        // ===== Конструктор и деструктор =====
        // ===== Constructor, destructor =====
        explicit BluetoothSubscription(GDBusConnection* connection, QObject* parent = nullptr);
        ~BluetoothSubscription() override;

    private:
        // ===== Подписки на D-Bus сигналы =====
        // ===== Subscriptions to D-Bus signals 
        GDBusConnection* connection = nullptr;
        guint signalSubscriptionId = 0;
        guint devicePropertiesSubscriptionId = 0;
        guint interfacesAddedSubscriptionId = 0;
        guint interfacesRemovedSubscriptionId = 0;

        // ===== Обработчики сигналов =====
        // ===== Signal handlers =====
        guint SubscribeToDeviceProperties();
        guint GetSignalSubscriptionId();
        guint InterfacesAdded();
        guint InterfacesRemoved();
        
    signals:
        // ===== Сигналы уведомлений =====
        // ===== Notification signals ====
        void BluetoothPowerChanged(bool enabled);
        void DeviceAdded(QString objectPath, QString name, QString address, bool connected, bool paired, bool trusted);
        void DeviceRemoved(QString objectPath);
        void DevicePropertiesChanged(QString objectPath, bool connected, bool connectedChanged, bool paired, bool pairedChanged);
};

#endif // BLUETOOTHSUBSCRIPTION_H
