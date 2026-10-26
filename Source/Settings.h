// Settings.h — per-user preferences shared by every Descant on the machine:
// tooltips on or off. User presets are kept in a folder beside this file.
//
//   macOS:   ~/Library/Application Support/Descant/Descant.settings
//   Windows: %APPDATA%\Descant\Descant.settings
//   Linux:   ~/.config/Descant/Descant.settings
//
// Message thread only.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace descant
{
struct SettingsStore
{
    SettingsStore();
    juce::PropertiesFile props;
};

class Settings
{
public:
    bool tooltipsEnabled() const;          // default: on
    void setTooltipsEnabled (bool);

    static juce::File file();

private:
    juce::SharedResourcePointer<SettingsStore> store;
};
} // namespace descant
