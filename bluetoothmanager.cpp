#include "bluetoothmanager.h"
#include "BluetoothSubscription.h"
#include "QDebug"

#include <gio/gio.h>
#include <QAbstractListModel>
#include "QString"
#include "QVector"
#include <algorithm>

// ============================================================================
// Блок: Конструктор, деструктор 
// Block: Constructor, destructor
// ============================================================================
BluetoothManager::BluetoothManager(QObject *parent): QAbstractListModel{parent}
{
    connection = ConnectToDBus();
    if (connection == nullptr) return;
    qDebug() << "BluetoothManager: подключение к D-Bus успешно";
    
    bluetoothEnabled = ReadBluetoothEnabled();
    qDebug() << "BluetoothManager: Состояние bluetoooh" << bluetoothEnabled;

    bluetoothSubscription = new BluetoothSubscription(connection, this);
    QObject::connect
    (
        bluetoothSubscription,
        &BluetoothSubscription::BluetoothPowerChanged,
        this,
        &BluetoothManager::HandleBluetoothPowerChanged
    );
    QObject::connect(
        bluetoothSubscription,
        &BluetoothSubscription::DevicePropertiesChanged,
        this,
        &BluetoothManager::HandleDevicePropertiesChanged
    );
    QObject::connect(
        bluetoothSubscription,
        &BluetoothSubscription::DeviceAdded,
        this,
        &BluetoothManager::HandleDeviceAdded
    );
    QObject::connect(
        bluetoothSubscription,
        &BluetoothSubscription::DeviceRemoved,
        this,
        &BluetoothManager::HandleDeviceRemoved
    );

    discoveryTimer.setSingleShot(true);

    connect(&discoveryTimer, &QTimer::timeout, this, [this]() { StopDiscovery(); });
}

BluetoothManager::~BluetoothManager()
{
    delete bluetoothSubscription;
    bluetoothSubscription = nullptr;

    if (connection != nullptr)
    {
        g_object_unref(connection);
        connection = nullptr;
    }
}


// ============================================================================
// Блок: вспомогательные функции и работа с D-Bus
// Block: auxiliary functions and D-Bus operations
// ============================================================================
void PrintError(GError* error)
{
    qDebug() << (error ? error->message : "неизвестная ошибка") << '\n';
    if (error) g_error_free(error);
}

void BluetoothManager::SortDevices()
{
    beginResetModel();

    std::stable_sort(devices.begin(), devices.end(),
        [](const BluetoothDevice &a, const BluetoothDevice &b)
        {
            return a.connected && !b.connected;
        }
    );

    endResetModel();
}

GDBusConnection* BluetoothManager::ConnectToDBus()
{
    GError* error = nullptr;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);

    if (connection == nullptr)
    {
        qWarning() << "Ошибка подключения к D-Bus:" << (error ? error->message : "неизвестная ошибка");
        if (error != nullptr) g_error_free(error);

        return nullptr;
    }

    return connection;
}


// ============================================================================
// Блок: обработчики событий BlueZ
// Block: BlueZ event handlers
// ============================================================================
void BluetoothManager::HandleBluetoothPowerChanged(bool enabled)
{
    if (bluetoothEnabled != enabled)
    {
        bluetoothEnabled = enabled;
        emit bluetoothEnabledChanged();
    }
}

void BluetoothManager::HandleDevicePropertiesChanged(QString objectPath, bool connected, bool connectedChanged, bool paired, bool pairedChanged)
{
    for (int i = 0; i < devices.size(); ++i)
    {
        if (devices[i].objectPath == objectPath)
        {
            if (connectedChanged)
            {
                devices[i].connected = connected;
            }

            if (pairedChanged)
            {
                devices[i].paired = paired;
            }

            emit dataChanged(
                index(i, 0),
                index(i, 0),
                {ConnectedRole, PairedRole}
            );

            SortDevices();

            break; 
        }
    }
}

void BluetoothManager::HandleDeviceAdded(QString objectPath, QString name, QString address, bool connected, bool paired, bool trusted)
{
    BluetoothDevice newDevice;

    newDevice.name = name;
    newDevice.address = address;
    newDevice.connected = connected;
    newDevice.paired = paired;
    newDevice.trusted = trusted;
    
    newDevice.objectPath = objectPath;
                                 
    for (const BluetoothDevice &device : devices)
        if (device.objectPath == newDevice.objectPath) return;
    
    beginInsertRows(QModelIndex(), devices.size(), devices.size());
    devices.push_back(newDevice);
    endInsertRows();
}

void BluetoothManager::HandleDeviceRemoved(QString objectPath)
{  
    for (int i = 0; i < devices.size(); ++i)
    {
        if (devices[i].objectPath == objectPath)
        {
            beginRemoveRows(QModelIndex(), i, i);
            devices.removeAt(i);
            endRemoveRows();
            break;
        }
    }
}


