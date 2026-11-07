// settings_page.cpp

#include <algorithm>

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "app_preferences.h"
#include "drop_shadow_widget.h"
#include "rounded_surface.h"
#include "settings_page.h"
#include "look_widgets.h"

namespace {

constexpr int fieldWidth = 280;

QString sectionButtonStyleSheet()
{
    return QStringLiteral(
        // RoundedSurface paints the hover and checked backgrounds.
        "QPushButton { background: transparent; border: none; color: rgb(150, 155, 170);"
        "  text-align: left; padding: 7px 12px; }"
        "QPushButton:checked { color: white; }"
    );
}

QWidget *framedSurface(QWidget *parent, QWidget **surfaceOut)
{
    auto *frame = new QWidget(parent);
    frame->setStyleSheet(QStringLiteral("background: transparent;"));
    // Widget gives every frame marked as a card a drop shadow.
    frame->setProperty(Look::cardProperty, true);
    RoundedSurface::install(frame, 12, Look::frameColor);
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(5, 5, 5, 5);
    auto *surface = new QWidget(frame);
    surface->setStyleSheet(QStringLiteral("background: transparent; color: #E0E0E0;"));
    RoundedSurface::install(surface, 10, Look::surfaceColor);
    frameLayout->addWidget(surface);
    *surfaceOut = surface;
    return frame;
}

QSpinBox *spinBox(QWidget *parent, int minimum, int maximum, const QString &suffix)
{
    auto *box = new QSpinBox(parent);
    box->setRange(minimum, maximum);
    box->setSuffix(suffix);
    box->setButtonSymbols(QAbstractSpinBox::NoButtons);
    box->setFixedSize(fieldWidth, 30);
    return box;
}

QDoubleSpinBox *percentBox(QWidget *parent)
{
    auto *box = new QDoubleSpinBox(parent);
    box->setRange(0.0, 100.0);
    box->setDecimals(1);
    box->setSuffix(QStringLiteral(" %"));
    box->setButtonSymbols(QAbstractSpinBox::NoButtons);
    box->setFixedSize(fieldWidth, 30);
    return box;
}

QPushButton *button(QWidget *parent, const QString &text, bool accent)
{
    auto *result = new QPushButton(text, parent);
    result->setFont(Look::pixelFont(9));
    accent ? Look::styleAccentButton(result) : Look::stylePlainButton(result);
    result->setCursor(Qt::PointingHandCursor);
    result->setFixedHeight(28);
    return result;
}

// Line edit with a Browse button that picks a directory.
QWidget *directoryField(QWidget *parent, QLineEdit **editOut, QPushButton **buttonOut, const QString &title)
{
    auto *field = new QWidget(parent);
    field->setFixedWidth(fieldWidth);
    auto *layout = new QHBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto *edit = new QLineEdit(field);
    edit->setFixedHeight(30);
    auto *browse = button(field, QObject::tr("Browse"), false);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, field, [field, edit, title]() {
        const QString directory = QFileDialog::getExistingDirectory(field, title, edit->text());
        if (!directory.isEmpty())
        {
            edit->setText(QDir::toNativeSeparators(directory));
        }
    });
    *editOut = edit;
    if (buttonOut)
    {
        *buttonOut = browse;
    }
    return field;
}

}

