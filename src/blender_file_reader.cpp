// blender_file_reader.cpp

#include <QThread>
#include <QString>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QDir>
#include <QTextStream>

#include "blender_file_reader.h"
#include "blender_file_info.h"
#include "simple_process.h"

BlenderFileReader::BlenderFileReader(QObject *parent):
    QThread(parent), stopped(false) {}

void BlenderFileReader::setParameter(QString filePath, QString blenderPath)
{
    this->filePath = filePath;
    this->blenderPath = blenderPath;
    isParameterSet = true;
}

void BlenderFileReader::stop()
{
    stopped = true;
}

void BlenderFileReader::run()
{
    stopped = false;

    if (!isParameterSet)
    {
        emit finishedReading(-1, BlenderFileInfo());
        return;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists())
    {
        emit finishedReading(-1, BlenderFileInfo());
        return;
    }

    QTemporaryFile readerFile(QDir::temp().filePath("blender_reader_XXXXXX.py"));
    readerFile.setAutoRemove(false);
    if (!readerFile.open())
    {
        emit finishedReading(-1, BlenderFileInfo());
        return;
    }

    QTemporaryFile outputFile(QDir::temp().filePath("blender_output_XXXXXX.txt"));
    outputFile.setAutoRemove(false);
    if (!outputFile.open())
    {
        emit finishedReading(-1, BlenderFileInfo());
        return;
    }
    outputFile.close();

    QTextStream out(&readerFile);
    QString script =
        "import bpy, json\n"
        "\n"
        "scene = bpy.context.scene\n"
        "render = scene.render\n"
        "engine_map = {\n"
        "    \"CYCLES\": 0,\n"
        "    \"BLENDER_EEVEE\": 1,\n"
        "    \"BLENDER_EEVEE_NEXT\": 1,\n"
        "    \"BLENDER_WORKBENCH\": 2,\n"
        "}\n"
        "\n"
        "info = {\n"
        "    \"resolutionX\": render.resolution_x,\n"
        "    \"resolutionY\": render.resolution_y,\n"
        "    \"resolutionScale\": render.resolution_percentage,\n"
        "    \"frameStart\": scene.frame_start,\n"
        "    \"frameEnd\": scene.frame_end,\n"
        "    \"frameStep\": scene.frame_step,\n"
        "    \"renderEngine\": engine_map.get(render.engine, -1),\n"
        "    \"outputPath\": scene.render.filepath\n"
        "}\n"
        "\n"
        "with open(\"%1\", \"w\") as f:\n"
        "    f.write(str(info['resolutionX']) + \"\\n\")\n"
        "    f.write(str(info['resolutionY']) + \"\\n\")\n"
        "    f.write(str(info['resolutionScale']) + \"\\n\")\n"
        "    f.write(str(info['frameStart']) + \"\\n\")\n"
        "    f.write(str(info['frameEnd']) + \"\\n\")\n"
        "    f.write(str(info['frameStep']) + \"\\n\")\n"
        "    f.write(str(info['renderEngine']) + \"\\n\")\n"
        "    f.write(str(info['outputPath']) + \"\\n\")\n"
        "bpy.ops.wm.quit_blender()\n";

    script = script.arg(outputFile.fileName());
    out << script;
    out.flush();
    readerFile.close();

    SimpleProcess process;

    QStringList args;
    args << "-b"
         << "--factory-startup"
         << filePath
         << "--python"
         << readerFile.fileName();

    if (process.start(blenderPath, args) != 0)
    {
        emit finishedReading(-1, BlenderFileInfo());
        return;
    }

    QFileInfo outputFileInfo(outputFile.fileName());
    while (process.isRunning())
    {
        process.waitForFinished(50);
        if (stopped)
        {
            process.kill();
            readerFile.remove();
            outputFile.remove();
            return;
        }
    }

    if (!outputFileInfo.exists() || outputFileInfo.size() == 0)
    {
        readerFile.remove();
        outputFile.remove();
        emit finishedReading(-1, BlenderFileInfo());
        return;
    }

    if (!outputFile.open())
    {
        readerFile.remove();
        outputFile.remove();
        emit finishedReading(-1, BlenderFileInfo());
        return;
    }

    QTextStream in(&outputFile);
    QStringList lines;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        if (!line.isEmpty())
        {
            lines.append(line);
        }
    }
    outputFile.close();

    if (lines.size() < 7)
    {
        readerFile.remove();
        outputFile.remove();
        emit finishedReading(-1, BlenderFileInfo());
        return;
    }

    int resolutionX = lines[0].toInt();
    int resolutionY = lines[1].toInt();
    int resolutionScale = lines[2].toInt();
    int frameStart = lines[3].toInt();
    int frameEnd = lines[4].toInt();
    int frameStep = lines[5].toInt();
    int renderEngine = lines[6].toInt();
    QString outputPath = lines[7].trimmed();

    QString renderEngineText;
    if (renderEngine == 0)
    {
        renderEngineText = "Cycles";
    }
    else if (renderEngine == 1)
    {
        renderEngineText = "EEVEE";
    }
    else if (renderEngine == 2)
    {
        renderEngineText = "Workbench";
    }
    else
    {
        renderEngineText = "Unknown";
    }

    BlenderFileInfo info(
        resolutionX,
        resolutionY,
        resolutionScale,
        frameStart,
        frameEnd,
        frameStep,
        renderEngineText,
        outputPath
    );

    readerFile.remove();
    outputFile.remove();

    emit finishedReading(0, info);
}