// ============================================================================
// Блок: публичное API модели и состояние Bluetooth
// Block: public model API and Bluetooth state
// ============================================================================
bool BluetoothManager::IsBluetoothEnabled()
{
    return bluetoothEnabled;
}

bool BluetoothManager::ReadBluetoothEnabled()
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
    (
        this->connection,
        "org.bluez",
        "/org/bluez/hci0",
        "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", "org.bluez.Adapter1", "Powered"),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (result == nullptr)
    {
        PrintError(error);
        return false;
    }

    GVariant* value = g_variant_get_child_value(result, 0);
    GVariant* actualValue = g_variant_get_variant(value);
    gboolean powered = g_variant_get_boolean(actualValue);

    g_variant_unref(result);
    g_variant_unref(value);
    g_variant_unref(actualValue);

    return powered;
}

void BluetoothManager::SetBluetoothEnabled(bool enabled)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
    (
        this->connection,
        "org.bluez",
        "/org/bluez/hci0",
        "org.freedesktop.DBus.Properties",
        "Set",
        g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered", g_variant_new_boolean(enabled)),
        G_VARIANT_TYPE("()"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (result == nullptr)
    {
        PrintError(error);
        return;
    }

    g_variant_unref(result);
    return;
}


// ============================================================================
// Блок: загрузка списка устройств из BlueZ
// Block: loading the list of devices from BlueZ
// ============================================================================
void BluetoothManager::updateDeviceList()
{
    GError* error = nullptr;    
    QVector<BluetoothDevice> newDevices;

    GVariant* result = g_dbus_connection_call_sync
    (
        this->connection,
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects",
        nullptr,
        G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (result == nullptr)
    {
        PrintError(error);
        return;
    }

    GVariant* value = g_variant_get_child_value(result, 0);

    GVariantIter iter;
    g_variant_iter_init(&iter, value);

    gchar* objectPath = nullptr;
    GVariant* interfaces = nullptr;

    
    while (g_variant_iter_next(&iter, "{o@a{sa{sv}}}", &objectPath, &interfaces))
    {   
        GVariant* found = g_variant_lookup_value(interfaces, "org.bluez.Device1", nullptr);
        if (found == nullptr)
        {
            g_free(objectPath);
            g_variant_unref(interfaces);
            continue;
        }

        BluetoothDevice BluetoothDeviceTemp;

        GVariant* nameValue = g_variant_lookup_value(found, "Name", G_VARIANT_TYPE_STRING);
        if (nameValue != nullptr)
        {
            const gchar* name = g_variant_get_string(nameValue, nullptr);
            BluetoothDeviceTemp.name = name;
            g_variant_unref(nameValue);
        }

        GVariant* addressValue = g_variant_lookup_value(found, "Address", G_VARIANT_TYPE_STRING);
        if (addressValue != nullptr)
        {
            const gchar* address = g_variant_get_string(addressValue, nullptr);
            BluetoothDeviceTemp.address = address;
            g_variant_unref(addressValue);
        }

        GVariant* connectedValue = g_variant_lookup_value(found, "Connected", G_VARIANT_TYPE_BOOLEAN);
        if (connectedValue != nullptr)
        {
            const gboolean connected = g_variant_get_boolean(connectedValue);
            BluetoothDeviceTemp.connected = connected;
            g_variant_unref(connectedValue);
        }

        GVariant* pairedValue = g_variant_lookup_value(found, "Paired", G_VARIANT_TYPE_BOOLEAN);
        if (pairedValue != nullptr)
        {
            const gboolean paired = g_variant_get_boolean(pairedValue);
            BluetoothDeviceTemp.paired = paired;
            g_variant_unref(pairedValue);
        }

        GVariant* trustedValue = g_variant_lookup_value(found, "Trusted", G_VARIANT_TYPE_BOOLEAN);
        if (trustedValue != nullptr)
        {
            const gboolean trusted = g_variant_get_boolean(trustedValue);
            BluetoothDeviceTemp.trusted = trusted;
            g_variant_unref(trustedValue);
        }

        BluetoothDeviceTemp.objectPath = objectPath;

        newDevices.push_back(BluetoothDeviceTemp);

        g_variant_unref(found);
        g_free(objectPath);
        g_variant_unref(interfaces);
    }
        
    beginResetModel();
    devices = newDevices;
    endResetModel();
    
    g_variant_unref(value);
    g_variant_unref(result);
}


// ============================================================================
// Блок: модель данных для QML / QListModel
// Block: data model for QML / QListModel
// ============================================================================
int BluetoothManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return devices.size();
}

QVariant BluetoothManager::data(const QModelIndex &index, int role) const
{   
    if (!index.isValid() || index.row() >= devices.size()) return {};

    const BluetoothDevice &device = devices.at(index.row());

    switch (role) 
    {
        case NameRole:  return device.name;
        case ObjectPathRole:  return device.objectPath;
        case AddressRole: return device.address;
        case ConnectedRole: return device.connected;
        case PairedRole: return device.paired;
        case TrustedRole: return device.trusted;
    }

    return {};
}

QHash<int, QByteArray> BluetoothManager::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[NameRole] = "deviceName";
    roles[ObjectPathRole] = "objectPath";
    roles[AddressRole] = "deviceAddress";
    roles[ConnectedRole] = "connected";
    roles[PairedRole] = "paired";
    roles[TrustedRole] = "trusted";

    return roles;
}


