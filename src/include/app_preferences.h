// app_preferences.h

#ifndef APP_PREFERENCES_H
#define APP_PREFERENCES_H

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QString>

#include "app_paths.h"

// Window-level preferences in preferences.json. They apply at once and never
// need a restart.
struct AppPreferences
{
    // Start Rendrop in the tray when the user signs in to Windows.
    bool launchAtLogin = true;
    // Soft drop shadows under the large cards.
    bool shadows = true;
    // The "keeps running in the tray" balloon has been shown once already.
    bool trayHintShown = false;
    // Blender processes that may read .blend files at the same time when
    // files are added or reloaded. The rest wait in the Loading state.
    int maxConcurrentLoads = 2;
};

namespace AppPreferencesLimits {
inline constexpr int minConcurrentLoads = 1;
inline constexpr int maxConcurrentLoads = 16;
}

namespace AppPreferencesFile {

inline QString path()
{
    return AppPaths::configFile(QStringLiteral("preferences.json"));
}

inline AppPreferences load()
{
    AppPreferences preferences;
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly))
    {
        return preferences;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    preferences.launchAtLogin = object.value(QStringLiteral("launchAtLogin")).toBool(preferences.launchAtLogin);
    preferences.shadows = object.value(QStringLiteral("shadows")).toBool(preferences.shadows);
    preferences.trayHintShown = object.value(QStringLiteral("trayHintShown")).toBool(preferences.trayHintShown);
    preferences.maxConcurrentLoads = std::clamp(
        object.value(QStringLiteral("maxConcurrentLoads")).toInt(preferences.maxConcurrentLoads),
        AppPreferencesLimits::minConcurrentLoads,
        AppPreferencesLimits::maxConcurrentLoads);
    return preferences;
}

inline bool save(const AppPreferences &preferences)
{
    QSaveFile file(path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    const QJsonObject object {
        {QStringLiteral("launchAtLogin"), preferences.launchAtLogin},
        {QStringLiteral("shadows"), preferences.shadows},
        {QStringLiteral("trayHintShown"), preferences.trayHintShown},
        {QStringLiteral("maxConcurrentLoads"), preferences.maxConcurrentLoads}
    };
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

}

// Per-user autostart through HKCU\...\CurrentVersion\Run; no elevation needed.
namespace Autostart {

inline constexpr char trayArgument[] = "--tray";

inline QString runKey()
{
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

inline QString command()
{
    const QString executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return QStringLiteral("\"%1\" %2").arg(executable, QLatin1String(trayArgument));
}

// True for the copy the installer put in place (Inno Setup leaves its
// uninstaller next to it). Development builds must not take over the Run
// entry just by starting.
inline bool isInstalledCopy()
{
    return QFileInfo::exists(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("unins000.exe")));
}

inline void setEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings run(runKey(), QSettings::NativeFormat);
    if (enabled)
    {
        run.setValue(QStringLiteral("Rendrop"), command());
    }
    else
    {
        run.remove(QStringLiteral("Rendrop"));
    }
#else
    Q_UNUSED(enabled);
#endif
}

// Startup sync: the installed copy keeps the entry in line with the
// preference and with its current path.
inline void sync(const AppPreferences &preferences)
{
    if (isInstalledCopy())
    {
        setEnabled(preferences.launchAtLogin);
    }
}

}

#endif // APP_PREFERENCES_H