SettingsPage::SettingsPage(const QString &distributedPath, int languageIndex, QWidget *parent):
    QWidget(parent),
    distributedPath(distributedPath)
{
    setStyleSheet(Look::inputStyleSheet() + QStringLiteral("QLabel { color: #E0E0E0; }"));
    setFont(Look::pixelFont(10));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // Section list.
    QWidget *tocSurface = nullptr;
    QWidget *tocFrame = framedSurface(this, &tocSurface);
    tocFrame->setFixedWidth(190);
    auto *tocLayout = new QVBoxLayout(tocSurface);
    tocLayout->setContentsMargins(8, 10, 8, 10);
    tocLayout->setSpacing(2);
    sectionButtons = new QButtonGroup(this);
    sectionButtons->setExclusive(true);
    const QStringList titles {
        tr("General"), tr("Blender"), tr("Farm"), tr("Scheduling"), tr("Load detection"), tr("Dependencies"), tr("Cache")
    };
    for (int index = 0; index < titles.size(); ++index)
    {
        auto *entry = new QPushButton(titles[index], tocSurface);
        entry->setCheckable(true);
        entry->setFont(Look::pixelFont(10));
        entry->setStyleSheet(sectionButtonStyleSheet());
        RoundedSurface::Colors entryColors {Qt::transparent, QColor(255, 255, 255, 14), QColor(), QColor()};
        entryColors.checked = QColor(0x2B, 0x2D, 0x40);
        RoundedSurface::install(entry, 8, entryColors);
        entry->setCursor(Qt::PointingHandCursor);
        sectionButtons->addButton(entry, index);
        tocLayout->addWidget(entry);
    }
    tocLayout->addStretch(1);
    layout->addWidget(tocFrame);

    // Form and the bar for settings that could not be saved.
    auto *right = new QVBoxLayout();
    right->setSpacing(10);
    QWidget *formSurface = nullptr;
    QWidget *formFrame = framedSurface(this, &formSurface);
    auto *formSurfaceLayout = new QVBoxLayout(formSurface);
    formSurfaceLayout->setContentsMargins(4, 4, 4, 4);
    scroll = new QScrollArea(formSurface);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }") + Look::scrollBarStyleSheet());
    scroll->viewport()->setAutoFillBackground(false);
    auto *form = new QWidget(scroll);
    form->setStyleSheet(QStringLiteral("background: transparent;"));
    formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(18, 14, 18, 18);
    // Gap between sections; rows inside a section use the tighter 12.
    formLayout->setSpacing(44);
    scroll->setWidget(form);
    formSurfaceLayout->addWidget(scroll);
    right->addWidget(formFrame, 1);

    saveBar = new QWidget(this);
    saveBar->setStyleSheet(QStringLiteral("background: transparent;"));
    RoundedSurface::install(saveBar, 10, QColor(0x2B, 0x2D, 0x40));
    auto *saveLayout = new QHBoxLayout(saveBar);
    saveLayout->setContentsMargins(14, 8, 8, 8);
    saveLayout->setSpacing(8);
    saveBarLabel = new QLabel(saveBar);
    saveBarLabel->setFont(Look::pixelFont(10));
    saveLayout->addWidget(saveBarLabel, 1);
    saveBar->hide();
    right->addWidget(saveBar);
    layout->addLayout(right, 1);

    // General.
    QWidget *general = addSection(Section::General, tr("General"));
    languageBox = new QComboBox(general);
    languageBox->addItem(QStringLiteral("English"));
    languageBox->addItem(QStringLiteral("中文"));
    languageBox->setCurrentIndex(languageIndex);
    languageBox->setFixedSize(fieldWidth, 30);
    addRow(general, tr("Language"), tr("Takes effect after a restart."), languageBox);
    const AppPreferences preferences = AppPreferencesFile::load();
    launchAtLoginSwitch = new ToggleSwitch(general);
    launchAtLoginSwitch->setChecked(preferences.launchAtLogin);
    addRow(general, tr("Launch at login"),
        tr("Start Rendrop in the system tray when you sign in to Windows."), launchAtLoginSwitch);
    shadowsSwitch = new ToggleSwitch(general);
    shadowsSwitch->setChecked(preferences.shadows);
    addRow(general, tr("Shadows"), tr("Soft shadows under cards. Turn off to save some drawing work."), shadowsSwitch);
    restoreDefaultsButton = button(general, tr("Restore"), false);
    addRow(general, tr("Restore defaults"),
        tr("Keeps the farm key, manual nodes, Blender versions and language."), restoreDefaultsButton);

    // Blender.
    QWidget *blender = addSection(Section::Blender, tr("Blender"));
    auto *blenderHelp = new QLabel(tr("Versions you can render with. Pick the active one in the queue toolbar."), blender);
    blenderHelp->setFont(Look::pixelFont(9));
    blenderHelp->setStyleSheet(QStringLiteral("color: rgb(110, 115, 130);"));
    blenderHelp->setWordWrap(true);
    static_cast<QVBoxLayout *>(blender->layout())->addWidget(blenderHelp);
    blenderList = new QVBoxLayout();
    blenderList->setSpacing(6);
    static_cast<QVBoxLayout *>(blender->layout())->addLayout(blenderList);
    addBlenderButton = button(blender, tr("Add Blender..."), true);
    auto *addRowLayout = new QHBoxLayout();
    addRowLayout->addWidget(addBlenderButton);
    addRowLayout->addStretch(1);
    static_cast<QVBoxLayout *>(blender->layout())->addLayout(addRowLayout);

    // Farm.
    QWidget *farm = addSection(Section::Farm, tr("Farm"));
    enabledSwitch = new ToggleSwitch(farm);
    addRow(farm, tr("Distributed rendering"),
        tr("Announce this machine on the LAN, share queued frames and render for other nodes."), enabledSwitch);
    auto *tokenField = new QWidget(farm);
    tokenField->setFixedWidth(fieldWidth + 150);
    auto *tokenLayout = new QHBoxLayout(tokenField);
    tokenLayout->setContentsMargins(0, 0, 0, 0);
    tokenLayout->setSpacing(6);
    tokenEdit = new QLineEdit(tokenField);
    tokenEdit->setEchoMode(QLineEdit::Password);
    tokenEdit->setFixedHeight(30);
    // Pixel Mixed draws the default mask (U+25CF) almost full height; the
    // bullet U+2022 keeps the hidden key the size of the visible one.
    tokenEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { font-family: 'Pixel Mixed'; lineedit-password-character: 8226; }"));
    showTokenButton = button(tokenField, tr("Show"), false);
    copyTokenButton = button(tokenField, tr("Copy"), false);
    regenerateTokenButton = button(tokenField, tr("New"), false);
    regenerateTokenButton->setToolTip(tr("Generate a new farm key"));
    tokenLayout->addWidget(tokenEdit, 1);
    tokenLayout->addWidget(showTokenButton);
    tokenLayout->addWidget(copyTokenButton);
    tokenLayout->addWidget(regenerateTokenButton);
    addRow(farm, tr("Farm key"),
        tr("Shared secret of the render group. Every machine needs the same key."), tokenField);
    auto *httpPort = new QLabel(QStringLiteral("28528"), farm);
    httpPort->setFixedWidth(fieldWidth);
    httpPort->setFont(Look::pixelFont(10));
    httpPort->setStyleSheet(QStringLiteral("color: rgb(150, 155, 170);"));
    addRow(farm, tr("HTTP port"), tr("Fixed. Workers and rendropctl connect here."), httpPort);
    discoveryPortBox = spinBox(farm, 0, 65535, QString());
    addRow(farm, tr("Discovery port"), tr("UDP port for LAN announcements. Must match on every machine."), discoveryPortBox);
    peersEdit = new QPlainTextEdit(farm);
    peersEdit->setFixedSize(fieldWidth, 84);
    peersEdit->setPlaceholderText(QStringLiteral("100.64.0.7\nstudio-03:28528"));
    peersEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: transparent; border: 1px solid transparent;"
        "  padding: 4px 6px; color: white; }") + Look::scrollBarStyleSheet());
    peersEdit->viewport()->setAutoFillBackground(false);
    RoundedSurface::install(peersEdit, 8, RoundedSurface::Colors {
        QColor(0x33, 0x38, 0x42), QColor(), QColor(), QColor(), QColor(65, 65, 85), QColor(110, 110, 180)});
    peersEdit->setFont(Look::pixelFont(10));
    addRow(farm, tr("Manual nodes"),
        tr("One host or host:port per line, for nodes broadcast cannot reach (Tailscale, other subnets)."), peersEdit);

    // Scheduling.
    QWidget *scheduling = addSection(Section::Scheduling, tr("Scheduling"));
    adaptiveSwitch = new ToggleSwitch(scheduling);
    addRow(scheduling, tr("Adaptive chunks"),
        tr("Size each lease from the node's measured speed."), adaptiveSwitch);
    targetLeaseBox = spinBox(scheduling, 10, 86400, tr(" s"));
    addRow(scheduling, tr("Target lease length"), tr("How long one adaptive lease should take."), targetLeaseBox);
    chunkSizeBox = spinBox(scheduling, 1, 100000, tr(" frames"));
    addRow(scheduling, tr("Initial chunk"),
        tr("Frames per lease before a node is measured, or always when adaptive chunks are off."), chunkSizeBox);
    maxChunkBox = spinBox(scheduling, 1, 100000, tr(" frames"));
    addRow(scheduling, tr("Largest chunk"), tr("Upper limit for adaptive leases."), maxChunkBox);
    leaseTimeoutBox = spinBox(scheduling, 5, 86400, tr(" s"));
    addRow(scheduling, tr("Lease timeout"),
        tr("Frames of a node silent for this long go back to the queue."), leaseTimeoutBox);

    // Load detection.
    QWidget *load = addSection(Section::Load, tr("Load detection"));
    auto *loadHelp = new QLabel(
        tr("Load from programs other than Rendrop marks this machine busy, so it stops taking work from other nodes."),
        load);
    loadHelp->setFont(Look::pixelFont(9));
    loadHelp->setStyleSheet(QStringLiteral("color: rgb(110, 115, 130);"));
    loadHelp->setWordWrap(true);
    static_cast<QVBoxLayout *>(load->layout())->addWidget(loadHelp);
    busyThresholdBox = percentBox(load);
    addRow(load, tr("Busy above"), tr("Mean CPU or GPU load that marks the machine busy."), busyThresholdBox);
    busyWindowBox = spinBox(load, 1, 3600, tr(" s"));
    addRow(load, tr("Busy window"), tr("Averaging time for the busy check."), busyWindowBox);
    idleThresholdBox = percentBox(load);
    addRow(load, tr("Idle below"), tr("Both CPU and GPU must stay under this to be idle again."), idleThresholdBox);
    idleWindowBox = spinBox(load, 1, 3600, tr(" s"));
    addRow(load, tr("Idle window"), tr("Averaging time for the idle check."), idleWindowBox);
    sampleIntervalBox = spinBox(load, 100, 60000, tr(" ms"));
    addRow(load, tr("Sample interval"), QString(), sampleIntervalBox);

    // Dependencies.
    QWidget *dependencies = addSection(Section::Dependencies, tr("Dependencies"));
    dependencyModeBox = new QComboBox(dependencies);
    dependencyModeBox->addItem(tr("Send the .blend only"), QStringLiteral("none"));
    dependencyModeBox->addItem(tr("Collect assets with BAT"), QStringLiteral("bat"));
    dependencyModeBox->setFixedSize(fieldWidth, 30);
    addRow(dependencies, tr("Asset handling"),
        tr("With the .blend only, textures and caches must be packed into the file."), dependencyModeBox);
    QWidget *batField = directoryField(dependencies, &batPathEdit, &batBrowseButton, tr("Blender Asset Tracer"));
    addRow(dependencies, tr("BAT location"),
        tr("Directory that contains the blender_asset_tracer package."), batField);

    // Cache.
    QWidget *cache = addSection(Section::Cache, tr("Cache"));
    QWidget *cacheField = directoryField(cache, &cacheEdit, nullptr, tr("Worker cache"));
    cacheEdit->setPlaceholderText(QStringLiteral("%TEMP%\\rendrop-worker"));
    addRow(cache, tr("Worker cache"),
        tr("Where projects from other nodes are kept while this machine renders them."), cacheField);

    formLayout->addStretch(1);
    Look::smoothInputs(this);

    loadDraft();

    // Every edit is written shortly after it stops; typing in a text field
    // does not rewrite the file on each keystroke.
    saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(600);
    QObject::connect(saveTimer, &QTimer::timeout, this, &SettingsPage::save);

    QObject::connect(sectionButtons, &QButtonGroup::idClicked, this, [this](int id) {
        showSection(static_cast<Section>(id));
    });
    QObject::connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, &SettingsPage::updateSectionHighlight);
    QObject::connect(languageBox, &QComboBox::currentIndexChanged, this, &SettingsPage::languageChanged);
    QObject::connect(addBlenderButton, &QPushButton::clicked, this, &SettingsPage::addBlenderRequested);
    QObject::connect(launchAtLoginSwitch, &ToggleSwitch::toggled, this, [](bool checked) {
        AppPreferences preferences = AppPreferencesFile::load();
        preferences.launchAtLogin = checked;
        AppPreferencesFile::save(preferences);
        // A development build may switch it off, but only the installed copy
        // registers itself; the next start of that copy syncs the entry.
        if (!checked || Autostart::isInstalledCopy())
        {
            Autostart::setEnabled(checked);
        }
    });
    QObject::connect(shadowsSwitch, &ToggleSwitch::toggled, this, [](bool checked) {
        AppPreferences preferences = AppPreferencesFile::load();
        preferences.shadows = checked;
        AppPreferencesFile::save(preferences);
        DropShadowWidget::setShadowsEnabled(checked);
    });

    QObject::connect(enabledSwitch, &ToggleSwitch::toggled, this, [this](bool checked) {
        if (checked && tokenEdit->text().trimmed().size() < DistributedConfig::minimumTokenLength)
        {
            tokenEdit->setText(DistributedConfig::generateToken());
        }
        updateFarmFields();
        markDirty();
    });
    QObject::connect(showTokenButton, &QPushButton::clicked, this, [this]() {
        const bool hidden = tokenEdit->echoMode() == QLineEdit::Password;
        tokenEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        showTokenButton->setText(hidden ? tr("Hide") : tr("Show"));
    });
    QObject::connect(copyTokenButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(tokenEdit->text().trimmed());
    });
    QObject::connect(regenerateTokenButton, &QPushButton::clicked, this, [this]() {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(tr("New farm key"));
        box.setText(tr("Machines that keep the old key can no longer work with this one. Generate a new key?"));
        box.setFont(Look::pixelFont(11));
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if (box.exec() == QMessageBox::Yes)
        {
            tokenEdit->setText(DistributedConfig::generateToken());
        }
    });
    QObject::connect(adaptiveSwitch, &ToggleSwitch::toggled, this, [this]() {
        updateFarmFields();
        markDirty();
    });
    QObject::connect(dependencyModeBox, &QComboBox::currentIndexChanged, this, [this]() {
        updateFarmFields();
        markDirty();
    });
    for (QLineEdit *edit : {tokenEdit, batPathEdit, cacheEdit})
    {
        QObject::connect(edit, &QLineEdit::textChanged, this, &SettingsPage::markDirty);
    }
    QObject::connect(peersEdit, &QPlainTextEdit::textChanged, this, &SettingsPage::markDirty);
    for (QSpinBox *box : {discoveryPortBox, chunkSizeBox, targetLeaseBox, maxChunkBox, leaseTimeoutBox,
             busyWindowBox, idleWindowBox, sampleIntervalBox})
    {
        QObject::connect(box, &QSpinBox::valueChanged, this, &SettingsPage::markDirty);
    }
    for (QDoubleSpinBox *box : {busyThresholdBox, idleThresholdBox})
    {
        QObject::connect(box, &QDoubleSpinBox::valueChanged, this, &SettingsPage::markDirty);
    }

    QObject::connect(restoreDefaultsButton, &QPushButton::clicked, this, &SettingsPage::restoreDefaults);

    sectionButtons->button(0)->setChecked(true);
}

