import AppKit
import ScreenSaver
import StarfieldCore

private func cgColor(_ c: sf_color) -> CGColor {
    CGColor(srgbRed: CGFloat(c.r), green: CGFloat(c.g), blue: CGFloat(c.b), alpha: CGFloat(c.a))
}

private func parseColor(_ hex: String, fallback: String) -> sf_color {
    var c = sf_color()
    if sf_parse_color(hex, &c) == 0 { sf_parse_color(fallback, &c) }
    return c
}

private func context(_ ctx: UnsafeMutableRawPointer?) -> CGContext? {
    guard let ctx else { return nil }
    return Unmanaged<CGContext>.fromOpaque(ctx).takeUnretainedValue()
}

private let drawFill: @convention(c) (UnsafeMutableRawPointer?, sf_color) -> Void = { ctx, color in
    guard let cg = context(ctx) else { return }
    cg.setFillColor(cgColor(color))
    cg.fill(CGRect(x: -1, y: -1, width: 100_000, height: 100_000))
}

private let drawLine: @convention(c) (UnsafeMutableRawPointer?, Float, Float, Float, Float, Float, sf_color) -> Void = {
    ctx, x0, y0, x1, y1, width, color in
    guard let cg = context(ctx) else { return }
    cg.setStrokeColor(cgColor(color))
    cg.setLineWidth(CGFloat(width))
    cg.beginPath()
    cg.move(to: CGPoint(x: CGFloat(x0), y: CGFloat(y0)))
    cg.addLine(to: CGPoint(x: CGFloat(x1), y: CGFloat(y1)))
    cg.strokePath()
}

private let drawCircle: @convention(c) (UnsafeMutableRawPointer?, Float, Float, Float, sf_color) -> Void = {
    ctx, x, y, radius, color in
    guard let cg = context(ctx) else { return }
    let r = CGFloat(radius)
    cg.setFillColor(cgColor(color))
    cg.fillEllipse(in: CGRect(x: CGFloat(x) - r, y: CGFloat(y) - r, width: r * 2, height: r * 2))
}

@objc(StarfieldView)
final class StarfieldView: ScreenSaverView {
    private let core: OpaquePointer
    private var settings = StarfieldSettings.load()
    private var lastTick: CFTimeInterval = 0
    private var configureController: ConfigureSheetController?
    private lazy var clockFormatter = DateFormatter()

    /// The trails need last frame's pixels, so the stars are drawn into this
    /// bitmap, which is never cleared, and the view copies it to the screen.
    private var canvas: CGContext?

    override init?(frame: NSRect, isPreview: Bool) {
        core = sf_create(UInt32.random(in: 1...UInt32.max))
        super.init(frame: frame, isPreview: isPreview)
        animationTimeInterval = 1.0 / 60.0
        applySettings()
        sf_resize(core, Float(frame.width), Float(frame.height))

        if !isPreview {
            // Since macOS 14 the legacyScreenSaver host can keep a saver running after
            // the screen saver ends; quit with it so it doesn't keep drawing in the background.
            DistributedNotificationCenter.default().addObserver(
                self, selector: #selector(screenSaverWillStop),
                name: NSNotification.Name("com.apple.screensaver.willstop"), object: nil)
        }
    }

    required init?(coder: NSCoder) {
        core = sf_create(UInt32.random(in: 1...UInt32.max))
        super.init(coder: coder)
    }

    deinit {
        DistributedNotificationCenter.default().removeObserver(self)
        sf_destroy(core)
    }

    override var hasConfigureSheet: Bool { true }

    override var configureSheet: NSWindow? {
        let controller = ConfigureSheetController(settings: StarfieldSettings.load()) { [weak self] saved in
            self?.settings = saved
            self?.applySettings()
        }
        configureController = controller
        return controller.window
    }

    @objc private func screenSaverWillStop() {
        exit(0)
    }

    // MARK: - Lifecycle

    override func startAnimation() {
        super.startAnimation()
        settings = StarfieldSettings.load()
        applySettings()
        lastTick = 0
    }

    override func setFrameSize(_ newSize: NSSize) {
        super.setFrameSize(newSize)
        sf_resize(core, Float(newSize.width), Float(newSize.height))
        canvas = nil
    }

