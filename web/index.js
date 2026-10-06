// Starfield for web pages: the same C core as the native savers, compiled to
// WebAssembly, drawing into a <canvas>. No dependencies.
//
//   import { createSaver, cleanSettings } from "screensaver-starfield";
//   const saver = createSaver(canvas, { settings: cleanSettings(saved) });
//   saver.update(newSettings); saver.destroy();

import wasmBase64 from "./wasm.js";
import { ACCENT_PERCENT, LABEL_MAX, PRESETS, SPEED_PERCENT, TRAILS_PERCENT } from "./presets.js";

export const ID = "starfield";
export const NAME = "Starfield";

const ORIGINAL = PRESETS[0];

// The settings, in the order and with the labels of the native Options sheet. A page builds its settings UI from this.
// type: "preset" | "color" | "range" | "toggle" | "choice" | "text" | "password"
export const SETTINGS = [
  { key: "preset", label: "Colors", type: "preset", presets: PRESETS, default: ORIGINAL.name },
  { key: "background", label: "Background", type: "color", default: ORIGINAL.colors.background },
  { key: "stars", label: "Stars", type: "color", default: ORIGINAL.colors.stars },
  { key: "accent", label: "Accent stars", type: "color", default: ORIGINAL.colors.accent },
  { key: "accentPercent", label: "Accent stars", type: "range", min: 0, max: 50, step: 1, unit: "%", default: ACCENT_PERCENT },
  { key: "speedPercent", label: "Speed", type: "range", min: 25, max: 300, step: 5, unit: "%", default: SPEED_PERCENT },
  { key: "trailsPercent", label: "Trails", type: "range", min: 0, max: 95, step: 1, unit: "%", default: TRAILS_PERCENT },
  { key: "clock", label: "Show the clock", type: "toggle", default: true },
  { key: "use24Hour", label: "24-hour time", type: "toggle", default: false },
  {
    key: "label",
    label: "Under the clock",
    type: "choice",
    options: [
      ["none", "Nothing"],
      ["name", "Your name"],
      ["username", "Your username"],
      ["custom", "Custom text"],
    ],
    default: "none",
  },
  { key: "labelText", label: "Custom text", type: "text", maxLength: LABEL_MAX, default: "", showIf: { label: "custom" } },
];

export const DEFAULTS = Object.fromEntries(SETTINGS.map((s) => [s.key, s.default]));

const COLOR_KEYS = ["background", "stars", "accent"];
const HEX = /^#[0-9a-f]{6}$/i;

// Any saved object → valid settings: unknown keys dropped, wrong types and out-of-range values replaced by defaults
export function cleanSettings(raw) {
  const src = raw && typeof raw === "object" ? raw : {};
  const out = {};
  for (const s of SETTINGS) {
    const v = src[s.key];
    switch (s.type) {
      case "color":
        out[s.key] = typeof v === "string" && HEX.test(v) ? v.toLowerCase() : s.default;
        break;
      case "range":
        out[s.key] = typeof v === "number" && Number.isFinite(v) && v >= s.min && v <= s.max ? v : s.default;
        break;
      case "toggle":
        out[s.key] = typeof v === "boolean" ? v : s.default;
        break;
      case "choice":
        out[s.key] = s.options.some(([id]) => id === v) ? v : s.default;
        break;
      case "text":
      case "password":
        out[s.key] = typeof v === "string" ? v.slice(0, s.maxLength ?? v.length) : s.default;
        break;
      default:
        out[s.key] = s.default;
    }
  }
  // "preset" follows the colors. A saved preset name with no colors picks that preset's colors.
  const named = PRESETS.find((p) => p.name === src.preset);
  if (named && COLOR_KEYS.every((k) => src[k] === undefined)) Object.assign(out, named.colors);
  out.preset = presetOf(out);
  return out;
}

// The preset whose colors match, or "Custom"
export function presetOf(settings) {
  const s = settings || {};
  const match = PRESETS.find((p) => COLOR_KEYS.every((k) => String(s[k] ?? "").toLowerCase() === p.colors[k]));
  return match ? match.name : "Custom";
}

/* ---------- WebAssembly ---------- */

let modulePromise;

function wasmBytes() {
  if (typeof atob === "function") {
    const bin = atob(wasmBase64);
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    return bytes;
  }
  return Uint8Array.from(Buffer.from(wasmBase64, "base64"));
}

function loadModule() {
  modulePromise ??= WebAssembly.compile(wasmBytes());
  return modulePromise;
}

// The core calls these while rendering; `target` is the context being drawn into.
function makeHost(target) {
  const rgba = (r, g, b, a) => `rgba(${Math.round(r * 255)},${Math.round(g * 255)},${Math.round(b * 255)},${a})`;
  return {
    fill(r, g, b, a) {
      const ctx = target.ctx;
      ctx.fillStyle = rgba(r, g, b, a);
      ctx.fillRect(0, 0, target.width, target.height);
    },
    line(x0, y0, x1, y1, width, r, g, b, a) {
      const ctx = target.ctx;
      ctx.strokeStyle = rgba(r, g, b, a);
      ctx.lineWidth = width;
      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      ctx.stroke();
    },
    circle(x, y, radius, r, g, b, a) {
      const ctx = target.ctx;
      ctx.fillStyle = rgba(r, g, b, a);
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.fill();
    },
  };
}

