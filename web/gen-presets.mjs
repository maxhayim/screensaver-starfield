// Writes web/presets.js from core/presets.h, so the web version lists the
// same presets and defaults as the native savers. Run by web/build.sh.
import { readFileSync, writeFileSync } from "node:fs";

const header = readFileSync(new URL("../core/presets.h", import.meta.url), "utf8");
const presets = [...header.matchAll(/\{\s*"([^"]+)",\s*"(#[0-9a-fA-F]{6})",\s*"(#[0-9a-fA-F]{6})",\s*"(#[0-9a-fA-F]{6})"\s*\}/g)].map(
  ([, name, background, stars, accent]) => ({ name, colors: { background, stars, accent } }),
);
const define = (name) => {
  const m = header.match(new RegExp(`#define ${name} (\\d+)`));
  if (!m) throw new Error(`${name} is missing from core/presets.h`);
  return Number(m[1]);
};
if (presets.length === 0) throw new Error("no presets found in core/presets.h");

const out = `// Generated from core/presets.h by web/gen-presets.mjs. Don't edit.
export const PRESETS = ${JSON.stringify(presets, null, 2)};

export const ACCENT_PERCENT = ${define("SF_DEFAULT_ACCENT_PERCENT")};
export const SPEED_PERCENT = ${define("SF_DEFAULT_SPEED_PERCENT")};
export const TRAILS_PERCENT = ${define("SF_DEFAULT_TRAILS_PERCENT")};
export const LABEL_MAX = ${define("SF_LABEL_MAX")};
`;
writeFileSync(new URL("./presets.js", import.meta.url), out);
