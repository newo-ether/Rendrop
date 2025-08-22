// blender_file_reader.h

#ifndef BLENDER_FILE_READER_H
#define BLENDER_FILE_READER_H

#include <QString>
#include <QThread>

#include "blender_file_info.h"

class BlenderFileReader : public QThread
{
    Q_OBJECT

public:
    BlenderFileReader(QObject *parent = nullptr);
    void setParameter(QString filePath, QString blenderPath);
    void stop();

protected:
    void run() override;

signals:
    void finishedReading(int status, BlenderFileInfo info);

private:
    QString filePath, blenderPath;
    bool isParameterSet;
    bool stopped;
};

#endif // BLENDER_FILE_READER_H
