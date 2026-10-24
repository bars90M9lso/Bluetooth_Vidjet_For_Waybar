#include "BluetoothAgent.h"
#include <QString>

// ============================================================================
// Блок: Конструктор, деструктор 
// Block: Constructor, destructor
// ============================================================================
BluetoothAgent::BluetoothAgent(GDBusConnection* connection, QObject* parent): QObject{parent}, connection(connection)
{
    if (connection == nullptr) return;
    qDebug() << "BluetoothAgent: подключение к Agent API передано";

    registrationId = RegisterAgentObject();
    if (registrationId == 0) return;
    RegisterAgent();
    qDebug() << "BluetoothAgent: Id регистрации получен";
}

BluetoothAgent::~BluetoothAgent()
{
    if (pendingPinInvocation != nullptr)
    {
        g_dbus_method_invocation_return_dbus_error
        (
            pendingPinInvocation,
            "org.bluez.Error.Canceled",
            "BluetoothAgent уничтожен"
        );

        g_object_unref(pendingPinInvocation);
        pendingPinInvocation = nullptr;
    }

    if (pendingConfirmationInvocation != nullptr)
    {
        g_dbus_method_invocation_return_dbus_error
        (
            pendingConfirmationInvocation,
            "org.bluez.Error.Canceled",
            "BluetoothAgent уничтожен"
        );

        g_object_unref(pendingConfirmationInvocation);
        pendingConfirmationInvocation = nullptr;
    }

    if (pendingPasskeyInvocation != nullptr)
    {
        g_dbus_method_invocation_return_dbus_error
        (
            pendingPasskeyInvocation,
            "org.bluez.Error.Canceled",
            "BluetoothAgent уничтожен"
        );

        g_object_unref(pendingPasskeyInvocation);
        pendingPasskeyInvocation = nullptr;
    }

    if (connection != nullptr && registrationId != 0)
    {
        g_dbus_connection_unregister_object
        (
            connection,
            registrationId
        );
    }
}

static void PrintError(GError* error)
{
    qDebug() << (error ? error->message : "неизвестная ошибка") << '\n';
    if (error) g_error_free(error);
}

void BluetoothAgent::AgentMethodCall(GDBusConnection* connection, const gchar* sender, const gchar* objectPath, const gchar* interfaceName, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData)
{
    auto* agent = static_cast<BluetoothAgent*>(userData);
    qDebug() << "AgentMethodCall: метод =" << methodName;
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

        if (agent->pendingPinInvocation != nullptr)
        {
            g_dbus_method_invocation_return_dbus_error
            (
                invocation,
                "org.bluez.Error.Rejected",
                "Другой запрос PIN-кода уже ожидает ответа"
            );
            return;
        }
        

        agent->pendingPinInvocation = G_DBUS_METHOD_INVOCATION(g_object_ref(invocation));
        emit agent->PinCodeRequested(QString::fromUtf8(devicePath));

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
        
        if (agent->pendingConfirmationInvocation != nullptr)
        {
            g_dbus_method_invocation_return_dbus_error(
                invocation,
                "org.bluez.Error.Rejected",
                "Другой запрос подтверждения уже ожидает ответа"
            );
            return;
        }
        qDebug() << "Agent: получен RequestConfirmation";
        agent->pendingConfirmationInvocation =
        G_DBUS_METHOD_INVOCATION(g_object_ref(invocation));
        qDebug() << "Agent: отправляю ConfirmationRequested:" << devicePath << passkey;
        emit agent->ConfirmationRequested(QString::fromUtf8(devicePath), passkey);

        return;
    }

    if (g_strcmp0(methodName, "RequestPasskey") == 0)
    {
        const gchar* devicePath = nullptr;
        g_variant_get(parameters, "(&o)", &devicePath);

        if (agent->pendingPasskeyInvocation != nullptr)
        {
            g_dbus_method_invocation_return_dbus_error(
                invocation,
                "org.bluez.Error.Rejected",
                "Другой запрос числового ключа уже ожидает ответа"
            );
            return;
        }

        agent->pendingPasskeyInvocation = G_DBUS_METHOD_INVOCATION(g_object_ref(invocation));
        emit agent->PasskeyRequested(QString::fromUtf8(devicePath));
        return;
    }

    if (g_strcmp0(methodName, "RequestAuthorization") == 0)
    {
        const gchar* devicePath = nullptr;

        g_variant_get(parameters, "(&o)", &devicePath);
        QString answer;
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

        QString answer;
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
        qDebug() << "BluetoothAgent: сопряжение отменено BlueZ";
        qDebug() << "BluetoothAgent: сигнал CancelRequested отправлен";
        
        if (agent->pendingPinInvocation != nullptr)
        {
            g_dbus_method_invocation_return_dbus_error(
                agent->pendingPinInvocation,
                "org.bluez.Error.Canceled",
                "Запрос PIN-кода отменён"
            );

            g_object_unref(agent->pendingPinInvocation);
            agent->pendingPinInvocation = nullptr;
        }

        if (agent->pendingConfirmationInvocation != nullptr)
        {
            g_dbus_method_invocation_return_dbus_error(
                agent->pendingConfirmationInvocation,
                "org.bluez.Error.Canceled",
                "Запрос отменён"
            );

            g_object_unref(agent->pendingConfirmationInvocation);
            agent->pendingConfirmationInvocation = nullptr;
        }

        if (agent->pendingPasskeyInvocation != nullptr)
        {
            g_dbus_method_invocation_return_dbus_error(
                agent->pendingPasskeyInvocation,
                "org.bluez.Error.Canceled",
                "Запрос отменён"
            );

            g_object_unref(agent->pendingPasskeyInvocation);
            agent->pendingPasskeyInvocation = nullptr;
        }

        g_dbus_method_invocation_return_value(
            invocation,
            nullptr
        );
        
        emit agent->CancelRequested();
        return;
    }

    g_dbus_method_invocation_return_dbus_error
    (
        invocation,
        "org.bluez.Error.Rejected",
        "Метод Agent пока не поддерживается"
    );
}

