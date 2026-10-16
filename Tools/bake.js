// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// bake.js : renders Tools/bake.html in headless Chrome and writes the Xbox assets.
//
//   node Tools/bake.js
//
// Output (big-endian, read by Assets.cpp):
//   Media/<name>.tex : 'OTX1', u32 levels, then per level u32 w, u32 h, w*h*4 bytes
//                      (A,R,G,B; premultiplied for art, R=glyph G=glow for fonts)
//   Media/<name>.fnt : 'OFN1', f32 size, f32 ascent, u32 glyph count, then per glyph
//                      u32 code, s16 x, y, w, h, ox, oy, f32 advance
//   Media/<name>.inf : 'OSP1', f32 boxW, f32 boxH, f32 pad   (sprite layout box)

'use strict';
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '..');
const media = path.join(root, 'Media');
const page = 'file:///' + path.join(__dirname, 'bake.html').replace(/\\/g, '/');

const chromes = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
];
const chrome = chromes.find(p => fs.existsSync(p));
if (!chrome) throw new Error('Chrome or Edge not found');

const dom = execFileSync(chrome, [
  '--headless=new', '--disable-gpu', '--allow-file-access-from-files',
  '--virtual-time-budget=20000', '--dump-dom', page,
], { maxBuffer: 512 * 1024 * 1024, encoding: 'utf8' });

const m = dom.match(/<pre id="out">([\s\S]*?)<\/pre>/);
if (!m) throw new Error('no output block in page');
const text = m[1].replace(/&quot;/g, '"').replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&');
if (!text.startsWith('{')) throw new Error('bake page said: ' + text.slice(0, 200));
const assets = JSON.parse(text);

fs.mkdirSync(media, { recursive: true });

function u32(n) { const b = Buffer.alloc(4); b.writeUInt32BE(n >>> 0); return b; }
function f32(n) { const b = Buffer.alloc(4); b.writeFloatBE(n); return b; }
function s16(n) { const b = Buffer.alloc(2); b.writeInt16BE(n); return b; }

function writeTex(name, tex) {
  const parts = [Buffer.from('OTX1'), u32(tex.levels.length)];
  for (const lv of tex.levels) {
    const data = Buffer.from(lv.data, 'base64');
    if (data.length !== lv.w * lv.h * 4) throw new Error(name + ': bad level size');
    parts.push(u32(lv.w), u32(lv.h), data);
  }
  fs.writeFileSync(path.join(media, name + '.tex'), Buffer.concat(parts));
}

for (const [name, tex] of Object.entries(assets.textures)) {
  writeTex(name, tex);
  if (tex.boxW !== undefined)
    fs.writeFileSync(path.join(media, name + '.inf'), Buffer.concat([Buffer.from('OSP1'), f32(tex.boxW), f32(tex.boxH), f32(tex.pad)]));
}

for (const [name, font] of Object.entries(assets.fonts)) {
  writeTex('font_' + name, font.tex);
  const parts = [Buffer.from('OFN1'), f32(font.size), f32(font.ascent), u32(font.glyphs.length)];
  for (const g of font.glyphs)
    parts.push(u32(g.code), s16(g.x), s16(g.y), s16(g.w), s16(g.h), s16(g.ox), s16(g.oy), f32(g.adv));
  fs.writeFileSync(path.join(media, 'font_' + name + '.fnt'), Buffer.concat(parts));
}

for (const f of fs.readdirSync(media)) console.log(f.padEnd(20), fs.statSync(path.join(media, f)).size);
