// settings_page.h

#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <vector>

#include <QString>
#include <QWidget>

#include "blender_version_manager.h"
#include "distributed_settings.h"

class QButtonGroup;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTimer;
class QVBoxLayout;
class ToggleSwitch;

// The "Settings" page: a section list on the left, the form on the right.
//
// Every change is saved automatically. Language, Blender versions, launch at
// login and shadows apply at once; only the language needs a restart. The
// distributed options are written to distributed.json shortly after an edit
// and handed to the window through farmSettingsSaved, which applies them
// live. acceptRemoteWork is not part of the form: the Nodes page switches it.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    enum class Section
    {
        General,
        Blender,
        Farm,
        Scheduling,
        Load,
        Dependencies,
        Cache
    };

    SettingsPage(const QString &distributedPath, int languageIndex, QWidget *parent = nullptr);
    ~SettingsPage() override;

    void setBlenderVersions(const std::vector<BlenderVersionManager::BlenderVersion> &versions);
    // Off while rendering: the queue holds the selected Blender path.
    void setBlenderEditingEnabled(bool enabled);
    // On while "blender --version" probes a newly picked executable.
    void setAddBlenderBusy(bool busy);
    void showSection(Section section);

signals:
    void languageChanged(int index);
    void addBlenderRequested();
    void removeBlenderRequested(QString version);
    // Written to distributed.json; the farm should switch to these now.
    void farmSettingsSaved(DistributedSettings settings);

private:
    QWidget *addSection(Section section, const QString &title);
    void addRow(QWidget *section, const QString &label, const QString &help, QWidget *field);
    void loadDraft();
    void populateForm(const DistributedSettings &settings);
    void restoreDefaults();
    void readForm(DistributedSettings &settings) const;
    void markDirty();
    void flushPendingSave();
    void save();
    void updateSectionHighlight();
    void updateFarmFields();

    QString distributedPath;
    DistributedSettings loaded;
    bool populating = false;
    QTimer *saveTimer;
    std::vector<QWidget *> sections;
    QButtonGroup *sectionButtons;
    QScrollArea *scroll;
    QVBoxLayout *formLayout;

    QComboBox *languageBox;
    ToggleSwitch *launchAtLoginSwitch;
    ToggleSwitch *shadowsSwitch;
    QVBoxLayout *blenderList;
    QPushButton *addBlenderButton;
    std::vector<QPushButton *> removeBlenderButtons;
    bool blenderEditingEnabled = true;
    bool addBlenderBusy = false;

    ToggleSwitch *enabledSwitch;
    QLineEdit *tokenEdit;
    QPushButton *showTokenButton;
    QPushButton *copyTokenButton;
    QPushButton *regenerateTokenButton;
    QSpinBox *discoveryPortBox;
    QPlainTextEdit *peersEdit;

    ToggleSwitch *adaptiveSwitch;
    QSpinBox *chunkSizeBox;
    QSpinBox *targetLeaseBox;
    QSpinBox *maxChunkBox;
    QSpinBox *leaseTimeoutBox;

    QDoubleSpinBox *busyThresholdBox;
    QSpinBox *busyWindowBox;
    QDoubleSpinBox *idleThresholdBox;
    QSpinBox *idleWindowBox;
    QSpinBox *sampleIntervalBox;

    QComboBox *dependencyModeBox;
    QLineEdit *batPathEdit;
    QPushButton *batBrowseButton;
    QLineEdit *cacheEdit;

    QWidget *saveBar;
    QLabel *saveBarLabel;
    QPushButton *restoreDefaultsButton;
};

#endif // SETTINGS_PAGE_H
