import AppKit

/// The "Options…" sheet in System Settings › Screen Saver.
final class ConfigureSheetController: NSObject {
    let window: NSWindow
    private var settings: StarfieldSettings
    private let onSave: (StarfieldSettings) -> Void

    private let presetPopup = NSPopUpButton()
    private let backgroundWell = NSColorWell()
    private let starsWell = NSColorWell()
    private let accentWell = NSColorWell()
    private let accentSlider = NSSlider(value: 7, minValue: 0, maxValue: 50, target: nil, action: nil)
    private let accentValue = NSTextField(labelWithString: "")
    private let speedSlider = NSSlider(value: 1, minValue: 0.25, maxValue: 3, target: nil, action: nil)
    private let speedValue = NSTextField(labelWithString: "")
    private let trailsSlider = NSSlider(value: 0.58, minValue: 0, maxValue: 0.95, target: nil, action: nil)
    private let trailsValue = NSTextField(labelWithString: "")
    private let clockCheck = NSButton(checkboxWithTitle: "Show the clock", target: nil, action: nil)
    private let hourCheck = NSButton(checkboxWithTitle: "24-hour time", target: nil, action: nil)

    init(settings: StarfieldSettings, onSave: @escaping (StarfieldSettings) -> Void) {
        self.settings = settings
        self.onSave = onSave
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 440, height: 360),
                          styleMask: [.titled], backing: .buffered, defer: false)
        super.init()
        build()
        load()
    }

    // MARK: - Layout

    private func build() {
        presetPopup.addItems(withTitles: ColorPreset.all.map(\.name) + ["Custom"])
        presetPopup.target = self
        presetPopup.action = #selector(presetChanged)
        for well in [backgroundWell, starsWell, accentWell] {
            well.target = self
            well.action = #selector(colorChanged)
            if #available(macOS 13.0, *) { well.colorWellStyle = .minimal }
            well.widthAnchor.constraint(equalToConstant: 44).isActive = true
            well.heightAnchor.constraint(equalToConstant: 24).isActive = true
        }
        let wells = NSStackView(views: [
            labeled("Background", backgroundWell), labeled("Stars", starsWell), labeled("Accent stars", accentWell),
        ])
        wells.spacing = 14

        for (slider, value) in [(accentSlider, accentValue), (speedSlider, speedValue), (trailsSlider, trailsValue)] {
            slider.target = self
            slider.action = #selector(sliderChanged)
            slider.widthAnchor.constraint(equalToConstant: 200).isActive = true
            value.font = .monospacedDigitSystemFont(ofSize: NSFont.smallSystemFontSize, weight: .regular)
            value.textColor = .secondaryLabelColor
            value.widthAnchor.constraint(equalToConstant: 60).isActive = true
        }

        let clockRow = NSStackView(views: [clockCheck, hourCheck])
        clockRow.spacing = 16

        let resetButton = NSButton(title: "Reset to defaults", target: self, action: #selector(resetDefaults))
        resetButton.bezelStyle = .rounded
        resetButton.controlSize = .small

        let grid = NSGridView(views: [
            [header("Colors"), NSGridCell.emptyContentView],
            [label("Preset"), presetPopup],
            [NSGridCell.emptyContentView, wells],
            [header("Flight"), NSGridCell.emptyContentView],
            [label("Accent stars"), row(accentSlider, accentValue)],
            [label("Speed"), row(speedSlider, speedValue)],
            [label("Trails"), row(trailsSlider, trailsValue)],
            [header("Clock"), NSGridCell.emptyContentView],
            [NSGridCell.emptyContentView, clockRow],
        ])
        grid.rowSpacing = 8
        grid.columnSpacing = 10
        grid.column(at: 0).xPlacement = .trailing
        for r in [0, 3, 7] {
            grid.row(at: r).topPadding = r == 0 ? 0 : 10
            grid.row(at: r).mergeCells(in: NSRange(location: 0, length: 2))
            grid.cell(atColumnIndex: 0, rowIndex: r).xPlacement = .leading
        }

        let cancel = NSButton(title: "Cancel", target: self, action: #selector(cancel))
        cancel.keyEquivalent = "\u{1b}"
        let ok = NSButton(title: "OK", target: self, action: #selector(save))
        ok.keyEquivalent = "\r"
        let spacer = NSView()
        spacer.setContentHuggingPriority(.defaultLow, for: .horizontal)
        let buttons = NSStackView(views: [resetButton, spacer, cancel, ok])
        buttons.widthAnchor.constraint(equalTo: grid.widthAnchor).isActive = true

        let root = NSStackView(views: [grid, buttons])
        root.orientation = .vertical
        root.alignment = .trailing
        root.spacing = 18
        root.edgeInsets = NSEdgeInsets(top: 20, left: 20, bottom: 20, right: 20)
        window.contentView = root
    }

    private func label(_ text: String) -> NSTextField { NSTextField(labelWithString: text) }

    private func header(_ text: String) -> NSTextField {
        let f = NSTextField(labelWithString: text)
        f.font = .boldSystemFont(ofSize: NSFont.systemFontSize)
        return f
    }

    private func row(_ views: NSView...) -> NSStackView {
        let stack = NSStackView(views: views)
        stack.spacing = 8
        return stack
    }

    private func labeled(_ text: String, _ view: NSView) -> NSStackView {
        let caption = NSTextField(labelWithString: text)
        caption.font = .systemFont(ofSize: NSFont.smallSystemFontSize)
        caption.textColor = .secondaryLabelColor
        let stack = NSStackView(views: [view, caption])
        stack.orientation = .vertical
        stack.spacing = 4
        return stack
    }

    // MARK: - Values

    private func load() {
        backgroundWell.color = Self.color(settings.background)
        starsWell.color = Self.color(settings.stars)
        accentWell.color = Self.color(settings.accent)
        accentSlider.doubleValue = settings.accentPercent
        speedSlider.doubleValue = settings.speed
        trailsSlider.doubleValue = settings.trails
        clockCheck.state = settings.showClock ? .on : .off
        hourCheck.state = settings.use24Hour ? .on : .off
        selectMatchingPreset()
        sliderChanged()
    }

    private func readFields() {
        settings.background = Self.hex(backgroundWell.color)
        settings.stars = Self.hex(starsWell.color)
        settings.accent = Self.hex(accentWell.color)
        settings.accentPercent = accentSlider.doubleValue.rounded()
        settings.speed = (speedSlider.doubleValue * 20).rounded() / 20
        settings.trails = trailsSlider.doubleValue
        settings.showClock = clockCheck.state == .on
        settings.use24Hour = hourCheck.state == .on
    }

    private func selectMatchingPreset() {
        let current = [backgroundWell, starsWell, accentWell].map { Self.hex($0.color) }
        let index = ColorPreset.all.firstIndex {
            [$0.background, $0.stars, $0.accent].map { $0.lowercased() } == current
        }
        presetPopup.selectItem(at: index ?? ColorPreset.all.count)
    }

    static func color(_ hex: String) -> NSColor {
        var s = hex.trimmingCharacters(in: .whitespaces)
        if s.hasPrefix("#") { s.removeFirst() }
        guard s.count == 6, let v = UInt32(s, radix: 16) else { return .white }
        return NSColor(srgbRed: CGFloat((v >> 16) & 0xff) / 255, green: CGFloat((v >> 8) & 0xff) / 255,
                       blue: CGFloat(v & 0xff) / 255, alpha: 1)
    }

    static func hex(_ color: NSColor) -> String {
        guard let c = color.usingColorSpace(.sRGB) else { return "#ffffff" }
        let v = [c.redComponent, c.greenComponent, c.blueComponent].map { Int(($0 * 255).rounded()).clamped(0, 255) }
        return String(format: "#%02x%02x%02x", v[0], v[1], v[2])
    }

    // MARK: - Actions

    @objc private func presetChanged() {
        let i = presetPopup.indexOfSelectedItem
        guard i < ColorPreset.all.count else { return }
        let p = ColorPreset.all[i]
        backgroundWell.color = Self.color(p.background)
        starsWell.color = Self.color(p.stars)
        accentWell.color = Self.color(p.accent)
    }

    @objc private func colorChanged() { selectMatchingPreset() }

    @objc private func sliderChanged() {
        accentValue.stringValue = "\(Int(accentSlider.doubleValue.rounded()))%"
        speedValue.stringValue = String(format: "%.2g×", (speedSlider.doubleValue * 20).rounded() / 20)
        let t = trailsSlider.doubleValue
        trailsValue.stringValue = t < 0.3 ? "Short" : t < 0.7 ? "Medium" : "Long"
    }

    @objc private func resetDefaults() {
        settings = StarfieldSettings()
        load()
    }

    @objc private func cancel() { close() }

    @objc private func save() {
        readFields()
        settings.save()
        onSave(settings)
        close()
    }

    private func close() {
        NSColorPanel.shared.close()
        if let parent = window.sheetParent { parent.endSheet(window) } else { window.close() }
    }
}

private extension Int {
    func clamped(_ lo: Int, _ hi: Int) -> Int { Swift.min(Swift.max(self, lo), hi) }
}