SettingsPage::~SettingsPage()
{
    // Closing within the debounce delay must not drop the last edit.
    flushPendingSave();
}

QWidget *SettingsPage::addSection(Section section, const QString &title)
{
    auto *widget = new QWidget(scroll->widget());
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *heading = new QLabel(title, widget);
    heading->setFont(Look::pixelFont(14));
    heading->setStyleSheet(QStringLiteral("color: white;"));
    layout->addWidget(heading);
    formLayout->addWidget(widget);
    sections.resize(std::max(sections.size(), static_cast<size_t>(section) + 1), nullptr);
    sections[static_cast<size_t>(section)] = widget;
    return widget;
}

void SettingsPage::addRow(QWidget *section, const QString &label, const QString &help, QWidget *field)
{
    auto *row = new QHBoxLayout();
    row->setSpacing(24);
    auto *text = new QVBoxLayout();
    text->setSpacing(3);
    auto *name = new QLabel(label, section);
    name->setFont(Look::pixelFont(10));
    name->setStyleSheet(QStringLiteral("color: rgb(224, 224, 232);"));
    text->addWidget(name);
    if (!help.isEmpty())
    {
        auto *description = new QLabel(help, section);
        description->setFont(Look::pixelFont(9));
        description->setStyleSheet(QStringLiteral("color: rgb(110, 115, 130);"));
        description->setWordWrap(true);
        text->addWidget(description);
    }
    text->addStretch(1);
    row->addLayout(text, 1);
    row->addWidget(field, 0, Qt::AlignTop | Qt::AlignRight);
    static_cast<QVBoxLayout *>(section->layout())->addLayout(row);
}

