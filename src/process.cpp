// process.cpp

#include <QString>
#include <QStringList>

#include "process.h"

Process::Process()
{
    ZeroMemory(&pi, sizeof(pi));
}

Process::~Process()
{
    kill();
}

int Process::start(const QString& program, const QStringList& args)
{
    if (running)
    {
        return -1;
    }

    cleanUp();

    SECURITY_ATTRIBUTES sa = {
        sizeof(SECURITY_ATTRIBUTES),
        nullptr,
        TRUE,
    };

    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
    {
        return -2;
    }

    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0))
    {
        if (hRead) {
            CloseHandle(hRead);
            hRead = nullptr;
        }

        if (hWrite) {
            CloseHandle(hWrite);
            hWrite = nullptr;
        }

        return -2;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;

    QString command = "\"" + program + "\"";
    for (QString arg : args)
    {
        command += " \"" + arg.replace("\"", "\\\"") + "\"";
    }
    std::wstring commandWStr = command.toStdWString();

    if (!CreateProcessW(
            nullptr,               // Application name
            const_cast<wchar_t*>(commandWStr.c_str()), // Command line (Unicode)
            nullptr,               // Process security attributes
            nullptr,               // Thread security attributes
            TRUE,                  // Inherit handles
            CREATE_NO_WINDOW,      // Creation flags
            nullptr,               // Environment
            nullptr,               // Current directory
            &si,                   // Startup info
            &pi                    // Process information
    )) {
        if (hRead) {
            CloseHandle(hRead);
            hRead = nullptr;
        }

        if (hWrite) {
            CloseHandle(hWrite);
            hWrite = nullptr;
        }

        return -2;
    }

    running = true;
    return 0;
}

void Process::kill()
{
    if (pi.hProcess)
    {
        TerminateProcess(pi.hProcess, 1);
    }
    cleanUp();
    running = false;
}

void Process::terminate()
{
    if (pi.hProcess)
    {
        PostThreadMessageW(pi.dwThreadId, WM_QUIT, 0, 0);
    }
    cleanUp();
    running = false;
}

void Process::updateState()
{
    if (!pi.hProcess)
    {
        running = false;
        return;
    }

    DWORD code = 0;
    if (GetExitCodeProcess(pi.hProcess, &code))
    {
        running = (code == STILL_ACTIVE);
        return;
    }
    running = false;
    cleanUp();
}

void Process::cleanUp()
{
    if (pi.hProcess)
    {
        CloseHandle(pi.hProcess);
        pi.hProcess = nullptr;
    }

    if (pi.hThread)
    {
        CloseHandle(pi.hThread);
        pi.hThread = nullptr;
    }

    ZeroMemory(&pi, sizeof(pi));

    if (hRead)
    {
        CloseHandle(hRead);
        hRead = nullptr;
    }

    if (hWrite)
    {
        CloseHandle(hWrite);
        hWrite = nullptr;
    }
}

bool Process::waitForFinished(int msec)
{
    if (!running || !pi.hProcess)
    {
        return true;
    }

    DWORD result = WaitForSingleObject(pi.hProcess, msec);

    if (result == WAIT_TIMEOUT)
    {
        return false;
    }

    updateState();
    return !running;
}

bool Process::isRunning() const
{
    return running;
}

QString Process::readStandardOutput()
{
    if (!hRead)
    {
        return QString();
    }

    DWORD bytesAvailable = 0;

    if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        return QString();
    }

    if (bytesAvailable == 0) {
        return QString();
    }

    DWORD bytesRead = 0;
    char buffer[4096] = {0};
    QString output;

    BOOL result = ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);

    if (!result || bytesRead == 0)
    {
        return QString();
    }

    buffer[bytesRead] = '\0';
    output += QString::fromUtf8(buffer);

    return output;
}
