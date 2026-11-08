// main.cpp

#include <QApplication>
#include <QFontDatabase>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QTranslator>
#include <QFile>
#include <QString>
#include <QSystemTrayIcon>

#include "app_paths.h"
#include "app_preferences.h"
#include "frame_driver.h"
#include "rendrop_style.h"
#include "widget.h"

namespace {

// One instance per user session. A second launch hands over to the first
// one, which shows its window, and exits.
const QString instanceName = QStringLiteral("RendropInstance");

bool signalRunningInstance()
{
    QLocalSocket socket;
    socket.connectToServer(instanceName);
    if (!socket.waitForConnected(500))
    {
        return false;
    }
    socket.write("show\n");
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return true;
}

}

int main(int argc, char *argv[])
{
    // Required by QWindowKit: native child widgets must not turn their
    // siblings native, or the custom frame loses its hit testing.
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    Look::selectFontEngine();
    QApplication app(argc, argv);
    // Before any widget exists, so every widget is polished by it.
    RendropStyle::install();
    Look::installTextRendering();
    FrameDriver::installForScreen();
    QCoreApplication::setApplicationName(QStringLiteral("Rendrop"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0.3"));
    // The window hides to the tray; message boxes closing while it is hidden
    // must not end the process. Quitting goes through the tray menu.
    app.setQuitOnLastWindowClosed(false);

    if (signalRunningInstance())
    {
        return 0;
    }
    QLocalServer instanceServer;
    // Clears a stale endpoint left by a crashed instance (Unix sockets).
    QLocalServer::removeServer(instanceName);
    instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
    instanceServer.listen(instanceName);

    // Started by the Run entry at login: stay in the tray.
    bool startInTray = app.arguments().contains(QLatin1String(Autostart::trayArgument));

    QFontDatabase::addApplicationFont(":/font/MiOutfit.ttf");
    QFontDatabase::addApplicationFont(":/font/JetBrainsMono-Regular.ttf");
    QFontDatabase::addApplicationFont(":/font/JetBrainsMono-Bold.ttf");
    QFontDatabase::addApplicationFont(":/font/BitcountSingle-Bold.ttf");

    if (!AppPaths::ensureConfigDirectory())
    {
        return 1;
    }
    Autostart::sync(AppPreferencesFile::load());

    int exitCode = 0;
    do {
        QTranslator translator;
        // Qt's own strings: message box buttons, text edit context menus.
        QTranslator qtTranslator;
        int languageIndex = 0;

        QFile configFile(AppPaths::configFile(QStringLiteral("language.cfg")));
        if (!configFile.exists())
        {
            if (configFile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                configFile.write("en_US\n");
                configFile.close();
            }
        }
        else
        {
            if (configFile.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QString language = configFile.readLine().trimmed();
                if (language == "zh_CN")
                {
                    if (translator.load(":/i18n/zh_CN.qm"))
                    {
                        app.installTranslator(&translator);
                        languageIndex = 1;
                    }
                    if (qtTranslator.load(":/i18n/qtbase_zh_CN.qm"))
                    {
                        app.installTranslator(&qtTranslator);
                    }
                }
                configFile.close();
            }
        }

        {
            Widget widget(languageIndex);
            QPointer<Widget> target(&widget);
            const QMetaObject::Connection handover = QObject::connect(
                &instanceServer, &QLocalServer::newConnection, &widget, [&instanceServer, target]() {
                    while (QLocalSocket *socket = instanceServer.nextPendingConnection())
                    {
                        socket->deleteLater();
                        if (target)
                        {
                            target->showFromTray();
                        }
                    }
                });
            // Login launch stays hidden; the widget waits for the taskbar to
            // put its tray icon up. A normal launch shows the window.
            if (startInTray)
            {
                widget.startInBackground();
            }
            else
            {
                widget.show();
            }
            // A restart (language change, farm settings) always shows the window.
            startInTray = false;
            exitCode = app.exec();
            QObject::disconnect(handover);
        }

        app.removeTranslator(&translator);
        app.removeTranslator(&qtTranslator);

    } while (exitCode == 773);

    return exitCode;
}
