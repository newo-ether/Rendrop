#include <functional>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "render_queue.h"

namespace {
QString createBlendFile(const QTemporaryDir &directory, const QString &name)
{
    const QString path = QDir(directory.path()).filePath(name + QStringLiteral(".blend"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return QString();
    }
    file.write("fake blend file");
    return QDir::toNativeSeparators(path);
}

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 8000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs)
    {
        if (condition())
        {
            return true;
        }
        QTest::qWait(10);
    }
    return condition();
}

QString persistenceDirectory(const QTemporaryDir &directory)
{
    return directory.path();
}

QJsonArray persistedProjects(const QString &directory)
{
    QFile file(QDir(directory).filePath(QStringLiteral("projects.json")));
    if (!file.open(QIODevice::ReadOnly))
    {
        return QJsonArray();
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject()
        ? document.object().value(QStringLiteral("projects")).toArray()
        : QJsonArray();
}

QJsonObject persistedRoot(const QString &directory)
{
    QFile file(QDir(directory).filePath(QStringLiteral("projects.json")));
    if (!file.open(QIODevice::ReadOnly))
    {
        return QJsonObject();
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}
QString uuidString(const QUuid &id)
{
    return id.toString(QUuid::WithoutBraces);
}
}

class RenderQueueTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void projectInfoSerializationNormalizesState();
    void loadErrorReloadAndEdit();
    void reorderAndRemove();
    void renderFailureAdvancesSerialQueue();
    void stopThenRestartIgnoresStoppedRunCompletion();
    void removingCurrentJobAdvancesQueue();
    void persistedProgressRestoresAndResumes();
    void legacyIntegerIdsMigrateToUuidSchema();

private:
    QString fakeBlenderPath;
};

void RenderQueueTest::initTestCase()
{
    qRegisterMetaType<ProjectInfo>("ProjectInfo");
    fakeBlenderPath = qEnvironmentVariable("RENDROP_FAKE_BLENDER");
    QVERIFY2(!fakeBlenderPath.isEmpty(), "RENDROP_FAKE_BLENDER is not set");
    QVERIFY2(QFileInfo::exists(fakeBlenderPath), qPrintable(fakeBlenderPath));
}

void RenderQueueTest::projectInfoSerializationNormalizesState()
{
    const QUuid projectId = QUuid::createUuid();
    ProjectInfo loading(
        projectId,
        QStringLiteral("project"),
        QStringLiteral("C:/project.blend"),
        QStringLiteral("//render_####.png"),
        ProjectState::Loading,
        1,
        3,
        1,
        1920,
        1080,
        100,
        QStringLiteral("EEVEE"),
        1,
        99
    );
    std::optional<ProjectInfo> restored = ProjectInfo::fromJson(loading.toJson());
    QVERIFY(restored.has_value());
    QCOMPARE(restored->id, projectId);
    QCOMPARE(restored->state, ProjectState::Queued);
    QCOMPARE(restored->finishedFrame, 1);
    QCOMPARE(restored->totalFrame, 3);

    loading.state = ProjectState::Rendering;
    loading.finishedFrame = 3;
    restored = ProjectInfo::fromJson(loading.toJson());
    QVERIFY(restored.has_value());
    QCOMPARE(restored->state, ProjectState::Finished);

    loading.state = ProjectState::Finished;
    loading.finishedFrame = 2;
    restored = ProjectInfo::fromJson(loading.toJson());
    QVERIFY(restored.has_value());
    QCOMPARE(restored->state, ProjectState::Queued);

    QJsonObject invalid = loading.toJson();
    invalid.insert(QStringLiteral("schemaVersion"), 99);
    QVERIFY(!ProjectInfo::fromJson(invalid).has_value());
}

