// dependency_packer.h

#ifndef DEPENDENCY_PACKER_H
#define DEPENDENCY_PACKER_H

#include <optional>
#include <vector>

#include <QHash>
#include <QObject>
#include <QString>

class QProcess;

// Collects a .blend and every external file it uses into one directory with
// Blender Asset Tracer, so remote workers can render projects whose assets are
// not packed. BAT runs inside Blender ("blender -b --factory-startup --python
// runner.py -- pack <blend> <target>"); assets outside the project directory
// end up under _outside_project with rewritten paths.
//
// One pack runs at a time. Results are keyed by the blend key and kept on
// disk until two newer packs exist.
class DependencyPacker : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        Missing,
        Packing,
        Ready,
        Failed
    };

    struct File
    {
        // Relative to the pack directory, '/' separated.
        QString path;
        qint64 size = 0;
    };

    struct Pack
    {
        QString key;
        QString directory;
        // Relative path of the .blend to render.
        QString mainFile;
        std::vector<File> files;
    };

    explicit DependencyPacker(const QString &cacheRoot, QObject *parent = nullptr);
    ~DependencyPacker() override;

    // blenderPath runs BAT; batPath is the directory holding the
    // blender_asset_tracer package.
    void setTool(const QString &blenderPath, const QString &batPath);

    // State of the pack for key; starts packing blendPath when nothing is
    // known about it and no other pack is running.
    State request(const QString &blendPath, const QString &key);
    std::optional<Pack> pack(const QString &key) const;
    QString errorText(const QString &key) const;
    void stop();

    // Loads a manifest written by a previous run. Exposed for tests.
    static std::optional<Pack> readManifest(const QString &directory, const QString &key);

signals:
    void finished(QString key, bool success, QString message);

private:
    void start(const QString &blendPath, const QString &key);
    void onProcessFinished(int exitCode);
    bool writeManifest(const QString &directory, const QString &mainFile);
    void prune();
    QString runnerPath() const;

    QString cacheRoot;
    QString blenderPath;
    QString batPath;
    QProcess *process = nullptr;
    QString runningKey;
    QString runningBlend;
    QHash<QString, Pack> ready;
    QHash<QString, QString> failed;
};

#endif // DEPENDENCY_PACKER_H
