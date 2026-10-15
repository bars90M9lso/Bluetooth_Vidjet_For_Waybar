#include "bluetoothmanager.h"
#include "QDebug"

#include <gio/gio.h>

// Конструктор //
BluetoothManager::BluetoothManager(QObject *parent): QObject{parent}
{
    connection = ConnectToDBus();
    if (connection == nullptr) return;
    qDebug() << "BluetoothManager: подключение к D-Bus успешно";
    
    bluetoothEnabled = ReadBluetoothEnabled();
    qDebug() << "BluetoothManager: Состояние bluetoooh" << bluetoothEnabled;

    signalSubscriptionId = GetSignalSubscriptionId();
    if (signalSubscriptionId == 0) return;
    qDebug() << "BluetoothManager: Подписка получена";
}

// Деструктор //
BluetoothManager::~BluetoothManager()
{
    if (connection != nullptr && signalSubscriptionId != 0)
    {
        g_dbus_connection_signal_unsubscribe(
            connection,
            signalSubscriptionId
        );
    }

    if (connection != nullptr) g_object_unref(connection);
}

// Внутрение функции //
void PrintError(GError* error)
{
    qDebug() << (error ? error->message : "неизвестная ошибка") << '\n';
    if (error) g_error_free(error);
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

guint BluetoothManager::GetSignalSubscriptionId()
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
                auto* self = static_cast<BluetoothManager*>(userData);

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
                    {
                        if (self->bluetoothEnabled != powered)
                        {
                            self->bluetoothEnabled = powered;
                            emit self->bluetoothEnabledChanged();
                        }
                    }
                }

                g_variant_unref(changedProperties);
                g_variant_unref(invalidatedProperties);
            },
     
        this,
        nullptr
    );
}

// Внешние функции //

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




