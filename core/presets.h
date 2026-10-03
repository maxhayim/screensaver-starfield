/*
 * Color presets for the Windows and Linux savers. Keep in step with
 * ColorPreset in macos/Settings.swift and linux/starfield.xml.
 */
#ifndef SF_PRESETS_H
#define SF_PRESETS_H

typedef struct {
    const char *name, *background, *stars, *accent;
} sf_preset;

static const sf_preset SF_PRESETS[] = {
    {"Original", "#0b0b0a", "#eeebe4", "#f06a2a"},
    {"Classic", "#000000", "#ffffff", "#ffffff"},
    {"Deep space", "#05081a", "#d6e2ff", "#ffcf6a"},
    {"Green terminal", "#000000", "#33ff66", "#ccffcc"},
    {"Amber terminal", "#0a0700", "#ffb000", "#fff1c4"},
    {"Synthwave", "#12041f", "#4de8ff", "#ff3fb4"},
    {"Paper", "#f4f1ea", "#1d1c1a", "#e8591a"},
};

#define SF_PRESET_COUNT ((int)(sizeof SF_PRESETS / sizeof SF_PRESETS[0]))

/* Defaults for the settings that aren't colors. */
#define SF_DEFAULT_ACCENT_PERCENT 7
#define SF_DEFAULT_SPEED_PERCENT 100
#define SF_DEFAULT_TRAILS_PERCENT 58

#endif