void SettingsPage::setBlenderVersions(const std::vector<BlenderVersionManager::BlenderVersion> &versions)
{
    while (QLayoutItem *item = blenderList->takeAt(0))
    {
        if (item->widget())
        {
            item->widget()->deleteLater();
        }
        delete item;
    }
    removeBlenderButtons.clear();
    QWidget *owner = sections[static_cast<size_t>(Section::Blender)];
    if (versions.empty())
    {
        auto *empty = new QLabel(tr("No Blender added yet."), owner);
        empty->setFont(Look::pixelFont(10));
        empty->setStyleSheet(QStringLiteral("color: rgb(215, 165, 70);"));
        blenderList->addWidget(empty);
    }
    for (const auto &version : versions)
    {
        auto *row = new QWidget(owner);
        row->setStyleSheet(QStringLiteral("background: transparent;"));
        RoundedSurface::install(row, 8, QColor(43, 45, 48));
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 6, 6, 6);
        layout->setSpacing(14);
        auto *name = new QLabel(version.version, row);
        name->setFont(Look::pixelFont(10));
        name->setStyleSheet(QStringLiteral("color: white;"));
        name->setMinimumWidth(110);
        auto *path = new QLabel(row);
        path->setFont(Look::pixelFont(9));
        path->setStyleSheet(QStringLiteral("color: rgb(110, 115, 130);"));
        path->setText(QFontMetrics(path->font()).elidedText(version.path, Qt::ElideMiddle, 420));
        path->setToolTip(version.path);
        auto *remove = button(row, tr("Remove"), false);
        remove->setEnabled(blenderEditingEnabled);
        const QString key = version.version;
        QObject::connect(remove, &QPushButton::clicked, this, [this, key]() {
            emit removeBlenderRequested(key);
        });
        removeBlenderButtons.push_back(remove);
        layout->addWidget(name);
        layout->addWidget(path, 1);
        layout->addWidget(remove);
        blenderList->addWidget(row);
    }
}