void BluetoothAgent::ReplyPinCode(const QString& pinCode)
{
    if (pendingPinInvocation == nullptr) return;

    const QByteArray pin = pinCode.toUtf8();

    g_dbus_method_invocation_return_value
    (
        pendingPinInvocation,
        g_variant_new("(s)", pin.constData())
    );

    g_object_unref(pendingPinInvocation);
    pendingPinInvocation = nullptr;
}


void BluetoothAgent::ReplyConfirmation(bool accepted)
{
    qDebug() << "ReplyConfirmation:"
         << "accepted =" << accepted
         << "pending =" << (pendingConfirmationInvocation != nullptr);
         
    if (pendingConfirmationInvocation == nullptr) return;

    if (accepted) 
    {   
        qDebug() << "ReplyConfirmation: Успех";
        g_dbus_method_invocation_return_value(pendingConfirmationInvocation, nullptr);
    }
    else g_dbus_method_invocation_return_dbus_error(pendingConfirmationInvocation, "org.bluez.Error.Rejected", "Пользователь отклонил подтверждение");

    g_object_unref(pendingConfirmationInvocation);
    pendingConfirmationInvocation = nullptr;
}

void BluetoothAgent::ReplyPasskey(const QString& passkey)
{
    if (pendingPasskeyInvocation == nullptr)
        return;

    bool ok = false;
    uint value = passkey.toUInt(&ok);

    if (!ok || passkey.length() != 6 || value > 999999)
        return;

    g_dbus_method_invocation_return_value(
        pendingPasskeyInvocation,
        g_variant_new("(u)", value)
    );

    g_object_unref(pendingPasskeyInvocation);
    pendingPasskeyInvocation = nullptr;
}

void BluetoothAgent::RejectPasskey()
{
    if (pendingPasskeyInvocation == nullptr)
        return;

    g_dbus_method_invocation_return_dbus_error(
        pendingPasskeyInvocation,
        "org.bluez.Error.Rejected",
        "Пользователь отменил ввод числового ключа"
    );

    g_object_unref(pendingPasskeyInvocation);
    pendingPasskeyInvocation = nullptr;
}



guint BluetoothAgent::RegisterAgentObject()
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

    registrationId = g_dbus_connection_register_object
    (
        this->connection,
        "/com/blvidject/agent",
        interfaceInfo,
        &vtable,
        this,
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

void BluetoothAgent::RegisterAgent()
{
    g_dbus_connection_call
    (
        this->connection,
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
            auto* agent = static_cast<BluetoothAgent*>(userData);
            agent->RequestDefaultAgent();

        },
        this
    );
}

void BluetoothAgent::RequestDefaultAgent()
{
    g_dbus_connection_call
    (
        this->connection,
        "org.bluez",
        "/org/bluez",
        "org.bluez.AgentManager1",
        "RequestDefaultAgent",
        g_variant_new("(o)", "/com/blvidject/agent"),
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
            qDebug() << "BluetoothAgent: агент назначен по умолчанию";
        },
        nullptr
    );
}