void RenderQueueTest::loadErrorReloadAndEdit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blendPath = createBlendFile(directory, QStringLiteral("recover"));
    QVERIFY(!blendPath.isEmpty());

    const QString persistDirectory = persistenceDirectory(directory);
    RenderQueue queue(persistDirectory);
    const QUuid id = queue.addJob(
        QStringLiteral("recover"),
        blendPath,
        QDir(directory.path()).filePath(QStringLiteral("missing_blender.exe"))
    );
    QVERIFY(!id.isNull());
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(id).state == ProjectState::Error;
    }));
    QJsonArray projects = persistedProjects(persistDirectory);
    QCOMPARE(projects.size(), 1);
    QCOMPARE(projects[0].toObject().value(QStringLiteral("state")).toString(), QStringLiteral("Error"));

    QVERIFY(queue.reloadJob(id, fakeBlenderPath));
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(id).state == ProjectState::Queued;
    }));
    ProjectInfo snapshot = queue.snapshotById(id);
    QCOMPARE(snapshot.frameStart, 1);
    QCOMPARE(snapshot.frameEnd, 3);
    QCOMPARE(snapshot.frameStep, 1);
    QCOMPARE(snapshot.totalFrame, 3);
    QCOMPARE(snapshot.renderEngine, QStringLiteral("EEVEE"));
    projects = persistedProjects(persistDirectory);
    QCOMPARE(projects.size(), 1);
    QCOMPARE(projects[0].toObject().value(QStringLiteral("state")).toString(), QStringLiteral("Queued"));
    QCOMPARE(projects[0].toObject().value(QStringLiteral("totalFrame")).toInt(), 3);

    QVERIFY(queue.renameJob(id, QStringLiteral("renamed")));
    QVERIFY(queue.setFrameRange(id, 2, 8, 2));
    snapshot = queue.snapshotById(id);
    QCOMPARE(snapshot.name, QStringLiteral("renamed"));
    QCOMPARE(snapshot.totalFrame, 4);
    QVERIFY(queue.framePath(id, 4).endsWith(QStringLiteral("render_0004.png")));
    projects = persistedProjects(persistDirectory);
    QCOMPARE(projects[0].toObject().value(QStringLiteral("name")).toString(), QStringLiteral("renamed"));
    QCOMPARE(projects[0].toObject().value(QStringLiteral("frameStart")).toInt(), 2);
    QCOMPARE(projects[0].toObject().value(QStringLiteral("frameEnd")).toInt(), 8);
    QCOMPARE(projects[0].toObject().value(QStringLiteral("frameStep")).toInt(), 2);
}

void RenderQueueTest::reorderAndRemove()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString missingBlender = QDir(directory.path()).filePath(QStringLiteral("missing.exe"));
    const QString persistDirectory = persistenceDirectory(directory);
    QUuid first;
    QUuid second;
    QUuid third;
    {
    RenderQueue queue(persistDirectory);
    first = queue.addJob(
        QStringLiteral("first"),
        createBlendFile(directory, QStringLiteral("first")),
        missingBlender
    );
    second = queue.addJob(
        QStringLiteral("second"),
        createBlendFile(directory, QStringLiteral("second")),
        missingBlender
    );
    third = queue.addJob(
        QStringLiteral("third"),
        createBlendFile(directory, QStringLiteral("third")),
        missingBlender
    );
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(first).state == ProjectState::Error
            && queue.snapshotById(second).state == ProjectState::Error
            && queue.snapshotById(third).state == ProjectState::Error;
    }));

    QVERIFY(queue.moveUp(second));
    std::vector<ProjectInfo> snapshots = queue.snapshots();
    QCOMPARE(snapshots[0].id, second);
    QCOMPARE(snapshots[1].id, first);
    QCOMPARE(snapshots[2].id, third);
    QJsonArray projects = persistedProjects(persistDirectory);
    QCOMPARE(projects.size(), 3);
    QCOMPARE(projects[0].toObject().value(QStringLiteral("id")).toString(), uuidString(second));
    QCOMPARE(projects[1].toObject().value(QStringLiteral("id")).toString(), uuidString(first));
    QCOMPARE(projects[2].toObject().value(QStringLiteral("id")).toString(), uuidString(third));

    QVERIFY(queue.moveDown(second));
    QVERIFY(!queue.moveDown(third));
    QVERIFY(queue.removeJob(second));
    snapshots = queue.snapshots();
    QCOMPARE(snapshots.size(), size_t(2));
    QCOMPARE(snapshots[0].id, first);
    QCOMPARE(snapshots[1].id, third);
    QVERIFY(queue.snapshotById(second).isNull);
    QVERIFY(!queue.removeJob(second));
    projects = persistedProjects(persistDirectory);
    QCOMPARE(projects.size(), 2);
    QCOMPARE(projects[0].toObject().value(QStringLiteral("id")).toString(), uuidString(first));
    QCOMPARE(projects[1].toObject().value(QStringLiteral("id")).toString(), uuidString(third));
    }

    RenderQueue restored(persistDirectory);
    QCOMPARE(restored.restorePersistedJobs(missingBlender), 2);
    const std::vector<ProjectInfo> restoredSnapshots = restored.snapshots();
    QCOMPARE(restoredSnapshots[0].id, first);
    QCOMPARE(restoredSnapshots[1].id, third);
}

