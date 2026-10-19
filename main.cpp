#include "bluetoothmanager.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    QObject::connect
    (
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    BluetoothManager bluetoothManager;
    engine.rootContext()->setContextProperty
    (
        "bluetoothManager",
        &bluetoothManager
    );

    bluetoothManager.updateDeviceList();

    engine.loadFromModule("BlVidject", "Main");
    return QGuiApplication::exec();
}
