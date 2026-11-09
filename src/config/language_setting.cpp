// language_setting.cpp

#include <QFile>
#include <QLocale>
#include <QStringList>

#include "config/app_paths.h"
#include "config/language_setting.h"

namespace LanguageSetting {

namespace {
const QString english = QStringLiteral("en_US");
const QString chinese = QStringLiteral("zh_CN");

// The Windows display language, which can differ from the format locale
// QLocale::system() names. Any Chinese gets the Chinese UI.
QString systemDefault()
{
    const QStringList languages = QLocale::system().uiLanguages();
    const QLocale preferred = languages.isEmpty() ? QLocale::system() : QLocale(languages.first());
    return preferred.language() == QLocale::Chinese ? chinese : english;
}
}

QString load()
{
    QFile file(AppPaths::languageFile());
    if (!file.exists())
    {
        // First run: follow the system, then keep that choice.
        const QString locale = systemDefault();
        save(locale);
        return locale;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return english;
    }
    return QString::fromUtf8(file.readLine()).trimmed();
}

bool save(const QString &locale)
{
    if (!AppPaths::ensureConfigDirectory())
    {
        return false;
    }
    QFile file(AppPaths::languageFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }
    return file.write((locale + QLatin1Char('\n')).toUtf8()) >= 0;
}

QString localeForIndex(int index)
{
    return index == Chinese ? chinese : english;
}

int indexForLocale(const QString &locale)
{
    return locale == chinese ? Chinese : English;
}

}
