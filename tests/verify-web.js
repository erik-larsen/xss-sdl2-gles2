// tests/verify-web.js -- portable headless-browser verifier for the
// emscripten build, used by tests/smoke-web.sh both in CI and locally.
//
// Loads a hack page in headless Chromium (SwiftShader WebGL), samples the
// canvas twice via compositor screenshots, and emits a single-line JSON
// verdict consumed by smoke-web.sh:
//   {"nonBlank":<bool>,"nonBlackFrac":..,"maxLum":..,"colors":..,"delta":..}
// "nonBlank" cleanly separates a rendered frame (maxLum 255) from an
// opaque-black canvas (maxLum 0, nonBlackFrac 0).
//
// Requires puppeteer-core + pngjs resolvable from this directory
// (tests/node_modules -- `npm ci` in tests/). Chrome is located via, in
// order: $CHROME_BIN, the Playwright sandbox path, then PATH names.
//
// Usage: node tests/verify-web.js <url> <outPng> [waitMs]
const puppeteer = require('puppeteer-core');
const { PNG } = require('pngjs');
const fs = require('fs');
const cp = require('child_process');

function findChrome() {
  if (process.env.CHROME_BIN) return process.env.CHROME_BIN;
  const candidates = [
    '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
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

function stats(buf) {
  const png = PNG.sync.read(buf);
  const { width, height, data } = png;
  let nonBlack = 0, maxL = 0;
  const colors = new Set();
  const n = width * height;
  for (let i = 0; i < data.length; i += 4) {
    const r = data[i], g = data[i + 1], b = data[i + 2];
    const l = Math.max(r, g, b);
    if (l > maxL) maxL = l;
    if (r > 8 || g > 8 || b > 8) nonBlack++;
    colors.add((r >> 3) + ',' + (g >> 3) + ',' + (b >> 3));
  }
  return { nonBlackFrac: nonBlack / n, maxLum: maxL, colors: colors.size };
}
function delta(a, b) {
  const pa = PNG.sync.read(a), pb = PNG.sync.read(b);
  if (pa.width !== pb.width) return -1;
  let d = 0;
  for (let i = 0; i < pa.data.length; i += 4)
    if (Math.abs(pa.data[i] - pb.data[i]) > 12 ||
        Math.abs(pa.data[i + 1] - pb.data[i + 1]) > 12 ||
        Math.abs(pa.data[i + 2] - pb.data[i + 2]) > 12) d++;
  return d / (pa.width * pa.height);
}

(async () => {
  const url = process.argv[2];
  const outPng = process.argv[3] || '/tmp/verifyweb.png';
  const waitMs = parseInt(process.argv[4] || '4000', 10);
  let verdict = { nonBlank: false };
  let browser;
  try {
    browser = await puppeteer.launch({
      executablePath: findChrome(), headless: 'new',
      args: ['--no-sandbox', '--use-gl=angle', '--use-angle=swiftshader',
             '--enable-unsafe-swiftshader', '--disable-dev-shm-usage'],
    });
    const page = await browser.newPage();
    await page.setViewport({ width: 800, height: 600 });
    await page.goto(url, { waitUntil: 'load', timeout: 30000 });
    await page.waitForFunction(
      () => window.Module && (window.Module.calledRun || window.Module.asm),
      { timeout: 30000 }).catch(() => {});
    await new Promise(r => setTimeout(r, waitMs));
    const c = await page.$('#canvas');
    const A = await c.screenshot();
    await new Promise(r => setTimeout(r, 700));
    const B = await c.screenshot();
    fs.writeFileSync(outPng, B);
    const sb = stats(B);
    verdict = {
      nonBlank: (sb.maxLum > 16 && sb.nonBlackFrac > 0.0003),
      nonBlackFrac: +sb.nonBlackFrac.toFixed(4),
      maxLum: sb.maxLum, colors: sb.colors, delta: +delta(A, B).toFixed(4),
    };
  } catch (e) {
    verdict = { nonBlank: false, error: String(e) };
  } finally {
    if (browser) await browser.close();
  }
  console.log(JSON.stringify(verdict));
})();
