// Smoke test for the browser build in headless Chromium (Playwright).
//
//   node web/smoke/smoke.cjs <url> <out-dir> [--phone]
//
// Plays the whole potion book through the page's own buttons and keys, the
// way a player does:
// - the page starts with no errors and shows How to play on a first visit;
//   the canvas draws the cottage (not one flat colour);
// - recipe 1 brewed wrong (a Gummy Newt) makes a frog; Pour out empties the
//   pot; then a tap on a jar in the cottage itself drops it in, the card
//   ticks it off, and stirring the right brew makes the potion;
// - every later recipe is read off its card and brewed (keys on a desktop,
//   taps on the phone profile); each riddle is first stirred wrong twice, then
//   "Peek at the answer" shows its steps, which are brewed;
// - the finished book shows its dialog with the stars;
// - on the phone profile no two controls overlap, all are on screen, and the
//   card, the verdict and the shelf stay clear of each other;
// - the Sound button turns the sound off and a reload keeps it off.
// Writes PNGs to <out-dir>.
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { chromium, devices } = require('playwright');

const [url, outDir] = process.argv.slice(2);
const phone = process.argv.includes('--phone');
if (!url || !outDir) {
  console.error('usage: smoke.cjs <url> <out-dir> [--phone]');
  process.exit(2);
}
fs.mkdirSync(outDir, { recursive: true });

// Minimal PNG decoder (8-bit RGB/RGBA, non-interlaced: what Chromium writes).
function decodePng(buf) {
  let pos = 8, width = 0, height = 0, channels = 0;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos);
    const type = buf.toString('ascii', pos + 4, pos + 8);
    const data = buf.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      channels = { 2: 3, 6: 4 }[data[9]];
      if (data[8] !== 8 || !channels || data[12] !== 0) throw new Error('unsupported PNG');
    } else if (type === 'IDAT') idat.push(data);
    pos += 12 + len;
  }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = width * channels;
  const out = Buffer.alloc(height * stride);
  for (let y = 0; y < height; y++) {
    const f = raw[y * (stride + 1)];
    for (let x = 0; x < stride; x++) {
      const v = raw[y * (stride + 1) + 1 + x];
      const a = x >= channels ? out[y * stride + x - channels] : 0;
      const b = y > 0 ? out[(y - 1) * stride + x] : 0;
      const c = x >= channels && y > 0 ? out[(y - 1) * stride + x - channels] : 0;
      let p;
      if (f === 0) p = 0;
      else if (f === 1) p = a;
      else if (f === 2) p = b;
      else if (f === 3) p = (a + b) >> 1;
      else {
        const pa = Math.abs(b - c), pb = Math.abs(a - c), pc = Math.abs(a + b - 2 * c);
        p = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      out[y * stride + x] = (v + p) & 255;
    }
  }
  return { width, height, channels, data: out };
}

// Distinct colours (quantised to 4 bits per channel) in a PNG.
function colourCount(png) {
  const img = decodePng(png);
  const seen = new Set();
  for (let i = 0; i < img.data.length; i += img.channels)
    seen.add(((img.data[i] >> 4) << 8) | ((img.data[i + 1] >> 4) << 4) | (img.data[i + 2] >> 4));
  return seen.size;
}