/*
#include <iostream>
#include <vector>
#include <string>

#include <thread>
#include <chrono>
#include <mutex>

#include <gio/gio.h>
#include <cstring>

using namespace std;

std::mutex inputMutex;
const char* agentXml =
    R"(
    <node>
        <interface name="org.bluez.Agent1">
            <method name="Release"/>

            <method name="RequestPinCode">
                <arg name="device" type="o" direction="in"/>
                <arg name="pincode" type="s" direction="out"/>
            </method>

            <method name="RequestPasskey">
                <arg name="device" type="o" direction="in"/>
                <arg name="passkey" type="u" direction="out"/>
            </method>

            <method name="RequestConfirmation">
                <arg name="device" type="o" direction="in"/>
                <arg name="passkey" type="u" direction="in"/>
            </method>

            <method name="RequestAuthorization">
                <arg name="device" type="o" direction="in"/>
            </method>

            <method name="AuthorizeService">
                <arg name="device" type="o" direction="in"/>
                <arg name="uuid" type="s" direction="in"/>
            </method>

            <method name="Cancel"/>
        </interface>
    </node>
)";


struct BluetoothDevice
{
    string name;
    string address;
    bool connected = false;
    bool paired = false;
    bool trusted = false;
    string objectPath;
};
vector<BluetoothDevice> devices;



static void AgentMethodCall(GDBusConnection* connection, const gchar* sender, const gchar* objectPath, const gchar* interfaceName, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData)
{
    if (g_strcmp0(methodName, "Release") == 0)
    {
        g_dbus_method_invocation_return_value(
            invocation,
            nullptr
            );
        return;
    }

    if (g_strcmp0(methodName, "RequestPinCode") == 0)
    {
        const gchar* devicePath = nullptr;
        g_variant_get(parameters, "(&o)", &devicePath);

        string pinCode;
        {
            std::lock_guard<std::mutex> lock(inputMutex);

            cout << "\nВведите PIN-код для устройства " << devicePath << ": ";

            cin >> pinCode;
        }

        g_dbus_method_invocation_return_value
            (
                invocation,
                g_variant_new("(s)", pinCode.c_str())
                );

        return;
    }

    if (g_strcmp0(methodName, "RequestConfirmation") == 0)
    {
        const gchar* devicePath = nullptr;
        guint32 passkey = 0;

        g_variant_get(
            parameters,
            "(&ou)",
            &devicePath,
            &passkey
            );

        cout << "\nУстройство: " << devicePath << '\n';
        cout << "Код подтверждения: " << passkey << '\n';
        cout << "Совпадает ли код на обоих устройствах? (y/n): ";

        char answer;
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            cin >> answer;
        }

        if (answer == 'y' || answer == 'Y')
        {
            g_dbus_method_invocation_return_value(
                invocation,
                nullptr
                );
        }
        else
        {
            g_dbus_method_invocation_return_dbus_error(
                invocation,
                "org.bluez.Error.Rejected",
                "Пользователь отклонил подтверждение"
                );
        }

        return;
    }


    if (g_strcmp0(methodName, "RequestPasskey") == 0)
    {
        const gchar* devicePath = nullptr;

        g_variant_get(
            parameters,
            "(&o)",
            &devicePath
            );

        string input;
        {
            std::lock_guard<std::mutex> lock(inputMutex);

            cout << "\nУстройство: " << devicePath << '\n';
            cout << "Введите числовой ключ (0–999999): ";

            cin >> input;
        }

        bool valid = !input.empty();

        for (char c : input)
        {
            if (c < '0' || c > '9')
            {
                valid = false;
                break;
            }
        }

        if (!valid || input.size() > 6)
        {
            g_dbus_method_invocation_return_dbus_error(
                invocation,
                "org.bluez.Error.Rejected",
                "Некорректный числовой ключ"
                );

            return;
        }

        guint32 passkey = 0;

        for (char c : input)
        {
            passkey = passkey * 10 + (c - '0');
        }

        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(u)", passkey)
            );

        return;
    }

    if (g_strcmp0(methodName, "RequestAuthorization") == 0)
    {
        const gchar* devicePath = nullptr;

        g_variant_get(parameters, "(&o)", &devicePath);

        char answer;
        {
            std::lock_guard<std::mutex> lock(inputMutex);

            cout << "\nУстройство запрашивает разрешение:\n" << devicePath << '\n';
            cout << "Разрешить? (y/n): ";

            cin >> answer;
        }

        if (answer == 'y' || answer == 'Y')
        {
            g_dbus_method_invocation_return_value(
                invocation,
                nullptr
                );
        }
        else
        {
            g_dbus_method_invocation_return_dbus_error(
                invocation,
                "org.bluez.Error.Rejected",
                "Пользователь отклонил запрос"
                );
        }

        return;
    }


    if (g_strcmp0(methodName, "AuthorizeService") == 0)
    {
        const gchar* devicePath = nullptr;
        const gchar* uuid = nullptr;

        g_variant_get(
            parameters,
            "(&o&s)",
            &devicePath,
            &uuid
            );

        char answer;
        {
            std::lock_guard<std::mutex> lock(inputMutex);

            cout << "\nУстройство: " << devicePath << '\n';
            cout << "Запрашиваемый сервис: " << uuid << '\n';
            cout << "Разрешить использование сервиса? (y/n): ";

            cin >> answer;
        }

        if (answer == 'y' || answer == 'Y')
        {
            g_dbus_method_invocation_return_value(
                invocation,
                nullptr
                );
        }
        else
        {
            g_dbus_method_invocation_return_dbus_error(
                invocation,
                "org.bluez.Error.Rejected",
                "Пользователь отклонил сервис"
                );
        }

        return;
    }

    if (g_strcmp0(methodName, "Cancel") == 0)
    {
        cout << "\nСопряжение отменено BlueZ\n";

        g_dbus_method_invocation_return_value(
            invocation,
            nullptr
            );

        return;
    }

    g_dbus_method_invocation_return_dbus_error
        (
            invocation,
            "org.bluez.Error.Rejected",
            "Метод Agent пока не поддерживается"
            );
}

guint RegisterAgentObject(GDBusConnection* connection)
{
    GError* error = nullptr;

    GDBusInterfaceVTable vtable = {};
    vtable.method_call = AgentMethodCall;

    GDBusNodeInfo* nodeInfo = g_dbus_node_info_new_for_xml(agentXml, &error);

    if (nodeInfo == nullptr)
    {
        PrintError(error);
        return 0;
    }

    GDBusInterfaceInfo* interfaceInfo = g_dbus_node_info_lookup_interface
        (
            nodeInfo,
            "org.bluez.Agent1"
            );

    if (interfaceInfo == nullptr)
    {
        g_dbus_node_info_unref(nodeInfo);
        return 0;
    }

    guint registrationId = g_dbus_connection_register_object(
        connection,
        "/com/blvidject/agent",
        interfaceInfo,
        &vtable,
        nullptr,
        nullptr,
        &error
        );

    g_dbus_node_info_unref(nodeInfo);

    if (registrationId == 0)
    {
        PrintError(error);
        return 0;
    }

    return registrationId;
}

bool RegisterAgent(GDBusConnection* connection)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "org.bluez",
        "/org/bluez",
        "org.bluez.AgentManager1",
        "RegisterAgent",
        g_variant_new(
            "(os)",
            "/com/blvidject/agent",
            "KeyboardDisplay"
            ),
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

    g_variant_unref(result);
    return true;
}

bool RequestDefaultAgent(GDBusConnection* connection)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "org.bluez",
        "/org/bluez",
        "org.bluez.AgentManager1",
        "RequestDefaultAgent",
        g_variant_new("(o)", "/com/blvidject/agent"),
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

    g_variant_unref(result);
    return true;
}

bool RemoveDevice(GDBusConnection* connection, const string& path)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
        (
            connection,
            "org.bluez",
            "/org/bluez/hci0",
            "org.bluez.Adapter1",
            "RemoveDevice",
            g_variant_new("(o)", path.c_str()),
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

    cout << "Устройство удалено\n";
    g_variant_unref(result);
    return true;
}

bool StopDiscovery(GDBusConnection* connection)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
        (
            connection,
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

    cout << "Поиск Bluetooth-устройств остановлен\n";
    g_variant_unref(result);
    return true;
}

bool StartDiscovery(GDBusConnection* connection)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
        (
            connection,
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

    cout << "Поиск Bluetooth-устройств запущен\n";
    g_variant_unref(result);
    return true;
}

bool ConnectDevice(GDBusConnection* connection, const string& path)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
        (
            connection,
            "org.bluez",
            path.c_str(),
            "org.bluez.Device1",
            "Connect",
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

    g_variant_unref(result);
    return true;
}

bool DisconnectDevice(GDBusConnection* connection, const string& path)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
        (
            connection,
            "org.bluez",
            path.c_str(),
            "org.bluez.Device1",
            "Disconnect",
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

    g_variant_unref(result);
    return true;
}

bool PairDevice(GDBusConnection* connection, const string& path)
{
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync
        (
            connection,
            "org.bluez",
            path.c_str(),
            "org.bluez.Device1",
            "Pair",
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

    g_variant_unref(result);
    return true;
}

void GetListDevices(GDBusConnection* connection)
{
    GError* error = nullptr;
    devices.clear();

    GVariant* result = g_dbus_connection_call_sync
        (
            connection,
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

        devices.push_back(BluetoothDeviceTemp);

        g_variant_unref(found);
        g_free(objectPath);
        g_variant_unref(interfaces);
    }

    g_variant_unref(value);
    g_variant_unref(result);
}

vector<BluetoothDevice> GetFilteredDevices(GDBusConnection* connection, string type = "all")
{
    vector<BluetoothDevice> filteredDevices;
    GetListDevices(connection);

    int i = 1;
    for (const auto& device : devices)
    {
        if (type == "connected" && !device.connected) continue;
        else if (type == "paired" && !(device.paired && !device.connected)) continue;
        else if (type == "new" && !(!device.paired && !device.connected)) continue;

        if (device.name.empty()) continue;

        filteredDevices.push_back(device);
    }

    return filteredDevices;
}

void PrintDevices(GDBusConnection* connection, string type = "all")
{
    GetListDevices(connection);

    int i = 1;
    for (const auto& device : devices)
    {
        if (type == "connected" && !device.connected) continue;
        else if (type == "paired" && !(device.paired && !device.connected)) continue;
        else if (type == "new" && !(!device.paired && !device.connected)) continue;

        if (device.name.empty()) continue;
        cout << "Device " << i << ":\n";
        i++;
        cout << "   Name: " << device.name << '\n';
        cout << "   Address: " << device.address << '\n';
        cout << "   Connected: " << device.connected << '\n';
        cout << "   Paired: " << device.paired << '\n';
        cout << "   Trusted: " << device.trusted << '\n';
        cout << "   Path: " << device.objectPath << '\n';
        cout << "----------------\n";
    }
}


*/
/*
int main()
{
    GDBusConnection* connection = GetConnection();
    if (connection == nullptr) return 1;

    if (IsActivated(connection)) cout << "Bluetooth включён\n";
    else {cout << "Bluetooth выключен\n"; return 1;}

    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);

    guint agentRegistrationId = RegisterAgentObject(connection);

    if (agentRegistrationId == 0) return 1;
    if (!RegisterAgent(connection)) return 1;
    if (!RequestDefaultAgent(connection)) return 1;

    thread eventThread([loop]() {g_main_loop_run(loop); });

    while (true)
    {
        int choice;

        cout << "1 - Отключиться от подключёного устройства" << endl;
        cout << "2 - Подключиться к сопряжённого устройства" << endl;
        cout << "3 - Найти новые устройства" << endl;
        cout << "4 - Удалить устройство" << endl;
        cout << "0 - Выход" << endl;

        cout << "выберите вариант " << endl;;

        cin >> choice;

        if (choice == 0)
        {
            cout << "Выход из программы." << endl;

            g_main_loop_quit(loop);
            eventThread.join();

            g_dbus_connection_unregister_object(
                connection,
                agentRegistrationId
                );

            g_main_loop_unref(loop);
            g_object_unref(connection);

            return 0;
        }
        else if (choice == 1)
        {
            auto filteredDevices = GetFilteredDevices(connection, "connected");
            PrintDevices(connection, "connected");

            int index;
            cout << "Выберите устройство " << endl;;
            cin >> index;

            if (index < 1 || index > filteredDevices.size())
            {
                cout << "Неверный номер устройства\n";
                continue;
            }

            string path = filteredDevices[index - 1].objectPath;

            DisconnectDevice(connection, path);
            PrintDevices(connection, "paired");
        }
        else if (choice == 2)
        {
            auto filteredDevices = GetFilteredDevices(connection, "paired");
            PrintDevices(connection, "paired");

            int index;
            cout << "Выберите устройство " << endl;;
            cin >> index;

            if (index < 1 || index > filteredDevices.size())
            {
                cout << "Неверный номер устройства\n";
                continue;
            }

            string path = filteredDevices[index - 1].objectPath;

            ConnectDevice(connection, path);
            PrintDevices(connection, "connected");
        }
        else if (choice == 3)
        {
            if (StartDiscovery(connection))
            {
                this_thread::sleep_for(chrono::seconds(30));
                if (!(StopDiscovery(connection))) continue;
            }
            else continue;

            auto filteredDevices = GetFilteredDevices(connection, "new");
            PrintDevices(connection, "new");

            int index;
            cout << "Выберите устройство " << endl;;
            cin >> index;

            if (index < 1 || index > filteredDevices.size())
            {
                cout << "Неверный номер устройства\n";
                continue;
            }

            string path = filteredDevices[index - 1].objectPath;

            if (!(PairDevice(connection, path))) continue;
            cout << "Устройство подкл. \n";
            PrintDevices(connection, "connected");

        }
        else if (choice == 4)
        {
            auto filteredDevices = GetFilteredDevices(connection, "all");
            PrintDevices(connection, "all");

            int index;
            cout << "Выберите устройство " << endl;;
            cin >> index;

            if (index < 1 || index > filteredDevices.size())
            {
                cout << "Неверный номер устройства\n";
                continue;
            }

            string path = filteredDevices[index - 1].objectPath;

            RemoveDevice(connection, path);
            PrintDevices(connection, "all");
        }
        else
        {
            cout << "НЕправильно " << endl;
        }
    }

    return 0;
}
*/