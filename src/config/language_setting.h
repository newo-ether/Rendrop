// language_setting.h

#ifndef LANGUAGE_SETTING_H
#define LANGUAGE_SETTING_H

#include <QString>

// The UI language in language.cfg: one locale name, applied at startup.
namespace LanguageSetting {

// Index order of the settings page's language selector.
enum Index
{
    English = 0,
    Chinese = 1
};

// The stored locale, e.g. "zh_CN". A missing file is created as en_US.
QString load();
bool save(const QString &locale);

QString localeForIndex(int index);
int indexForLocale(const QString &locale);

}

#endif // LANGUAGE_SETTING_H
