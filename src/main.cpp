// main.cpp

#include <QApplication>
#include <QFontDatabase>
#include <QTranslator>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QDir>

#include "widget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QFontDatabase::addApplicationFont(":/font/PixelMixed.ttf");
    QFontDatabase::addApplicationFont(":/font/BitcountSingle-Bold.ttf");

    QTranslator translator;
    int languageIndex = 0;

    QDir configDir = QFileInfo("config/language.cfg").absoluteDir();
    if (!configDir.exists())
    {
        configDir.mkpath(configDir.absolutePath());
    }

    QFile configFile("config/language.cfg");
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
            }
            configFile.close();
        }
    }

    Widget widget(languageIndex);
    widget.show();
    return app.exec();
}