void RenderQueueTest::renderFailureAdvancesSerialQueue()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    RenderQueue queue(persistenceDirectory(directory));
    const QUuid failing = queue.addJob(
        QStringLiteral("failing"),
        createBlendFile(directory, QStringLiteral("render_fail")),
        fakeBlenderPath
    );
    const QUuid succeeding = queue.addJob(
        QStringLiteral("succeeding"),
        createBlendFile(directory, QStringLiteral("normal")),
        fakeBlenderPath
    );
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(failing).state == ProjectState::Queued
            && queue.snapshotById(succeeding).state == ProjectState::Queued;
    }));

    QSignalSpy renderingSpy(&queue, &RenderQueue::renderingChanged);
    QVERIFY(queue.start());
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(failing).state == ProjectState::Rendering;
    }));
    QCOMPARE(queue.snapshotById(succeeding).state, ProjectState::Queued);
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(failing).state == ProjectState::Error
            && queue.snapshotById(succeeding).state == ProjectState::Finished
            && !queue.isRendering();
    }));
    QVERIFY(renderingSpy.count() >= 2);
    QCOMPARE(renderingSpy.first().at(0).toBool(), true);
    QCOMPARE(renderingSpy.last().at(0).toBool(), false);
}

void RenderQueueTest::stopThenRestartIgnoresStoppedRunCompletion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    RenderQueue queue(persistenceDirectory(directory));
    const QUuid id = queue.addJob(
        QStringLiteral("slow"),
        createBlendFile(directory, QStringLiteral("slow")),
        fakeBlenderPath
    );
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(id).state == ProjectState::Queued;
    }));
    QVERIFY(queue.start());
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(id).state == ProjectState::Rendering;
    }));

    queue.stop();
    QCOMPARE(queue.snapshotById(id).state, ProjectState::Queued);
    QVERIFY(!queue.isRendering());
    QVERIFY(queue.start());
    QCOMPARE(queue.snapshotById(id).state, ProjectState::Rendering);
    QTest::qWait(250);
    QVERIFY(queue.isRendering());
    QCOMPARE(queue.snapshotById(id).state, ProjectState::Rendering);

    queue.stop();
    QVERIFY(!queue.isRendering());
    QCOMPARE(queue.snapshotById(id).state, ProjectState::Queued);
}

void RenderQueueTest::removingCurrentJobAdvancesQueue()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    RenderQueue queue(persistenceDirectory(directory));
    const QUuid first = queue.addJob(
        QStringLiteral("slow"),
        createBlendFile(directory, QStringLiteral("slow_remove")),
        fakeBlenderPath
    );
    const QUuid second = queue.addJob(
        QStringLiteral("next"),
        createBlendFile(directory, QStringLiteral("next")),
        fakeBlenderPath
    );
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(first).state == ProjectState::Queued
            && queue.snapshotById(second).state == ProjectState::Queued;
    }));
    QVERIFY(queue.start());
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(first).state == ProjectState::Rendering;
    }));

    QVERIFY(queue.removeJob(first));
    QVERIFY(queue.snapshotById(first).isNull);
    QVERIFY(waitUntil([&]() {
        return queue.snapshotById(second).state == ProjectState::Finished
            && !queue.isRendering();
    }));
}

