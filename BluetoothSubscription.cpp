#include "BluetoothSubscription.h"
#include "QDebug"

#include <gio/gio.h>
#include <QString>

// ============================================================================
// Блок: Конструктор, деструктор 
// Block: Constructor, destructor
// ============================================================================
BluetoothSubscription::BluetoothSubscription(GDBusConnection* connection, QObject* parent): QObject{parent}, connection(connection)
{   
    if (connection == nullptr) return;
    qDebug() << "BluetoothSubscription: подключение к D-Bus передано";

    signalSubscriptionId = GetSignalSubscriptionId();
    if (signalSubscriptionId == 0) return;
    qDebug() << "BluetoothSubscription: Подписка получена";

    interfacesAddedSubscriptionId = InterfacesAdded();
    if (interfacesAddedSubscriptionId == 0) return;
    qDebug() << "BluetoothSubscription: Подписка на добавление интерфейсов получена";

    interfacesRemovedSubscriptionId = InterfacesRemoved();
    if (interfacesRemovedSubscriptionId == 0) return;
    qDebug() << "BluetoothSubscription: Подписка на удаление интерфейсов получена";

    devicePropertiesSubscriptionId = SubscribeToDeviceProperties();
    if (devicePropertiesSubscriptionId == 0) return;
    qDebug() << "BluetoothSubscription: Подписка на изменение свойств устройств получена";
}

BluetoothSubscription::~BluetoothSubscription()
{
    if (connection != nullptr)
    {
        if (signalSubscriptionId != 0) g_dbus_connection_signal_unsubscribe(connection, signalSubscriptionId);
        if (interfacesAddedSubscriptionId != 0) g_dbus_connection_signal_unsubscribe(connection, interfacesAddedSubscriptionId);
        if (interfacesRemovedSubscriptionId != 0) g_dbus_connection_signal_unsubscribe(connection, interfacesRemovedSubscriptionId);
        if (devicePropertiesSubscriptionId != 0) g_dbus_connection_signal_unsubscribe(connection, devicePropertiesSubscriptionId);
    }
}


// ============================================================================
// Подписка на изменения состояния адаптера
// Subscription to adapter state changes
// ============================================================================
guint BluetoothSubscription::GetSignalSubscriptionId()
{
    return g_dbus_connection_signal_subscribe
    (
        this->connection,
        "org.bluez",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        "/org/bluez/hci0",
        "org.bluez.Adapter1",
        G_DBUS_SIGNAL_FLAGS_NONE,
        
        [](GDBusConnection* connection,
            const gchar* senderName,
            const gchar* objectPath,
            const gchar* interfaceName,
            const gchar* signalName,
            GVariant* parameters,
            gpointer userData)
            {
                auto* self = static_cast<BluetoothSubscription*>(userData);

                const gchar* changedInterface = nullptr;
                GVariant* changedProperties = nullptr;
                GVariant* invalidatedProperties = nullptr;

                g_variant_get
                (
                    parameters,
                    "(&s@a{sv}@as)",
                    &changedInterface,
                    &changedProperties,
                    &invalidatedProperties
                );

                if (g_strcmp0(changedInterface, "org.bluez.Adapter1") == 0)
                {
                    gboolean powered = FALSE;

                    if (g_variant_lookup(changedProperties, "Powered", "b", &powered)) 
                        emit self->BluetoothPowerChanged(powered);
                
                }

                g_variant_unref(changedProperties);
                g_variant_unref(invalidatedProperties);
            },
     
        this,
        nullptr
    );
}


// ============================================================================
// Подписка на изменение свойств устройств
// Subscription to device property changes
// ============================================================================
guint BluetoothSubscription::SubscribeToDeviceProperties()
{
    return g_dbus_connection_signal_subscribe
    (
        this->connection,
        "org.bluez",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        nullptr,
        "org.bluez.Device1",
        G_DBUS_SIGNAL_FLAGS_NONE,
        
        [](GDBusConnection* connection,
            const gchar* senderName,
            const gchar* objectPath,
            const gchar* interfaceName,
            const gchar* signalName,
            GVariant* parameters,
            gpointer userData)
            {
                auto* self = static_cast<BluetoothSubscription*>(userData);

                const gchar* changedInterface = nullptr;
                GVariant* changedProperties = nullptr;
                GVariant* invalidatedProperties = nullptr;

                g_variant_get
                (
                    parameters,
                    "(&s@a{sv}@as)",
                    &changedInterface,
                    &changedProperties,
                    &invalidatedProperties
                );

                if (g_strcmp0(changedInterface, "org.bluez.Device1") == 0)
                {
                    bool connected = false;
                    bool connectedChanged = false;

                    bool paired = false;
                    bool pairedChanged = false;

                    GVariant* connectedValue = g_variant_lookup_value(
                        changedProperties,
                        "Connected",
                        G_VARIANT_TYPE_BOOLEAN
                    );

                    if (connectedValue != nullptr)
                    {
                        connected = g_variant_get_boolean(connectedValue);
                        connectedChanged = true;

                        g_variant_unref(connectedValue);
                    }

                    GVariant* pairedValue = g_variant_lookup_value(
                        changedProperties,
                        "Paired",
                        G_VARIANT_TYPE_BOOLEAN
                    );

                    if (pairedValue != nullptr)
                    {
                        paired = g_variant_get_boolean(pairedValue);
                        pairedChanged = true;

                        g_variant_unref(pairedValue);
                    }

                    if (connectedChanged || pairedChanged)
                    {
                        emit self->DevicePropertiesChanged(
                            QString::fromUtf8(objectPath),
                            connected,
                            connectedChanged,
                            paired,
                            pairedChanged
                        );
                    }
                }

                g_variant_unref(changedProperties);
                g_variant_unref(invalidatedProperties);
            },
     
        this,
        nullptr
    );
}