void SettingsPage::setBlenderEditingEnabled(bool enabled)
{
    blenderEditingEnabled = enabled;
    addBlenderButton->setEnabled(enabled && !addBlenderBusy);
    for (QPushButton *remove : removeBlenderButtons)
    {
        remove->setEnabled(enabled);
    }
}

void SettingsPage::setAddBlenderBusy(bool busy)
{
    addBlenderBusy = busy;
    addBlenderButton->setText(busy ? tr("Checking Blender...") : tr("Add Blender..."));
    addBlenderButton->setEnabled(blenderEditingEnabled && !busy);
}

void SettingsPage::showSection(Section section)
{
    const QWidget *target = sections[static_cast<size_t>(section)];
    if (!target)
    {
        return;
    }
    scroll->verticalScrollBar()->setValue(target->y() - formLayout->contentsMargins().top());
    if (QAbstractButton *entry = sectionButtons->button(static_cast<int>(section)))
    {
        entry->setChecked(true);
    }
}

void SettingsPage::updateSectionHighlight()
{
    const QScrollBar *bar = scroll->verticalScrollBar();
    int current = 0;
    if (bar->value() >= bar->maximum() && bar->maximum() > 0)
    {
        // The last sections may be too short to reach the top.
        current = static_cast<int>(sections.size()) - 1;
    }
    else
    {
        for (size_t index = 0; index < sections.size(); ++index)
        {
            if (sections[index] && sections[index]->y() <= bar->value() + 40)
            {
                current = static_cast<int>(index);
            }
        }
    }
    if (QAbstractButton *entry = sectionButtons->button(current); entry && !entry->isChecked())
    {
        entry->setChecked(true);
    }
}

