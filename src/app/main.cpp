// main.cpp

#include <QApplication>
#include <QFontDatabase>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QTranslator>
#include <QString>
#include <QSystemTrayIcon>

#include "app/app_controller.h"
#include "config/app_paths.h"
#include "config/app_preferences.h"
#include "config/language_setting.h"
#include "ui/motion/frame_driver.h"
#include "ui/style/look_widgets.h"
#include "ui/style/rendrop_style.h"
#include "ui/style/theme.h"
#include "ui/window/widget.h"

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
    const AppPreferences preferences = AppPreferencesFile::load();
    Autostart::sync(preferences);
    // Before the first window, so it opens in its scheme without a fade.
    Theme::init(Theme::modeFromName(preferences.theme));

    int exitCode = 0;
    do {
        // Every exec() takes the driver off as it returns.
        FrameDriver::installForScreen();
        QTranslator translator;
        // Qt's own strings: message box buttons, text edit context menus.
        QTranslator qtTranslator;
        int languageIndex = LanguageSetting::English;
        if (LanguageSetting::load() == LanguageSetting::localeForIndex(LanguageSetting::Chinese))
        {
            if (translator.load(QStringLiteral(":/i18n/zh_CN.qm")))
            {
                app.installTranslator(&translator);
                languageIndex = LanguageSetting::Chinese;
            }
            if (qtTranslator.load(QStringLiteral(":/i18n/qtbase_zh_CN.qm")))
            {
                app.installTranslator(&qtTranslator);
            }
        }

        {
            // Declared first so it goes last: the window drives it.
            AppController controller;
            Widget widget(&controller, languageIndex);
            controller.start();
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
