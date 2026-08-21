// scripts/thumb-web.js -- generic "best shot" thumbnail capturer for a
// web page that animates (this project's hack pages, rss-sdl2-gles2's
// saver pages, any canvas demo).
//
// Loads the URL in headless Chromium, screenshots it every --interval
// ms for --duration ms, scores each shot by colorfulness, and keeps
// only the running winner, which is written to <out.png>. Score =
// distinct quantized colors (5 bits/channel), tie-broken by non-black
// coverage -- "the most color variation" wins, so slow-starting or
// flashing pages get a representative thumbnail instead of a black one.
//
// Ends early once the winner is "good enough" (--good-colors distinct
// colors AND --good-coverage non-black fraction) -- no point sampling
// the full duration for a page that's already showing plenty.
//
// Usage: node scripts/thumb-web.js <url> <out.png>
//                                  [--interval 10000] [--duration 180000]
//                                  [--good-colors 200] [--good-coverage 0.05]
//                                  [--warmup 5000]
// Chrome is located via $CHROME_BIN, then common install paths.
// Requires puppeteer-core + pngjs resolvable from ./node_modules or
// tests/node_modules (npm ci in tests/).

'use strict';

function req(name) {
  const paths = [name, __dirname + '/../tests/node_modules/' + name];
  for (const p of paths) { try { return require(p); } catch (e) {} }
  throw new Error(name + ' not found (npm ci in tests/)');
}
const puppeteer = req('puppeteer-core');
const { PNG } = req('pngjs');
const fs = require('fs');
const cp = require('child_process');

function findChrome() {
  if (process.env.CHROME_BIN) return process.env.CHROME_BIN;
  const candidates = [
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    '/usr/bin/google-chrome', '/usr/bin/google-chrome-stable',
    '/usr/bin/chromium', '/usr/bin/chromium-browser',
  ];
  for (const c of candidates) { try { fs.accessSync(c); return c; } catch (e) {} }
  for (const n of ['google-chrome', 'chromium', 'chromium-browser']) {
    try { return cp.execSync('command -v ' + n, { stdio: ['ignore', 'pipe', 'ignore'] })
      .toString().trim(); } catch (e) {}
  }
  throw new Error('no Chrome found (set CHROME_BIN)');
}

function score(buf) {
  const png = PNG.sync.read(buf);
  const { width, height, data } = png;
  const colors = new Set();
  let nonBlack = 0;
  for (let i = 0; i < data.length; i += 4) {
    const r = data[i], g = data[i + 1], b = data[i + 2];
    if (r > 8 || g > 8 || b > 8) nonBlack++;
    colors.add((r >> 3) + ',' + (g >> 3) + ',' + (b >> 3));
  }
  return { colors: colors.size, nonBlackFrac: nonBlack / (width * height) };
}

function better(a, b) {          // is a a better shot than b?
  if (!b) return true;
  if (a.colors !== b.colors) return a.colors > b.colors;
  return a.nonBlackFrac > b.nonBlackFrac;
}

(async () => {
  const args = process.argv.slice(2);
  const url = args[0], out = args[1];
  if (!url || !out) {
    console.error('usage: thumb-web.js <url> <out.png> [--interval ms] [--duration ms]');
    process.exit(2);
  }
  const opt = (name, dflt) => {
    const i = args.indexOf(name);
    return i >= 0 ? parseInt(args[i + 1], 10) : dflt;
  };
  const interval = opt('--interval', 10000);
  const duration = opt('--duration', 180000);
  const warmup = opt('--warmup', 5000);   // let load veils fade first
  const goodColors = opt('--good-colors', 200);
  const goodCoverage = (() => {
    const i = args.indexOf('--good-coverage');
    return i >= 0 ? parseFloat(args[i + 1]) : 0.05;
  })();

  const browser = await puppeteer.launch({
    executablePath: findChrome(),
    headless: 'new',
    args: ['--no-sandbox', '--disable-dev-shm-usage',
           '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
           '--window-size=800,600', '--hide-scrollbars'],
  });
  try {
    const page = await browser.newPage();
    await page.setViewport({ width: 800, height: 600 });
    await page.goto(url, { waitUntil: 'load', timeout: 45000 });
    await new Promise(r => setTimeout(r, warmup));

    let best = null, bestScore = null, samples = 0;
    const t0 = Date.now();
    while (Date.now() - t0 < duration) {
      await new Promise(r => setTimeout(r, interval));
      const shot = await page.screenshot({ type: 'png' });
      const s = score(shot);
      samples++;
      if (better(s, bestScore)) { best = shot; bestScore = s; }
      console.error(`sample ${samples}: colors=${s.colors} ` +
                    `nonBlack=${s.nonBlackFrac.toFixed(3)}` +
                    (bestScore === s ? '  <- new best' : ''));
      if (bestScore.colors >= goodColors &&
          bestScore.nonBlackFrac >= goodCoverage) {
        console.error('good enough, stopping early');
        break;
      }
    }
    if (!best) throw new Error('no samples captured');
    fs.writeFileSync(out, best);
    console.log(JSON.stringify({ out, samples, best: bestScore }));
  } finally {
    await browser.close();
  }
})().catch(e => { console.error(e.message || e); process.exit(1); });