void SettingsPage::loadDraft()
{
    loaded = DistributedConfig::load(distributedPath);
    populateForm(loaded);
}

void SettingsPage::populateForm(const DistributedSettings &settings)
{
    populating = true;
    enabledSwitch->setChecked(settings.enabled);
    tokenEdit->setText(settings.token);
    discoveryPortBox->setValue(settings.discoveryPort);
    peersEdit->setPlainText(settings.peers.join(QLatin1Char('\n')));
    adaptiveSwitch->setChecked(settings.adaptiveChunks);
    chunkSizeBox->setValue(settings.chunkSize);
    targetLeaseBox->setValue(settings.targetLeaseSeconds);
    maxChunkBox->setValue(settings.maxChunkSize);
    leaseTimeoutBox->setValue(settings.leaseTimeoutSeconds);
    busyThresholdBox->setValue(settings.load.busyThresholdPercent);
    busyWindowBox->setValue(settings.load.busyWindowSeconds);
    idleThresholdBox->setValue(settings.load.idleThresholdPercent);
    idleWindowBox->setValue(settings.load.idleWindowSeconds);
    sampleIntervalBox->setValue(settings.load.sampleIntervalMs);
    dependencyModeBox->setCurrentIndex(settings.dependencyMode == QStringLiteral("bat") ? 1 : 0);
    batPathEdit->setText(settings.batPath);
    cacheEdit->setText(settings.workerCacheDirectory);
    populating = false;
    updateFarmFields();
}

