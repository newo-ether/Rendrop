// load_settings.h

#ifndef LOAD_SETTINGS_H
#define LOAD_SETTINGS_H

// Thresholds for deciding whether this machine is busy with work that is not
// Rendrop's own. All percentages are 0-100 of the whole machine.
struct LoadSettings
{
    int sampleIntervalMs = 1000;
    // Busy when the mean foreign CPU or GPU load over busyWindowSeconds rises
    // above busyThresholdPercent.
    double busyThresholdPercent = 25.0;
    int busyWindowSeconds = 10;
    // Idle again only after both means over idleWindowSeconds stay below
    // idleThresholdPercent. The gap between the two thresholds and windows
    // keeps the state from flapping.
    double idleThresholdPercent = 10.0;
    int idleWindowSeconds = 60;
};

#endif // LOAD_SETTINGS_H
