// Presets.h — factory presets, and user presets saved as files.
//
// User presets live in a folder next to the settings file:
//   macOS:   ~/Library/Application Support/Descant/Presets
//   Windows: %APPDATA%\Descant\Presets
//   Linux:   ~/.config/Descant/Presets
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace descant
{
struct FactoryPreset
{
    const char* name;
    const char* blurb;                                   // one line, shown under the preset name
    std::vector<std::pair<const char*, float>> values;   // plain values; anything not listed is default
};

const std::vector<FactoryPreset>& factoryPresets();

juce::File userPresetFolder();
juce::Array<juce::File> userPresets();   // sorted by name
inline constexpr const char* kPresetExtension = ".descant";
} // namespace descant