void SettingsPage::restoreDefaults()
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Restore defaults"));
    box.setText(tr("Reset every setting to its default? The farm key, manual nodes, Blender versions and language are kept."));
    box.setFont(Look::pixelFont(11));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
    {
        return;
    }
    DistributedSettings defaults;
    // Other machines pair through the key and the manual list is typed by
    // hand; losing either costs more than any default saves.
    DistributedSettings current;
    readForm(current);
    defaults.token = current.token.size() >= DistributedConfig::minimumTokenLength
        ? current.token
        : DistributedConfig::generateToken();
    defaults.peers = current.peers;
    populateForm(defaults);
    // The switches save and apply themselves when they flip.
    const AppPreferences preferences;
    launchAtLoginSwitch->setChecked(preferences.launchAtLogin);
    shadowsSwitch->setChecked(preferences.shadows);
    saveTimer->stop();
    save();
}

void SettingsPage::readForm(DistributedSettings &settings) const
{
    settings.enabled = enabledSwitch->isChecked();
    settings.token = tokenEdit->text().trimmed();
    settings.discoveryPort = discoveryPortBox->value();
    settings.peers.clear();
    for (const QString &line : peersEdit->toPlainText().split(QLatin1Char('\n')))
    {
        const QString peer = line.trimmed();
        if (!peer.isEmpty() && !settings.peers.contains(peer))
        {
            settings.peers.append(peer);
        }
    }
    settings.adaptiveChunks = adaptiveSwitch->isChecked();
    settings.chunkSize = chunkSizeBox->value();
    settings.targetLeaseSeconds = targetLeaseBox->value();
    settings.maxChunkSize = std::max(maxChunkBox->value(), settings.chunkSize);
    settings.leaseTimeoutSeconds = leaseTimeoutBox->value();
    settings.load.busyThresholdPercent = busyThresholdBox->value();
    settings.load.busyWindowSeconds = busyWindowBox->value();
    settings.load.idleThresholdPercent = idleThresholdBox->value();
    settings.load.idleWindowSeconds = idleWindowBox->value();
    settings.load.sampleIntervalMs = sampleIntervalBox->value();
    settings.dependencyMode = dependencyModeBox->currentData().toString();
    settings.batPath = batPathEdit->text().trimmed();
    settings.workerCacheDirectory = cacheEdit->text().trimmed();
}

