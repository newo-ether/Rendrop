// process.h

#ifndef PROCESS_H
#define PROCESS_H

#include <QString>

#define NOMINMAX
#include <windows.h>

class Process
{
public:
    Process();
    ~Process();

    int start(const QString& program, const QStringList& args);
    void kill();
    void terminate();
    bool waitForFinished(int msec);
    bool isRunning() const;
    QString readStandardOutput();

private:
    void updateState();
    void cleanUp();

private:
    PROCESS_INFORMATION pi{};
    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    bool running = false;
};

#endif // PROCESS_H
