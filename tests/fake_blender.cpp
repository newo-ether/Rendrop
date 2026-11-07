#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>
#include <QThread>

namespace {
QString argumentValue(const QStringList &arguments, const QString &name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments[index + 1]
        : QString();
}

int writeProjectInfo(const QStringList &arguments)
{
    const QString scriptPath = argumentValue(arguments, QStringLiteral("--python"));
    const QString blendPath = argumentValue(arguments, QStringLiteral("--factory-startup"));
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return 2;
    }
    const QString script = QString::fromUtf8(scriptFile.readAll());
    const QString marker = QStringLiteral("with open(\"");
    const int pathStart = script.indexOf(marker);
    if (pathStart < 0)
    {
        return 3;
    }
    const int valueStart = pathStart + marker.size();
    const int valueEnd = script.indexOf(QStringLiteral("\", \"w\")"), valueStart);
    if (valueEnd < 0)
    {
        return 4;
    }
    QFile outputFile(script.mid(valueStart, valueEnd - valueStart));
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        return 5;
    }
    QTextStream output(&outputFile);
    output << "1920\n"
           << "1080\n"
           << "100\n"
           << "1\n"
           << "3\n"
           << "1\n"
           << "1\n"
           << QDir(QFileInfo(blendPath).absolutePath()).filePath(QStringLiteral("render_####.png"))
           << "\n";
    return 0;
}

int renderFrames(const QStringList &arguments)
{
    const QString blendPath = argumentValue(arguments, QStringLiteral("-b"));
    if (blendPath.contains(QStringLiteral("render_fail"), Qt::CaseInsensitive))
    {
        QThread::msleep(250);
        return 6;
    }
    const int frameStart = argumentValue(arguments, QStringLiteral("-s")).toInt();
    const int frameEnd = argumentValue(arguments, QStringLiteral("-e")).toInt();
    const int frameStep = argumentValue(arguments, QStringLiteral("-j")).toInt();
    if (frameStep <= 0 || frameEnd < frameStart)
    {
        return 7;
    }
    const bool slow = blendPath.contains(QStringLiteral("slow"), Qt::CaseInsensitive);
    // With -o, write real files the way Blender names them: the last run of
    // '#' becomes the zero-padded frame, otherwise four digits are appended;
    // a missing extension becomes .png.
    const QString outputPattern = argumentValue(arguments, QStringLiteral("-o"));
    QTextStream output(stdout, QIODevice::WriteOnly);
    for (int frame = frameStart; frame <= frameEnd; frame += frameStep)
    {
        QThread::msleep(slow ? 1000 : 80);
        output << "Fra: " << frame << " Mem: 1.00M\n";
        if (outputPattern.isEmpty())
        {
            output << "Saved: 'frame_" << frame << ".png'\n";
            output.flush();
            continue;
        }
        QString path = QDir::fromNativeSeparators(outputPattern);
        const qsizetype hashEnd = path.lastIndexOf(QLatin1Char('#'));
        if (hashEnd >= 0)
        {
            qsizetype hashStart = hashEnd;
            while (hashStart > 0 && path[hashStart - 1] == QLatin1Char('#'))
            {
                --hashStart;
            }
            const int width = static_cast<int>(hashEnd - hashStart + 1);
            path.replace(hashStart, width, QStringLiteral("%1").arg(frame, width, 10, QLatin1Char('0')));
        }
        else
        {
            path += QStringLiteral("%1").arg(frame, 4, 10, QLatin1Char('0'));
        }
        if (QFileInfo(path).suffix().isEmpty())
        {
            path += QStringLiteral(".png");
        }
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return 8;
        }
        file.write(QByteArray("fake frame ") + QByteArray::number(frame));
        file.close();
        output << "Saved: '" << QDir::toNativeSeparators(path) << "'\n";
        output.flush();
    }
    return 0;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    return arguments.contains(QStringLiteral("--python"))
        ? writeProjectInfo(arguments)
        : renderFrames(arguments);
}
