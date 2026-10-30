#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtTest>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

class CliTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void argumentOutput_data();
    void argumentOutput();
#ifdef Q_OS_WIN
    void noStandardHandles_data();
    void noStandardHandles();
#endif

private:
    QString executable;
};

void CliTest::initTestCase()
{
    executable = qEnvironmentVariable("RENDROPCTL_EXECUTABLE");
    QVERIFY2(!executable.isEmpty(), "RENDROPCTL_EXECUTABLE is not set");
    QVERIFY2(QFileInfo(executable).isFile(), qPrintable(executable));
}

void CliTest::argumentOutput_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<int>("exitCode");
    QTest::addColumn<QByteArray>("stdoutText");
    QTest::addColumn<QByteArray>("stderrText");

    QTest::newRow("help") << QStringList{"--help"}
        << 0 << QByteArray("Usage:") << QByteArray();
    QTest::newRow("help-short") << QStringList{"-h"}
        << 0 << QByteArray("Usage:") << QByteArray();
    QTest::newRow("help-all") << QStringList{"--help-all"}
        << 0 << QByteArray("Usage:") << QByteArray();
#ifdef Q_OS_WIN
    QTest::newRow("help-windows") << QStringList{"-?"}
        << 0 << QByteArray("Usage:") << QByteArray();
#endif
    QTest::newRow("version") << QStringList{"--version"}
        << 0 << QByteArray("rendropctl ") << QByteArray();
    QTest::newRow("version-short") << QStringList{"-v"}
        << 0 << QByteArray("rendropctl ") << QByteArray();
    QTest::newRow("unknown-option") << QStringList{"--invalid-cli-option"}
        << 1 << QByteArray() << QByteArray("Unknown option");
    QTest::newRow("missing-option-value") << QStringList{"--port"}
        << 1 << QByteArray() << QByteArray("Missing value after");
    QTest::newRow("missing-command") << QStringList{}
        << 1 << QByteArray() << QByteArray("Missing command");
    QTest::newRow("unknown-command") << QStringList{"invalid-command"}
        << 1 << QByteArray() << QByteArray("Unknown command");
    QTest::newRow("invalid-port") << QStringList{"--port", "0", "status"}
        << 1 << QByteArray() << QByteArray("Invalid port");
}

void CliTest::argumentOutput()
{
    QFETCH(QStringList, arguments);
    QFETCH(int, exitCode);
    QFETCH(QByteArray, stdoutText);
    QFETCH(QByteArray, stderrText);

    QProcess child;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    // Verify the executable itself prevents dialogs, without Qt's opt-out flag.
    environment.remove(QStringLiteral("QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES"));
    child.setProcessEnvironment(environment);
#ifdef Q_OS_WIN
    child.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *process) {
        process->flags |= CREATE_NO_WINDOW;
    });
#endif
    child.start(executable, arguments);
    QVERIFY2(child.waitForStarted(5000), qPrintable(child.errorString()));
    if (!child.waitForFinished(3000))
    {
        child.kill();
        child.waitForFinished(1000);
        QFAIL("CLI did not exit without a console; it may be waiting for a dialog");
    }

    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), exitCode);
    const QByteArray standardOutput = child.readAllStandardOutput();
    const QByteArray standardError = child.readAllStandardError();
    if (stdoutText.isEmpty())
    {
        QVERIFY2(standardOutput.isEmpty(), standardOutput.constData());
    }
    else
    {
        QVERIFY2(standardOutput.contains(stdoutText), standardOutput.constData());
    }
    if (stderrText.isEmpty())
    {
        QVERIFY2(standardError.isEmpty(), standardError.constData());
    }
    else
    {
        QVERIFY2(standardError.contains(stderrText), standardError.constData());
    }
}

#ifdef Q_OS_WIN
void CliTest::noStandardHandles_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<int>("exitCode");
    QTest::newRow("help") << QStringList{"--help"} << 0;
    QTest::newRow("version") << QStringList{"--version"} << 0;
    QTest::newRow("argument-error") << QStringList{"--invalid-cli-option"} << 1;
}

void CliTest::noStandardHandles()
{
    QFETCH(QStringList, arguments);
    QFETCH(int, exitCode);

    QProcess child;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES"));
    child.setProcessEnvironment(environment);
    child.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *process) {
        process->flags |= CREATE_NO_WINDOW;
        process->startupInfo->dwFlags |= STARTF_USESTDHANDLES;
        process->startupInfo->hStdInput = INVALID_HANDLE_VALUE;
        process->startupInfo->hStdOutput = INVALID_HANDLE_VALUE;
        process->startupInfo->hStdError = INVALID_HANDLE_VALUE;
        process->inheritHandles = false;
    });
    child.start(executable, arguments);
    QVERIFY2(child.waitForStarted(5000), qPrintable(child.errorString()));
    if (!child.waitForFinished(3000))
    {
        child.kill();
        child.waitForFinished(1000);
        QFAIL("CLI must exit without dialogs even when standard handles are unavailable");
    }
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), exitCode);
}
#endif

QTEST_GUILESS_MAIN(CliTest)

#include "cli_test.moc"