// ============================================================================
// Блок: управление поиском устройств
// Block: device discovery management
// ============================================================================
bool BluetoothManager::IsDiscovery()
{
    return discoveryActive;
}

bool BluetoothManager::StartDiscovery()
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
        (
            this->connection,
            "org.bluez",
            "/org/bluez/hci0",
            "org.bluez.Adapter1",
            "StartDiscovery",
            nullptr,
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error
            );

    if (result == nullptr)
    {
        PrintError(error);
        return false;
    }

    discoveryActive = true;
    emit discoveryActiveChanged();
    discoveryTimer.start(20000);
    g_variant_unref(result);
    return true;
}

bool BluetoothManager::StopDiscovery()
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
        (
            this->connection,
            "org.bluez",
            "/org/bluez/hci0",
            "org.bluez.Adapter1",
            "StopDiscovery",
            nullptr,
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error
            );

    if (result == nullptr)
    {
        PrintError(error);
        return false;
    }

    discoveryActive = false;
    emit discoveryActiveChanged();
    discoveryTimer.stop();
    g_variant_unref(result);
    return true;
}


// ============================================================================
// Блок: подключение, отключение и сопряжение устройств
// Block: connecting, disconnecting, and pairing devices
// ============================================================================
void BluetoothManager::ConnectToDevice(const QString& objectPath)
{
    for (const BluetoothDevice &device : devices)
    {
        if (device.objectPath == objectPath) 
        {
            if (device.connected) 
            {
                DisconnectDevice(objectPath);
                return;
            }
            else if (device.paired == true && device.connected == false) 
            {
                ConnectDevice(objectPath);
                return;
            }
            else if (device.paired == false && device.connected == false)
            {
                PairDevice(objectPath);
                return;
            }

            break;
        }
    }
}

void BluetoothManager::ConnectDevice(const QString& objectPath)
{
    g_dbus_connection_call
    (
        this->connection,
        "org.bluez",
        objectPath.toStdString().c_str(),
        "org.bluez.Device1",
        "Connect",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        [](GObject* source, GAsyncResult* result, gpointer userData)
        {
            GError* error = nullptr;

            GVariant* reply = g_dbus_connection_call_finish
            (
                G_DBUS_CONNECTION(source),
                result,
                &error
            );

            if (reply == nullptr)
            {
                g_printerr("Ошибка подключения: %s\n", error ? error->message : "Неизвестная ошибка");
                if (error != nullptr) g_error_free(error);

                return;
            }

            g_variant_unref(reply);
        },
        nullptr
    );
}

void BluetoothManager::DisconnectDevice(const QString& objectPath)
{
    g_dbus_connection_call
    (
        this->connection,
        "org.bluez",
        objectPath.toStdString().c_str(),
        "org.bluez.Device1",
        "Disconnect",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        [](GObject* source, GAsyncResult* result, gpointer userData)
        {
            GError* error = nullptr;

            GVariant* reply = g_dbus_connection_call_finish
            (
                G_DBUS_CONNECTION(source),
                result,
                &error
            );

            if (reply == nullptr)
            {
                g_printerr("Ошибка подключения: %s\n", error ? error->message : "Неизвестная ошибка");
                if (error != nullptr) g_error_free(error);

                return;
            }

            g_variant_unref(reply);
        },
        nullptr
    );

}

void BluetoothManager::PairDevice(const QString& objectPath)
{
    g_dbus_connection_call
    (
        this->connection,
        "org.bluez",
        objectPath.toStdString().c_str(),
        "org.bluez.Device1",
        "Pair",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        [](GObject* source, GAsyncResult* result, gpointer userData)
        {
            GError* error = nullptr;

            GVariant* reply = g_dbus_connection_call_finish
            (
                G_DBUS_CONNECTION(source),
                result,
                &error
            );

            if (reply == nullptr)
            {
                g_printerr("Ошибка подключения: %s\n", error ? error->message : "Неизвестная ошибка");
                if (error != nullptr) g_error_free(error);

                return;
            }

            g_variant_unref(reply);
        },
        nullptr
    );
}