    override func viewDidChangeBackingProperties() {
        super.viewDidChangeBackingProperties()
        canvas = nil
    }

    private func applySettings() {
        var o = sf_default_options()
        let base = ColorPreset.all[0]
        o.background = parseColor(settings.background, fallback: base.background)
        o.star = parseColor(settings.stars, fallback: base.stars)
        o.accent = parseColor(settings.accent, fallback: base.accent)
        o.accent_share = Float(settings.accentPercent / 100)
        o.speed = Float(settings.speed)
        o.trails = Float(settings.trails)
        o.reduced_motion = NSWorkspace.shared.accessibilityDisplayShouldReduceMotion ? 1 : 0
        o.compact = isPreview ? 1 : 0
        sf_set_options(core, &o)
        clockFormatter.dateFormat = settings.use24Hour ? "HH:mm" : "h:mm a"
        needsDisplay = true
    }

    /// A bitmap the size of the view in pixels, with a top-left origin like the site's canvas.
    private func makeCanvas() -> CGContext? {
        let scale = window?.backingScaleFactor ?? NSScreen.main?.backingScaleFactor ?? 2
        let w = Int((bounds.width * scale).rounded()), h = Int((bounds.height * scale).rounded())
        guard w > 0, h > 0, let space = CGColorSpace(name: CGColorSpace.sRGB),
              let cg = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: 0,
                                 space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
        else { return nil }
        cg.translateBy(x: 0, y: CGFloat(h))
        cg.scaleBy(x: scale, y: -scale)
        cg.setLineCap(.round)
        cg.setShouldAntialias(true)
        return cg
    }

    // MARK: - Drawing

    override func animateOneFrame() {
        let now = CACurrentMediaTime()
        let dt = lastTick == 0 ? 0 : now - lastTick
        lastTick = now
        sf_step(core, Float(dt))

        if canvas == nil { canvas = makeCanvas(); _ = sf_take_needs_clear(core); clearCanvas() }
        guard let cg = canvas else { return }
        if sf_take_needs_clear(core) != 0 { clearCanvas() }
        var renderer = sf_renderer(
            ctx: Unmanaged.passUnretained(cg).toOpaque(),
            fill: drawFill,
            line: drawLine,
            circle: drawCircle)
        sf_render(core, &renderer)
        needsDisplay = true
    }

    private func clearCanvas() {
        guard let cg = canvas else { return }
        cg.setFillColor(cgColor(parseColor(settings.background, fallback: ColorPreset.all[0].background)))
        cg.fill(CGRect(x: -1, y: -1, width: bounds.width + 2, height: bounds.height + 2))
    }

    override func draw(_ rect: NSRect) {
        guard let cg = NSGraphicsContext.current?.cgContext else { return }
        if let image = canvas?.makeImage() {
            cg.interpolationQuality = .none
            cg.draw(image, in: bounds)
        } else {
            cg.setFillColor(cgColor(parseColor(settings.background, fallback: ColorPreset.all[0].background)))
            cg.fill(bounds)
        }
        if settings.showClock || settings.label != nil { drawClock() }
    }

    /// Bottom-left: the time in large type, with the optional label under it.
    private func drawClock() {
        let ink = NSColor(cgColor: cgColor(parseColor(settings.stars, fallback: ColorPreset.all[0].stars))) ?? .white
        let scale = isPreview ? max(0.25, bounds.height / 900) : 1
        let margin = 32 * scale
        var y = margin // the view isn't flipped: y grows upward from the bottom edge

        if let text = settings.label {
            let label = NSAttributedString(string: text, attributes: [
                .font: NSFont.systemFont(ofSize: 14 * scale, weight: .regular),
                .foregroundColor: ink.withAlphaComponent(0.5),
            ])
            label.draw(at: NSPoint(x: margin, y: y))
            y += label.size().height - 6 * scale
        }
        guard settings.showClock else { return }
        let time = NSAttributedString(string: clockFormatter.string(from: Date()), attributes: [
            .font: NSFont.monospacedDigitSystemFont(ofSize: 48 * scale, weight: .semibold),
            .foregroundColor: ink.withAlphaComponent(0.8),
            .kern: -1.0 * scale,
        ])
        time.draw(at: NSPoint(x: margin, y: y))
    }
}
