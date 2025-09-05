// main.cpp

#include <QApplication>
#include <QFontDatabase>
#include <QTranslator>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QDir>
#include <QSharedMemory>

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

    QSharedMemory sharedMemory("RendropAppKey");
    if (!sharedMemory.create(1)) {
        QMessageBox messageBox;
        QFont font("Pixel Mixed", 12);
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setText(QCoreApplication::translate("main", "The application is already running."));
        messageBox.setWindowTitle(QCoreApplication::translate("main", "Warning"));
        messageBox.setFont(font);
        return messageBox.exec();
    }

    Widget widget(languageIndex);
    widget.show();
    return app.exec();
}
