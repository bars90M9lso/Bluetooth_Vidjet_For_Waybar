#ifndef BLUETOOTHAGENT_H
#define BLUETOOTHAGENT_H

#include <gio/gio.h>
#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QVariant>
#include <QTimer>

class BluetoothAgent : public QObject
{
    Q_OBJECT

    public:
        // ===== Конструктор и деструктор =====
        // ===== Constructor, destructor =====
        explicit BluetoothAgent(GDBusConnection* connection, QObject* parent = nullptr);
        ~BluetoothAgent() override;
        
        Q_INVOKABLE void ReplyPinCode(const QString& pinCode);
        Q_INVOKABLE void ReplyConfirmation(bool accepted);
        Q_INVOKABLE void ReplyPasskey(const QString& passkey);
        Q_INVOKABLE void RejectPasskey();
        
    private:
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

        guint registrationId = 0;
        GDBusConnection* connection = nullptr;

        guint RegisterAgentObject();
        static void AgentMethodCall(GDBusConnection* connection, const gchar* sender, const gchar* objectPath, const gchar* interfaceName, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData);
        void RegisterAgent();
        void RequestDefaultAgent();

        GDBusMethodInvocation* pendingPinInvocation = nullptr;
        GDBusMethodInvocation* pendingConfirmationInvocation = nullptr;
        GDBusMethodInvocation* pendingPasskeyInvocation = nullptr;

    signals:
        // ===== Сигналы уведомлений =====
        // ===== Notification signals ====
        void PinCodeRequested(const QString& devicePath);
        void ConfirmationRequested(const QString& devicePath, quint32 passkey);
        void PasskeyRequested(const QString& devicePath);

        void CancelRequested();
};

#endif // BLUETOOTHAGENT_H
