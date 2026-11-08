// language_setting.cpp

#include <QFile>

#include "config/app_paths.h"
#include "config/language_setting.h"

namespace LanguageSetting {

namespace {
const QString english = QStringLiteral("en_US");
const QString chinese = QStringLiteral("zh_CN");
}

QString load()
{
    QFile file(AppPaths::languageFile());
    if (!file.exists())
    {
        save(english);
        return english;
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