function hexToRgb(hex) {
  const v = parseInt(hex.slice(1), 16);
  return [(v >> 16 & 255) / 255, (v >> 8 & 255) / 255, (v & 255) / 255];
}

/* ---------- clock ---------- */

const FONT = '-apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif';
const RTL = /[֐-ࣿיִ-﷿ﹰ-﻿]/;
const LTR = /[A-Za-zÀ-ÖØ-öø-֏]/;

// Text that starts with a right-to-left letter reads right to left as a whole, as in the native savers.
function startsRtl(text) {
  for (const ch of text) {
    if (RTL.test(ch)) return true;
    if (LTR.test(ch)) return false;
  }
  return false;
}

function formatTime(date, use24Hour) {
  const h = date.getHours();
  const mm = String(date.getMinutes()).padStart(2, "0");
  if (use24Hour) return `${String(h).padStart(2, "0")}:${mm}`;
  return `${h % 12 || 12}:${mm} ${h < 12 ? "AM" : "PM"}`;
}

function labelText(settings, userName) {
  let text = "";
  if (settings.label === "custom") text = settings.labelText;
  else if (settings.label === "name" || settings.label === "username") text = userName || "";
  text = String(text).replace(/[\r\n\t]+/g, " ").trim();
  return [...text].slice(0, LABEL_MAX).join("");
}

// The descent of a line of text, for placing it by its bottom edge like AppKit's draw(at:).
function descent(ctx, size) {
  const m = ctx.measureText("Mg");
  return m.fontBoundingBoxDescent ?? size * 0.24;
}

function lineHeight(ctx, size) {
  const m = ctx.measureText("Mg");
  return m.fontBoundingBoxAscent != null ? m.fontBoundingBoxAscent + m.fontBoundingBoxDescent : size * 1.2;
}

// Bottom-left: the time in large type, with the optional label under it (StarfieldView.drawClock).
function drawClock(ctx, width, height, settings, compact, userName, date) {
  const label = labelText(settings, userName);
  if (!settings.clock && !label) return;
  const scale = compact ? Math.max(0.25, height / 900) : 1;
  const margin = 32 * scale;
  const ink = hexToRgb(settings.stars).map((v) => Math.round(v * 255)).join(",");
  let bottom = height - margin;

  ctx.save();
  ctx.textAlign = "left";
  ctx.textBaseline = "alphabetic";
  if (label) {
    const size = 14 * scale;
    ctx.font = `400 ${size}px ${FONT}`;
    ctx.direction = startsRtl(label) ? "rtl" : "ltr";
    ctx.fillStyle = `rgba(${ink},0.5)`;
    ctx.fillText(label, margin, bottom - descent(ctx, size));
    bottom -= lineHeight(ctx, size) - 6 * scale;
  }
  if (settings.clock) {
    const size = 48 * scale;
    ctx.direction = "ltr";
    ctx.font = `600 ${size}px ${FONT}`;
    if ("letterSpacing" in ctx) ctx.letterSpacing = `${-1 * scale}px`;
    if ("fontVariantNumeric" in ctx) ctx.fontVariantNumeric = "tabular-nums";
    ctx.fillStyle = `rgba(${ink},0.8)`;
    ctx.fillText(formatTime(date, settings.use24Hour), margin, bottom - descent(ctx, size));
  }
  ctx.restore();
}

/* ---------- the saver ---------- */

function makeCanvas(w, h, like) {
  if (typeof OffscreenCanvas !== "undefined") return new OffscreenCanvas(w, h);
  const doc = like?.ownerDocument ?? globalThis.document;
  const c = doc.createElement("canvas");
  c.width = w;
  c.height = h;
  return c;
}

/* Run the saver in a canvas. The canvas is sized by the page (CSS); the saver follows its size and devicePixelRatio
   (ResizeObserver), keeps its pixels between frames for the trails, and pauses while the page is hidden.
   options:
     settings       – as from cleanSettings (missing keys use DEFAULTS)
     compact        – a small preview (sf_options.compact)
     reducedMotion  – slower flight
     userName       – the text for "Your name" and "Your username" (a page has no system user)
     now            – () => Date, for the clock (default: () => new Date())
   Returns { update(settings), destroy() }. */