(async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const base = phone ? devices['iPhone 13'] : { viewport: { width: 1280, height: 800 } };
  // "headless" in the UA keeps arcade from counting a smoke run as a play.
  const context = await browser.newContext({ ...base, userAgent: (base.userAgent || 'Mozilla/5.0') + ' cauldron-smoke headless' });
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  const fail = async (why) => {
    console.error('FAIL: ' + why);
    await page.screenshot({ path: path.join(outDir, 'failure.png') }).catch(() => {});
    await browser.close();
    process.exit(1);
  };
  const open = async (reload) => {
    if (reload) await page.reload({ waitUntil: 'commit', timeout: 120000 });
    else await page.goto(url, { waitUntil: 'commit' });
    try {
      await page.waitForFunction(() => window.cauldron && window.cauldronStatus, null, { timeout: 90000 });
    } catch (e) {
      await fail('the game never became ready' + (reload ? ' after a reload' : '') + ': ' + errors.join(' | '));
    }
  };
  const status = async () => JSON.parse(await page.evaluate(() => window.cauldron.status()));
  const until = async (pred, ms = 30000) => {
    const end = Date.now() + ms;
    let st = await status();
    while (!pred(st) && Date.now() < end) {
      await page.waitForTimeout(150);
      st = await status();
    }
    return st;
  };
  const shot = async (name) => page.screenshot({ path: path.join(outDir, name + '.png') });
  // A tap on the phone, a click on a desktop.
  const hit = async (sel) => {
    const l = page.locator(sel).first();
    if (phone) await l.tap(); else await l.click();
  };

  // On a phone, every visible control is on screen and none overlaps another;
  // the card and the verdict don't run into each other or the shelf.
  const checkLayout = async (when) => {
    if (!phone) return;
    const boxes = await page.evaluate(() => {
      const out = [];
      const add = (el, id) => {
        const r = el.getBoundingClientRect();
        if (r.width > 0 && r.height > 0 && getComputedStyle(el).display !== 'none' && el.offsetParent !== null)
          out.push({ id, x: r.left, y: r.top, w: r.width, h: r.height });
      };
      document.querySelectorAll('#top button, #tray button, #toast button').forEach((b, i) => {
        if (!b.hidden) add(b, b.id || b.getAttribute('aria-label') || 'button ' + i);
      });
      add(document.getElementById('card'), 'card');
      const t = document.getElementById('toast');
      if (t.classList.contains('show')) add(t, 'toast');
      return { out, vw: innerWidth, vh: innerHeight };
    });
    const inside = (a, b) => a.x >= b.x - 0.5 && a.y >= b.y - 0.5 && a.x + a.w <= b.x + b.w + 0.5 && a.y + a.h <= b.y + b.h + 0.5;
    for (const a of boxes.out) {
      if (a.x < -0.5 || a.y < -0.5 || a.x + a.w > boxes.vw + 0.5 || a.y + a.h > boxes.vh + 0.5) await fail(`${when}: ${a.id} is off screen`);
      for (const b of boxes.out) {
        if (a === b || inside(a, b) || inside(b, a)) continue;  // the toast's own buttons sit in it
        if (a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h) await fail(`${when}: ${a.id} overlaps ${b.id}`);
      }
      if (/^(Add|stir|pour|next|again|peek)/i.test(a.id) && (a.w < 43.5 || a.h < 43.5)) await fail(`${when}: ${a.id} is smaller than a finger (${a.w}x${a.h})`);
    }
    console.log(`ok: ${boxes.out.length} controls on screen, none overlapping (${when})`);
  };

  await open(false);
  if (!(await page.locator('#help').isVisible())) await fail('How to play was not shown on a first visit');
  await shot('help');
  await hit('#helpClose');
  await page.waitForTimeout(800);
  const canvasShot = await page.locator('#canvas').screenshot();
  fs.writeFileSync(path.join(outDir, 'cottage-canvas.png'), canvasShot);
  const colours = colourCount(canvasShot);
  console.log(`the cottage draws ${colours} colours`);
  if (colours < 40) await fail(`the canvas looks blank (${colours} colours)`);
  await shot('start');
  await checkLayout('start');

  const ingredients = JSON.parse(await page.evaluate(() => window.cauldron.ingredients()));
  const named = (text) => {
    const t = text.replace(/^then /, '').replace(/^\d+ /, '');
    const i = ingredients.findIndex((it) => t.slice(0, 6) === it.name.slice(0, 6));
    if (i < 0) throw new Error('no ingredient for "' + text + '"');
    return i;
  };
  const stepsOf = (lines) => {
    const out = [];
    for (const l of lines) {
      const n = parseInt(l.replace(/^then /, ''), 10) || 1;
      for (let k = 0; k < n; k++) out.push(named(l));
    }
    return out;
  };
  const addOne = async (i) => {
    const before = (await status()).pot.length;
    if (phone) await hit(`#shelf button[data-i="${i}"]`);
    else await page.keyboard.press(String(i + 1));
    const s = await until((st) => st.pot.length === before + 1, 5000);
    if (s.pot.length !== before + 1) await fail(`adding ingredient ${i} did nothing`);
  };
  const stirNow = async () => {
    if (phone) await hit('#stirBtn'); else await page.keyboard.press('s');
    const s = await until((st) => st.stirred, 30000);
    if (!s.stirred) await fail('stirring never finished');
    return s;
  };

  // ---- recipe 1, wrong first: a frog.
  let s = await status();
  if (s.recipe !== 0 || s.name !== 'Glow Juice') await fail('the book did not open at Glow Juice: ' + JSON.stringify(s));
  await hit('#shelf button[data-i="0"]');
  s = await until((st) => st.pot.length === 1);
  await hit('#stirBtn');
  s = await until((st) => st.stirred);
  if (s.effect !== 'frog' || s.success) await fail('a Gummy Newt in Glow Juice did not make a frog: ' + s.effect);
  await page.waitForFunction(() => document.getElementById('toast').classList.contains('show'), null, { timeout: 5000 }).catch(() => {});
  await page.waitForTimeout(2200);  // the frog is out of the pot by now
  console.log(`wrong brew: ${s.title} ${s.message}`);
  await shot('frog');
  await checkLayout('a frog');
  await hit('#againBtn');
  s = await until((st) => !st.stirred && st.pot.length === 0);
  if (s.stirred || s.pot.length) await fail('Pour out did not empty the pot');

  // ---- recipe 1, right: the first spoonful by tapping its jar in the cottage.
  const jar = (JSON.parse(await page.evaluate(() => window.cauldron.ingredients())))[1].jar;
  const box = await page.locator('#canvas').boundingBox();
  if (!(jar[0] > 0 && jar[0] < 1 && jar[1] > 0 && jar[1] < 1)) await fail('the Glow Mushroom jar is off screen: ' + jar);
  const jx = box.x + jar[0] * box.width, jy = box.y + jar[1] * box.height;
  if (phone) await page.touchscreen.tap(jx, jy); else await page.mouse.click(jx, jy);
  s = await until((st) => st.pot.length === 1);
  if (s.pot[0] !== 1) await fail('a tap on the Glow Mushroom jar did not drop one in: ' + JSON.stringify(s.pot));
  await addOne(2);
  s = await status();
  if (s.matched !== 2) await fail('the card did not tick both lines: matched ' + s.matched);
  const ticks = await page.locator('#cardLines li.done').count();
  if (ticks !== 2) await fail(`the card shows ${ticks} ticks, not 2`);
  s = await stirNow();
  if (!s.success) await fail('the right Glow Juice failed: ' + s.message);
  await page.waitForTimeout(1200);
  await shot('glow-juice');
  console.log(`right brew: ${s.message}`);
  await hit('#nextBtn');
  s = await until((st) => st.recipe === 1 && !st.stirred);
  if (s.recipe !== 1) await fail('Next recipe did not turn the page');

  // ---- the rest of the book.
  while (!s.done) {
    if (s.riddle) {
      // Two wrong stirs, then the answer may be peeked.
      for (let k = 0; k < 2; k++) {
        await addOne(k === 0 ? 7 : 5);
        await addOne(k === 0 ? 7 : 5);
        await addOne(k === 0 ? 7 : 5);
        s = await stirNow();
        if (s.success) await fail('a wrong riddle brew succeeded');
        if (k === 1 && s.recipe === 5) { await page.waitForTimeout(1500); await shot('riddle-mishap'); }
        if (k === 1) {
          if (!s.canPeek) await fail('no peek after two wrong tries');
          await page.waitForFunction(() => !document.getElementById('peekBtn').hidden, null, { timeout: 5000 }).catch(() => {});
          await hit('#peekBtn');
          s = await until((st) => st.answer.length > 0, 5000);
          if (!s.answer.length) await fail('Peek at the answer showed nothing');
        }
        await hit('#againBtn');
        s = await until((st) => !st.stirred);
      }
      if (s.recipe === 5) await shot('riddle-peeked');
    }
    const steps = stepsOf(s.riddle ? s.answer : s.lines);
    for (const i of steps) await addOne(i);
    s = await stirNow();
    if (!s.success) await fail(`${s.name} brewed from its card failed: ${s.message}`);
    console.log(`made ${s.name} (${steps.length} spoonfuls)`);
    if (s.recipe === 2 || s.recipe === 3) { await page.waitForTimeout(2000); await shot('made-' + s.recipe); }
    if (phone) await hit('#nextBtn'); else await page.keyboard.press('Enter');
    const was = s.recipe;
    s = await until((st) => st.recipe === was + 1);
    if (s.recipe !== was + 1) await fail('the book did not move on from ' + was);
  }
  await page.waitForFunction(() => document.getElementById('done').style.display === 'block', null, { timeout: 10000 }).catch(() => {});
  if (!(await page.locator('#done').isVisible())) await fail('the finished book showed no dialog');
  console.log(`book finished: ${s.stars} of ${s.maxStars} stars`);
  await shot('finished');
  await hit('#doneClose');
  await page.waitForTimeout(1500);
  await shot('finale');

  // ---- sound off, and remembered.
  await hit('#soundBtn');
  if ((await page.locator('#soundBtn').getAttribute('aria-pressed')) !== 'false') await fail('the Sound button did not turn the sound off');
  await open(true);
  if ((await page.locator('#soundBtn').getAttribute('aria-pressed')) !== 'false') await fail('the sound came back on after a reload');
  if (await page.locator('#help').isVisible()) await fail('How to play showed again on a second visit');

  if (errors.length) await fail('page errors: ' + errors.join(' | '));
  console.log('PASS (' + (phone ? 'phone' : 'desktop') + ')');
  await browser.close();
})().catch((e) => { console.error('FAIL: ' + (e && e.stack || e)); process.exit(1); });
