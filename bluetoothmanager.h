#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include "BluetoothSubscription.h"
#include <gio/gio.h>
#include <QObject>
#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QHash>
#include <QVariant>
#include <QTimer>

class BluetoothManager : public QAbstractListModel
{
    Q_OBJECT

    public:
        // ===== Конструктор и деструктор =====
        // ===== Constructor, destructor =====
        explicit BluetoothManager(QObject *parent = nullptr);
        ~BluetoothManager() override;

        // ===== Роли модели QListView/QML =====
        // ===== Model roles for QListView/QML =====
        enum Roles 
        {
            NameRole = Qt::UserRole + 1,
            AddressRole,
            ConnectedRole,
            PairedRole,
            TrustedRole,
            ObjectPathRole
        };
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

        // ===== Состояние Bluetooth =====
        // ===== Bluetooth state =====
        bool IsBluetoothEnabled();
        bool IsDiscovery();
        void updateDeviceList();

        // ===== Передача свойств Bluetooth =====
        // ===== Bluetooth property management =====
        Q_PROPERTY(bool bluetoothEnabled READ IsBluetoothEnabled NOTIFY bluetoothEnabledChanged)
        Q_INVOKABLE void SetBluetoothEnabled(bool enabled);

        // ===== Передача управление поиском устройств =====
        // ===== Device discovery management =====
        Q_PROPERTY(bool discoveryActive READ IsDiscovery NOTIFY discoveryActiveChanged)
        Q_INVOKABLE bool StartDiscovery();
        Q_INVOKABLE bool StopDiscovery();

        // ===== Передача управления устройствами =====
        // ===== Device management =====
        Q_INVOKABLE void ConnectToDevice(const QString& objectPath);

    private:
        // ===== Массив устройства =====
        // ===== Device array =====
        struct BluetoothDevice
        {
            QString name;
            QString address;
            bool connected = false;
            bool paired = false;
            bool trusted = false;
            QString objectPath;  
        };
        QVector<BluetoothDevice> devices;

        // ===== Внутренние переменные =====
        // ===== Internal variables =====
        GDBusConnection* connection = nullptr;
        BluetoothSubscription* bluetoothSubscription = nullptr;
        bool bluetoothEnabled = false;
        bool discoveryActive = false;
        QTimer discoveryTimer;
        
        // ===== Операции с устройствами =====
        // ===== Device operations =====
        GDBusConnection* ConnectToDBus();
        bool ReadBluetoothEnabled();
        void ConnectDevice(const QString& objectPath);
        void DisconnectDevice(const QString& objectPath);
        void PairDevice(const QString& objectPath);
        void SortDevices();

        // ===== Обработчики сигналов =====
        // ===== BlueZ event handlers =====
        void HandleBluetoothPowerChanged(bool enabled);
        void HandleDeviceAdded(QString objectPath, QString name, QString address, bool connected, bool paired, bool trusted);
        void HandleDeviceRemoved(QString objectPath);
        void HandleDevicePropertiesChanged(QString objectPath, bool connected, bool connectedChanged, bool paired, bool pairedChanged);

    signals:
        void bluetoothEnabledChanged();
        void discoveryActiveChanged();  
};

#endif // BLUETOOTHMANAGER_H
