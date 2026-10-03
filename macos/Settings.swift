import Foundation
import ScreenSaver

/// A color scheme for the saver. Colors are "#rrggbb" strings, the same format the core parses.
struct ColorPreset {
    let name: String
    let background: String
    let stars: String
    let accent: String

    static let all: [ColorPreset] = [
        ColorPreset(name: "Original", background: "#0b0b0a", stars: "#eeebe4", accent: "#f06a2a"),
        ColorPreset(name: "Classic", background: "#000000", stars: "#ffffff", accent: "#ffffff"),
        ColorPreset(name: "Deep space", background: "#05081a", stars: "#d6e2ff", accent: "#ffcf6a"),
        ColorPreset(name: "Green terminal", background: "#000000", stars: "#33ff66", accent: "#ccffcc"),
        ColorPreset(name: "Amber terminal", background: "#0a0700", stars: "#ffb000", accent: "#fff1c4"),
        ColorPreset(name: "Synthwave", background: "#12041f", stars: "#4de8ff", accent: "#ff3fb4"),
        ColorPreset(name: "Paper", background: "#f4f1ea", stars: "#1d1c1a", accent: "#e8591a"),
    ]
}

/// Everything the user can set, stored in the screen saver's own defaults domain.
struct StarfieldSettings {
    static let moduleName = "com.maxhayim.screensaver-starfield"

    var background = ColorPreset.all[0].background
    var stars = ColorPreset.all[0].stars
    var accent = ColorPreset.all[0].accent
    var accentPercent = 7.0 // share of accent stars, 0...100
    var speed = 1.0         // 1 = the site's speed
    var trails = 0.58       // how long the streaks linger, 0...0.95
    var showClock = true
    var use24Hour = false

    private static var defaults: UserDefaults? { ScreenSaverDefaults(forModuleWithName: moduleName) }

    static func load() -> StarfieldSettings {
        var s = StarfieldSettings()
        guard let d = defaults else { return s }
        s.background = d.string(forKey: "background") ?? s.background
        s.stars = d.string(forKey: "stars") ?? s.stars
        s.accent = d.string(forKey: "accent") ?? s.accent
        if d.object(forKey: "accentPercent") != nil { s.accentPercent = d.double(forKey: "accentPercent") }
        if d.object(forKey: "speed") != nil { s.speed = d.double(forKey: "speed") }
        if d.object(forKey: "trails") != nil { s.trails = d.double(forKey: "trails") }
        if d.object(forKey: "showClock") != nil { s.showClock = d.bool(forKey: "showClock") }
        if d.object(forKey: "use24Hour") != nil { s.use24Hour = d.bool(forKey: "use24Hour") }
        return s
    }

    func save() {
        guard let d = StarfieldSettings.defaults else { return }
        d.set(background, forKey: "background")
        d.set(stars, forKey: "stars")
        d.set(accent, forKey: "accent")
        d.set(accentPercent, forKey: "accentPercent")
        d.set(speed, forKey: "speed")
        d.set(trails, forKey: "trails")
        d.set(showClock, forKey: "showClock")
        d.set(use24Hour, forKey: "use24Hour")
        d.synchronize()
    }
}
