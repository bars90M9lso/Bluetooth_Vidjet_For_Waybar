#include "bluetoothmanager.h"
#include "BluetoothAgent.h"

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
#include <QQuickWindow>
#include <QMargins>
#include <QSize>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    BluetoothManager bluetoothManager;

    QObject::connect
    (
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    QObject::connect(
        bluetoothManager.GetBluetoothAgent(),
        &BluetoothAgent::ConfirmationRequested,
        &app,
        [&engine](const QString& devicePath, quint32 passkey) {
            const auto roots = engine.rootObjects();

            if (roots.isEmpty()) {
                qWarning() << "QML-окно ещё не создано";
                return;
            }

            QObject* root = roots.constFirst();

            const bool invoked = QMetaObject::invokeMethod(
                root,
                "showConfirmation",
                Q_ARG(QVariant, QVariant(devicePath)),
                Q_ARG(QVariant, QVariant(passkey))
            );

            if (!invoked) {
                qWarning() << "Не удалось вызвать showConfirmation()";
            }
        }
    );

    QObject::connect(
        bluetoothManager.GetBluetoothAgent(),
        &BluetoothAgent::PasskeyRequested,
        &app,
        [&engine](const QString& devicePath) {
            const auto roots = engine.rootObjects();

            if (roots.isEmpty()) {
                qWarning() << "QML-окно ещё не создано";
                return;
            }

            QObject* root = roots.constFirst();

            const bool invoked = QMetaObject::invokeMethod(
                root,
                "showPasskeyDialog",
                Q_ARG(QVariant, QVariant(devicePath))
            );

            if (!invoked) {
                qWarning() << "Не удалось вызвать PasskeyRequested()";
            }
        }
    );

    QObject::connect(
        bluetoothManager.GetBluetoothAgent(),
        &BluetoothAgent::CancelRequested,
        &app,
        [&engine]()
        {
            const auto roots = engine.rootObjects();

            if (roots.isEmpty())
                return;

            QObject* root = roots.constFirst();

            const bool invoked = QMetaObject::invokeMethod(
                root,
                "onCancelRequested"
            );

            if (!invoked)
                qWarning() << "Не удалось вызвать onCancelRequested()";
        }
    );

    engine.rootContext()->setContextProperty
    (
        "bluetoothManager",
        &bluetoothManager
    );

    engine.rootContext()->setContextProperty(
        "bluetoothAgent",
        bluetoothManager.GetBluetoothAgent()
    );


    
    bluetoothManager.updateDeviceList();

    engine.loadFromModule("BlVidject", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    auto *window = qobject_cast<QQuickWindow *>(
        engine.rootObjects().constFirst()
    );

    if (!window) {
        qWarning() << "Не удалось получить QQuickWindow";
        return -1;
    }

    auto *layerWindow = LayerShellQt::Window::get(window);

    if (!layerWindow) {
        qWarning() << "Layer Shell недоступен для окна";
        return -1;
    }

    layerWindow->setLayer(
        LayerShellQt::Window::LayerTop
    );

    layerWindow->setAnchors(
        LayerShellQt::Window::Anchor(
            LayerShellQt::Window::AnchorTop |
            LayerShellQt::Window::AnchorRight
        )
    );

    layerWindow->setMargins(QMargins(0, 30, 10, 0));
    layerWindow->setDesiredSize(QSize(500, 250));
    layerWindow->setExclusiveZone(-1);

    window->show();

    return app.exec();
}
