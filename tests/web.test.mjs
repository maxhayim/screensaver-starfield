// Web version tests: node tests/web.test.mjs
// Runs the saver against a stub canvas whose 2D context counts its calls.
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

function stubContext(counts) {
  const ctx = {
    fillStyle: "", strokeStyle: "", lineWidth: 1, lineCap: "butt", font: "", textAlign: "start", textBaseline: "alphabetic",
    direction: "ltr", letterSpacing: "0px",
    texts: [],
    save() {}, restore() {}, setTransform() {}, beginPath() {}, moveTo() {}, lineTo() {}, arc() {},
    fillRect() { counts.fillRect++; },
    stroke() { counts.stroke++; },
    fill() { counts.fill++; },
    drawImage() { counts.drawImage++; },
    fillText(text) { counts.fillText++; ctx.texts.push({ text, direction: ctx.direction, font: ctx.font }); },
    measureText() { return { fontBoundingBoxAscent: 10, fontBoundingBoxDescent: 3 }; },
  };
  return ctx;
}

function stubCanvas(counts, w = 800, h = 500) {
  const ctx = stubContext(counts);
  return { width: w, height: h, ctx, getContext: () => ctx, getBoundingClientRect: () => ({ width: w, height: h }) };
}

// Browser pieces the saver uses.
const trailCounts = { fillRect: 0, stroke: 0, fill: 0, drawImage: 0, fillText: 0 };
globalThis.OffscreenCanvas = class {
  constructor(w, h) { Object.assign(this, stubCanvas(trailCounts, w, h)); }
};
let queued = [];
globalThis.requestAnimationFrame = (cb) => (queued.push(cb), queued.length);
globalThis.cancelAnimationFrame = () => {};
const runFrames = async (n) => {
  let t = 1000;
  for (let i = 0; i < n; i++) {
    await new Promise((r) => setTimeout(r, 0)); // let the WebAssembly module load
    const cbs = queued;
    queued = [];
    t += 1000 / 60;
    cbs.forEach((cb) => cb(t));
  }
};

const web = await import("../web/index.js");
const { ID, NAME, SETTINGS, DEFAULTS, cleanSettings, presetOf, createSaver } = web;

// Identity and the shared interface.
assert.equal(ID, "starfield");
assert.equal(NAME, "Starfield");
assert.ok(Array.isArray(SETTINGS) && SETTINGS.every((s) => s.key && s.label && s.type && "default" in s));
assert.deepEqual(Object.keys(DEFAULTS), SETTINGS.map((s) => s.key));

// Every preset in core/presets.h is offered.
const header = readFileSync(new URL("../core/presets.h", import.meta.url), "utf8");
const names = [...header.matchAll(/\{\s*"([^"]+)",\s*"#/g)].map((m) => m[1]);
const offered = SETTINGS.find((s) => s.type === "preset").presets;
assert.ok(names.length >= 7);
assert.deepEqual(offered.map((p) => p.name), names);
for (const p of offered) assert.equal(presetOf(p.colors), p.name);

// Defaults match the native savers.
assert.equal(DEFAULTS.preset, "Original");
assert.equal(DEFAULTS.background, "#0b0b0a");
assert.equal(DEFAULTS.accentPercent, 7);
assert.equal(DEFAULTS.speedPercent, 100);
assert.equal(DEFAULTS.trailsPercent, 58);
assert.equal(DEFAULTS.clock, true);
assert.equal(DEFAULTS.label, "none");

// cleanSettings fixes bad input.
const cleaned = cleanSettings({
  background: "red", stars: "#ABCDEF", accentPercent: 400, speedPercent: "fast", trailsPercent: 30,
  clock: "yes", use24Hour: true, label: "everything", labelText: "x".repeat(200), bogus: 1,
});
assert.equal(cleaned.background, "#0b0b0a");
assert.equal(cleaned.stars, "#abcdef");
assert.equal(cleaned.accentPercent, 7);
assert.equal(cleaned.speedPercent, 100);
assert.equal(cleaned.trailsPercent, 30);
assert.equal(cleaned.clock, true);
assert.equal(cleaned.use24Hour, true);
assert.equal(cleaned.label, "none");
assert.equal(cleaned.labelText.length, 80);
assert.equal(cleaned.preset, "Custom");
assert.ok(!("bogus" in cleaned));
assert.deepEqual(cleanSettings(null), DEFAULTS);
assert.equal(cleanSettings({ preset: "Synthwave" }).background, "#12041f");

// Stars are drawn, frame after frame, with the clock and label on top.
const counts = { fillRect: 0, stroke: 0, fill: 0, drawImage: 0, fillText: 0 };
const canvas = stubCanvas(counts);
const saver = createSaver(canvas, {
  settings: { label: "custom", labelText: "מקס חיים · Office" },
  now: () => new Date(2026, 9, 6, 19, 5),
});
await runFrames(30);
assert.ok(trailCounts.fillRect > 20, "the afterglow fill runs every frame");
assert.ok(trailCounts.stroke + trailCounts.fill > 1000, `stars drawn (${trailCounts.stroke} streaks, ${trailCounts.fill} dots)`);
assert.ok(counts.drawImage >= 29, "each frame is copied to the canvas");
const texts = canvas.ctx.texts.slice(-2).map((t) => t.text);
assert.deepEqual(texts, ["מקס חיים · Office", "7:05 PM"]);
assert.equal(canvas.ctx.texts.at(-2).direction, "rtl");

// update: a preset sets its colors; 24-hour time; the label off.
saver.update({ preset: "Amber terminal", use24Hour: true, label: "none" });
canvas.ctx.texts.length = 0;
await runFrames(2);
assert.deepEqual(canvas.ctx.texts.map((t) => t.text).slice(-1), ["19:05"]);
assert.ok(canvas.ctx.texts.every((t) => t.text !== "מקס חיים · Office"));
saver.destroy();
const before = counts.drawImage;
await runFrames(3);
assert.equal(counts.drawImage, before, "nothing draws after destroy");

console.log("all web tests passed");