void RenderQueueTest::persistedProgressRestoresAndResumes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString persistDirectory = persistenceDirectory(directory);
    const QString blendPath = createBlendFile(directory, QStringLiteral("slow_persist"));
    QVERIFY(!blendPath.isEmpty());
    QUuid id;
    {
        RenderQueue queue(persistDirectory);
        id = queue.addJob(QStringLiteral("slow"), blendPath, fakeBlenderPath);
        QVERIFY(waitUntil([&]() {
            return queue.snapshotById(id).state == ProjectState::Queued;
        }));
        QVERIFY(queue.start());
        QVERIFY(waitUntil([&]() {
            const ProjectInfo snapshot = queue.snapshotById(id);
            return snapshot.state == ProjectState::Rendering && snapshot.finishedFrame >= 1;
        }, 10000));
        const QJsonArray projects = persistedProjects(persistDirectory);
        QCOMPARE(projects.size(), 1);
        QCOMPARE(projects[0].toObject().value(QStringLiteral("state")).toString(), QStringLiteral("Rendering"));
        QCOMPARE(projects[0].toObject().value(QStringLiteral("finishedFrame")).toInt(), 1);
    }

    RenderQueue restored(persistDirectory);
    QCOMPARE(restored.restorePersistedJobs(fakeBlenderPath), 1);
    ProjectInfo snapshot = restored.snapshotById(id);
    QCOMPARE(snapshot.state, ProjectState::Queued);
    QCOMPARE(snapshot.finishedFrame, 1);
    QCOMPARE(snapshot.totalFrame, 3);
    QVERIFY(restored.start());
    QVERIFY(waitUntil([&]() {
        return restored.snapshotById(id).state == ProjectState::Finished
            && !restored.isRendering();
    }, 10000));
    snapshot = restored.snapshotById(id);
    QCOMPARE(snapshot.finishedFrame, snapshot.totalFrame);
    const QJsonArray projects = persistedProjects(persistDirectory);
    QCOMPARE(projects[0].toObject().value(QStringLiteral("state")).toString(), QStringLiteral("Finished"));
    QCOMPARE(projects[0].toObject().value(QStringLiteral("finishedFrame")).toInt(), 3);
}

void RenderQueueTest::legacyIntegerIdsMigrateToUuidSchema()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blendPath = createBlendFile(directory, QStringLiteral("legacy"));
    QVERIFY(!blendPath.isEmpty());
    const QJsonObject legacyProject = {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("id"), 42},
        {QStringLiteral("name"), QStringLiteral("legacy")},
        {QStringLiteral("path"), blendPath},
        {QStringLiteral("outputPath"), QStringLiteral("//render_####.png")},
        {QStringLiteral("state"), QStringLiteral("Rendering")},
        {QStringLiteral("frameStart"), 1},
        {QStringLiteral("frameEnd"), 3},
        {QStringLiteral("frameStep"), 1},
        {QStringLiteral("resolutionX"), 1920},
        {QStringLiteral("resolutionY"), 1080},
        {QStringLiteral("resolutionScale"), 100},
        {QStringLiteral("renderEngine"), QStringLiteral("EEVEE")},
        {QStringLiteral("finishedFrame"), 1},
        {QStringLiteral("totalFrame"), 3}
    };
    const QJsonObject legacyRoot = {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("projects"), QJsonArray{legacyProject}}
    };
    const QByteArray legacyData = QJsonDocument(legacyRoot).toJson();
    QFile legacyFile(QDir(directory.path()).filePath(QStringLiteral("projects.json")));
    QVERIFY(legacyFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(legacyFile.write(legacyData), qint64(legacyData.size()));
    legacyFile.close();

    RenderQueue queue(persistenceDirectory(directory));
    QCOMPARE(queue.restorePersistedJobs(fakeBlenderPath), 1);
    const std::vector<ProjectInfo> snapshots = queue.snapshots();
    QCOMPARE(snapshots.size(), size_t(1));
    QVERIFY(!snapshots[0].id.isNull());
    QCOMPARE(snapshots[0].state, ProjectState::Queued);
    QCOMPARE(snapshots[0].finishedFrame, 1);

    const QJsonObject migratedRoot = persistedRoot(directory.path());
    QCOMPARE(migratedRoot.value(QStringLiteral("schemaVersion")).toInt(), 2);
    const QJsonArray migratedProjects = migratedRoot.value(QStringLiteral("projects")).toArray();
    QCOMPARE(migratedProjects.size(), 1);
    const QJsonObject migratedProject = migratedProjects[0].toObject();
    QCOMPARE(migratedProject.value(QStringLiteral("schemaVersion")).toInt(), 2);
    QVERIFY(migratedProject.value(QStringLiteral("id")).isString());
    QCOMPARE(migratedProject.value(QStringLiteral("id")).toString(), uuidString(snapshots[0].id));
}
QTEST_GUILESS_MAIN(RenderQueueTest)

#include "render_queue_test.moc"