// ============================================================================
// Подписка на добавление интерфейсов устройств
// Subscription for adding device interfaces
// ============================================================================
guint BluetoothSubscription::InterfacesAdded()
{
    return g_dbus_connection_signal_subscribe
    (
        this->connection,
        "org.bluez",
        "org.freedesktop.DBus.ObjectManager",
        "InterfacesAdded",
        "/",
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        
        [](GDBusConnection* connection,
            const gchar* senderName,
            const gchar* objectPath,
            const gchar* interfaceName,
            const gchar* signalName,
            GVariant* parameters,
            gpointer userData)
            {
                auto* self = static_cast<BluetoothSubscription*>(userData);

                const gchar* addedObjectPath = nullptr;
                GVariant* interfacesAndProperties = nullptr;

                g_variant_get
                (
                    parameters,
                    "(&o@a{sa{sv}})",
                    &addedObjectPath,
                    &interfacesAndProperties
                );

                GVariant* deviceInterface = g_variant_lookup_value(
                    interfacesAndProperties,
                    "org.bluez.Device1",
                    nullptr
                );
                
                if (deviceInterface != nullptr)
                {   
                    QString name;
                    QString address;
                    bool connected = false;
                    bool paired = false;
                    bool trusted = false;

                    GVariant* nameValue = g_variant_lookup_value(deviceInterface, "Name", G_VARIANT_TYPE_STRING);
                    if (nameValue != nullptr)
                    {
                        name = g_variant_get_string(nameValue, nullptr);
                        g_variant_unref(nameValue);
                    }

                    if (name.isEmpty()) 
                    {
                        g_variant_unref(deviceInterface);
                        g_variant_unref(interfacesAndProperties);
                        return;
                    }

                    GVariant* addressValue = g_variant_lookup_value(deviceInterface, "Address", G_VARIANT_TYPE_STRING);
                    if (addressValue != nullptr)
                    {
                        address = g_variant_get_string(addressValue, nullptr);
                        g_variant_unref(addressValue);
                    }

                    GVariant* connectedValue = g_variant_lookup_value(deviceInterface, "Connected", G_VARIANT_TYPE_BOOLEAN);
                    if (connectedValue != nullptr)
                    {
                        connected = g_variant_get_boolean(connectedValue);
                        g_variant_unref(connectedValue);
                    }

                    GVariant* pairedValue = g_variant_lookup_value(deviceInterface, "Paired", G_VARIANT_TYPE_BOOLEAN);
                    if (pairedValue != nullptr)
                    {
                        paired = g_variant_get_boolean(pairedValue);
                        g_variant_unref(pairedValue);
                    }

                    GVariant* trustedValue = g_variant_lookup_value(deviceInterface, "Trusted", G_VARIANT_TYPE_BOOLEAN);
                    if (trustedValue != nullptr)
                    {
                        trusted = g_variant_get_boolean(trustedValue);
                        g_variant_unref(trustedValue);
                    }

                    emit self->DeviceAdded
                    (
                        QString::fromUtf8(addedObjectPath),
                        name,
                        address,
                        connected,
                        paired,
                        trusted
                    );
                                    
                    g_variant_unref(deviceInterface);
                }

                g_variant_unref(interfacesAndProperties);
            },
        
        this,
        nullptr
    );
}


// ============================================================================
// Подписка на удаление интерфейсов устройств
// Subscription for device interface removal
// ============================================================================
guint BluetoothSubscription::InterfacesRemoved()
{
    return g_dbus_connection_signal_subscribe(
        this->connection,
        "org.bluez",
        "org.freedesktop.DBus.ObjectManager",
        "InterfacesRemoved",
        "/",
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,

         [](GDBusConnection* connection,
            const gchar* senderName,
            const gchar* objectPath,
            const gchar* interfaceName,
            const gchar* signalName,
            GVariant* parameters,
            gpointer userData)
            {
                auto* self = static_cast<BluetoothSubscription*>(userData);

                const gchar* removedObjectPath = nullptr;
                GVariant* removedInterfaces = nullptr;

                g_variant_get(
                    parameters,
                    "(&o@as)",
                    &removedObjectPath,
                    &removedInterfaces
                );

                GVariantIter iter;
                const gchar* removedInterface = nullptr;

                g_variant_iter_init(&iter, removedInterfaces);

                while (g_variant_iter_next(&iter, "&s", &removedInterface))
                {
                    if (g_strcmp0(removedInterface, "org.bluez.Device1") == 0)
                    {
                        emit self->DeviceRemoved(QString::fromUtf8(removedObjectPath));
                        break;
                    }
                }

                g_variant_unref(removedInterfaces);
            },

        this,
        nullptr
    );
}