export function createSaver(canvas, options = {}) {
  const view = canvas.getContext("2d");
  let settings = { ...DEFAULTS, ...cleanSettings({ ...DEFAULTS, ...(options.settings || {}) }) };
  const compact = !!options.compact;
  const reducedMotion = !!options.reducedMotion;
  const userName = options.userName ?? "";
  const now = options.now ?? (() => new Date());

  // The stars are drawn into `trail`, which keeps last frame's pixels; each frame is copied to the
  // canvas with the clock on top, so the clock never smears.
  const target = { ctx: null, width: 0, height: 0 };
  let trail = null;
  let exports = null;
  let sf = 0;
  let width = 0, height = 0, dpr = 1; // points, and pixels per point
  let frame = 0, last = 0, destroyed = false, needsResize = true;

  const applyOptions = () => {
    if (!exports || !sf) return;
    const [br, bg, bb] = hexToRgb(settings.background);
    const [sr, sg, sb] = hexToRgb(settings.stars);
    const [ar, ag, ab] = hexToRgb(settings.accent);
    exports.set_options(sf, br, bg, bb, sr, sg, sb, ar, ag, ab, settings.accentPercent / 100, settings.speedPercent / 100,
      settings.trailsPercent / 100, reducedMotion ? 1 : 0, compact ? 1 : 0);
  };

  const clearTrail = () => {
    if (!trail) return;
    const t = trail.getContext("2d");
    t.save();
    t.setTransform(1, 0, 0, 1, 0, 0);
    t.fillStyle = settings.background;
    t.fillRect(0, 0, trail.width, trail.height);
    t.restore();
  };

  const resize = () => {
    needsResize = false;
    const rect = canvas.getBoundingClientRect ? canvas.getBoundingClientRect() : { width: canvas.width, height: canvas.height };
    dpr = Math.min(globalThis.devicePixelRatio || 1, 3);
    width = Math.max(1, rect.width || canvas.width || 1);
    height = Math.max(1, rect.height || canvas.height || 1);
    const pw = Math.max(1, Math.round(width * dpr)), ph = Math.max(1, Math.round(height * dpr));
    canvas.width = pw;
    canvas.height = ph;
    trail = makeCanvas(pw, ph, canvas);
    const t = trail.getContext("2d");
    t.setTransform(dpr, 0, 0, dpr, 0, 0);
    t.lineCap = "round";
    target.ctx = t;
    target.width = width;
    target.height = height;
    if (exports && sf) {
      exports.resize(sf, width, height);
      exports.take_needs_clear(sf);
    }
    clearTrail();
  };

  const tick = (time) => {
    frame = 0;
    if (destroyed) return;
    if (needsResize) resize();
    const dt = last ? Math.min(0.1, Math.max(0, (time - last) / 1000)) : 0;
    last = time;
    if (exports && sf) {
      exports.step(sf, dt);
      if (exports.take_needs_clear(sf)) clearTrail();
      exports.render(sf);
    }
    view.setTransform(1, 0, 0, 1, 0, 0);
    view.drawImage(trail, 0, 0);
    view.setTransform(dpr, 0, 0, dpr, 0, 0);
    drawClock(view, width, height, settings, compact, userName, now());
    schedule();
  };

  const hidden = () => typeof document !== "undefined" && document.hidden;
  const schedule = () => {
    if (!destroyed && !frame && !hidden()) frame = requestAnimationFrame(tick);
  };
  const onVisibility = () => {
    if (hidden()) {
      if (frame) cancelAnimationFrame(frame);
      frame = 0;
    } else {
      last = 0; // don't jump ahead by the time spent hidden
      schedule();
    }
  };

  const observer = typeof ResizeObserver !== "undefined" ? new ResizeObserver(() => { needsResize = true; }) : null;
  observer?.observe(canvas);
  if (typeof document !== "undefined") document.addEventListener("visibilitychange", onVisibility);

  resize();
  loadModule()
    .then((mod) => WebAssembly.instantiate(mod, { host: makeHost(target) }))
    .then((instance) => {
      if (destroyed) return;
      exports = instance.exports;
      exports._initialize?.();
      sf = exports.create((Math.random() * 0xffffffff) >>> 0 || 1);
      applyOptions();
      exports.resize(sf, width, height);
      exports.take_needs_clear(sf);
      clearTrail();
    })
    .catch((err) => console.error("Starfield: couldn't start the WebAssembly core", err));
  schedule();

  return {
    update(next) {
      const before = settings.background;
      const merged = { ...settings, ...(next || {}) };
      // Picking a preset sets its colors, unless the same update sets colors of its own.
      const preset = PRESETS.find((p) => p.name === next?.preset);
      if (preset && COLOR_KEYS.every((k) => next[k] === undefined)) Object.assign(merged, preset.colors);
      settings = { ...DEFAULTS, ...cleanSettings(merged) };
      applyOptions();
      if (settings.background !== before) clearTrail();
    },
    destroy() {
      destroyed = true;
      if (frame) cancelAnimationFrame(frame);
      frame = 0;
      observer?.disconnect();
      if (typeof document !== "undefined") document.removeEventListener("visibilitychange", onVisibility);
      if (exports && sf) exports.destroy(sf);
      sf = 0;
    },
  };
}