void SettingsPage::markDirty()
{
    if (populating)
    {
        return;
    }
    saveTimer->start();
}

void SettingsPage::flushPendingSave()
{
    if (saveTimer->isActive())
    {
        saveTimer->stop();
        save();
    }
}

void SettingsPage::save()
{
    // Start from the file as it is now: the Nodes page may have switched
    // acceptRemoteWork since the draft was loaded.
    DistributedSettings settings = DistributedConfig::load(distributedPath);
    readForm(settings);
    if (settings.enabled && settings.token.size() < DistributedConfig::minimumTokenLength)
    {
        // Half-typed keys stay in the field; the file keeps the last valid one.
        saveBarLabel->setText(
            tr("The farm key needs at least %1 characters. Not saved yet.").arg(DistributedConfig::minimumTokenLength));
        saveBarLabel->setStyleSheet(QStringLiteral("color: rgb(215, 165, 70);"));
        saveBar->show();
        return;
    }
    if (!DistributedConfig::save(distributedPath, settings))
    {
        saveBarLabel->setText(tr("Failed to save distributed.json."));
        saveBarLabel->setStyleSheet(QStringLiteral("color: rgb(225, 110, 110);"));
        saveBar->show();
        return;
    }
    loaded = settings;
    saveBar->hide();
    emit farmSettingsSaved(settings);
}

void SettingsPage::updateFarmFields()
{
    const bool farm = enabledSwitch->isChecked();
    for (QWidget *field : std::initializer_list<QWidget *> {
             tokenEdit, showTokenButton, copyTokenButton, regenerateTokenButton, discoveryPortBox, peersEdit})
    {
        field->setEnabled(farm);
    }
    const bool adaptive = adaptiveSwitch->isChecked();
    targetLeaseBox->setEnabled(adaptive);
    maxChunkBox->setEnabled(adaptive);
    const bool bat = dependencyModeBox->currentData().toString() == QStringLiteral("bat");
    batPathEdit->setEnabled(bat);
    batBrowseButton->setEnabled(bat);
}
