// Panelleri başsız Chrome'da sahte A3 sunucusuna bağlayıp ekran görüntüsü alır
// ve sekme başına bellek/işlemci kullanımını ölçer (W3-web kabul 4 ve 6).
// Chrome DevTools Protokolü Node'un yerleşik WebSocket'iyle konuşulur; ek paket yok.
//
//   npm run derle && node arac/panel-olcum.mjs --ekran <dizin> [--genislik 1280 --yukseklik 720]
//                                              [--olc 30] [--atla 128] [--kip sil|hil_1x]
//   --etkilesim: ekran görüntüsünden önce durum panelinde Duraklat'a, senaryo panelinde hatali.toml + Doğrula'ya basar
//   --sim <açılış adresi>: sahte sunucu yerine çalışan gerçek ida_sim'e bağlanır (http://127.0.0.1:<port>/#erisim=<jeton>)
//   node arac/panel-olcum.mjs --kopma <dizin> --paneller durum   (kopma ve toparlanma görüntüleri)
//
// Chrome yolu CHROME ortam değişkeninden ya da bilinen konumlardan bulunur.
// Ekran görüntüleri depoya girmez (web/.gitignore: ekran/).

import { spawn, execFileSync } from 'node:child_process';
import { randomBytes } from 'node:crypto';
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';

const kok = dirname(dirname(fileURLToPath(import.meta.url)));
const { values: a } = parseArgs({
  options: {
    ekran: { type: 'string' },
    kopma: { type: 'string' },
    genislik: { type: 'string', default: '1280' },
    yukseklik: { type: 'string', default: '720' },
    olcek: { type: 'string', default: '1' },
    bekle: { type: 'string', default: '14' },
    olc: { type: 'string' },
    atla: { type: 'string', default: '0' },
    kip: { type: 'string', default: 'sil' },
    emniyet: { type: 'string', default: 'olagan' },
    elle: { type: 'boolean', default: false },
    etkilesim: { type: 'boolean', default: false },
    sim: { type: 'string' },
    port: { type: 'string', default: '47181' },
    paneller: { type: 'string' },
    onek: { type: 'string', default: '' },
  },
});

const CHROME_ADAYLARI = [
  process.env.CHROME,
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
].filter(Boolean);
const chrome = CHROME_ADAYLARI.find((p) => existsSync(p));
if (!chrome) throw new Error('Chrome bulunamadı; CHROME ortam değişkenini verin');

const bekle = (ms) => new Promise((r) => setTimeout(r, ms));
const dis = a.sim === undefined ? null : /^http:\/\/127\.0\.0\.1:(\d+)\/#erisim=([0-9a-f]{64})$/.exec(a.sim);
if (a.sim !== undefined && dis === null) throw new Error('--sim: açılış adresi http://127.0.0.1:<port>/#erisim=<jeton> biçiminde olmalı');
const jeton = dis === null ? randomBytes(32).toString('hex') : dis[2];
const port = dis === null ? Number(a.port) : Number(dis[1]);

// ---- Sahte sunucu ----
async function sunucuBaslat() {
  if (dis !== null) return null; // dış sim: süreci biz yönetmiyoruz
  const s = spawn(process.execPath, ['.derleme/sahte-a3.mjs', '--port', String(port), '--jeton', jeton, '--statik', 'dist', '--kip', a.kip, '--atla', a.atla, '--emniyet', a.emniyet, ...(a.elle ? ['--elle'] : [])], {
    cwd: kok,
    stdio: ['ignore', 'pipe', 'inherit'],
  });
  await new Promise((coz, red) => {
    s.stdout.on('data', (d) => {
      if (String(d).includes('Açılış adresi')) coz();
    });
    s.once('exit', (k) => red(new Error('sahte sunucu çıktı: ' + k)));
  });
  return s;
}
async function sunucuDurdur(s) {
  if (s === null) return;
  const bitti = new Promise((r) => s.once('exit', r));
  s.kill('SIGTERM');
  await bitti;
}
// Ekran görüntüsünde her panel aynı senaryo anını görsün diye sunucu her panel için yeniden başlatılır.
let sunucu = await sunucuBaslat();

// ---- Chrome ----
const profil = mkdtempSync(join(tmpdir(), 'ida-chrome-'));
const hataAyiklamaPort = (dis === null ? port : 47400) + 1000;
const tarayici = spawn(
  chrome,
  [
    '--headless=new',
    `--remote-debugging-port=${hataAyiklamaPort}`,
    `--user-data-dir=${profil}`,
    '--no-first-run',
    '--no-default-browser-check',
    '--hide-scrollbars',
    '--disable-extensions',
    // yedek renderer süreci olmasın: sekme ↔ süreç eşlemesi için
    '--disable-features=SpareRendererForSitePerProcess',
    `--window-size=${a.genislik},${a.yukseklik}`,
    'about:blank',
  ],
  { stdio: 'ignore' },
);

async function jsonAl(yol) {
  for (let i = 0; i < 50; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${hataAyiklamaPort}${yol}`);
      return await r.json();
    } catch {
      await bekle(200);
    }
  }
  throw new Error('Chrome hata ayıklama ucu açılmadı');
}

const surum = await jsonAl('/json/version');
const ws = new WebSocket(surum.webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r, { once: true }));
let no = 0;
const bekleyen = new Map();
ws.addEventListener('message', (e) => {
  const m = JSON.parse(e.data);
  if (m.id && bekleyen.has(m.id)) {
    const { coz, red } = bekleyen.get(m.id);
    bekleyen.delete(m.id);
    if (m.error) red(new Error(m.error.message));
    else coz(m.result);
  }
});
function cdp(yontem, parametre = {}, oturum) {
  const id = ++no;
  ws.send(JSON.stringify({ id, method: yontem, params: parametre, ...(oturum ? { sessionId: oturum } : {}) }));
  return new Promise((coz, red) => bekleyen.set(id, { coz, red }));
}

/** Bu Chrome profiline ait renderer süreçleri: pid → birikimli işlemci süresi (s). `ps` kullanır (macOS/Linux). */
async function rendererPidleri() {
  const cikti = execFileSync('ps', ['-axo', 'pid=,ppid=,time=,command=']).toString();
  const m = new Map();
  for (const satir of cikti.split('\n')) {
    if (!satir.includes('--type=renderer')) continue;
    const [pid, ppid, zaman] = satir.trim().split(/\s+/);
    if (Number(ppid) !== tarayici.pid) continue; // yalnız bu başsız Chrome'un renderer'ları
    const p = zaman.split(':').map(Number); // [ss:]dd:ss.cc
    const sn = p.reduce((s, x) => s * 60 + x, 0);
    m.set(Number(pid), sn);
  }
  return m;
}

const G = Number(a.genislik);
const Y = Number(a.yukseklik);
const adres = (panel) => (panel === 'index' ? `http://127.0.0.1:${port}/#erisim=${jeton}` : `http://127.0.0.1:${port}/panel/${panel}.html#erisim=${jeton}`);

async function sekmeAc(panel) {
  const onceki = await rendererPidleri();
  const { browserContextId } = await cdp('Target.createBrowserContext');
  const { targetId } = await cdp('Target.createTarget', { url: 'about:blank', browserContextId, width: G, height: Y });
  const { sessionId } = await cdp('Target.attachToTarget', { targetId, flatten: true });
  await cdp('Emulation.setDeviceMetricsOverride', { width: G, height: Y, deviceScaleFactor: Number(a.olcek), mobile: false }, sessionId);
  await cdp('Performance.enable', {}, sessionId);
  await cdp('Page.enable', {}, sessionId);
  // Her panel ayrı pencerede ve görünür kabul edilir (08 §2.1); başsız kipte
  // odak öykünmesi olmadan arka plandaki sekmenin çizimi kısılır.
  await cdp('Emulation.setFocusEmulationEnabled', { enabled: true }, sessionId);
  await cdp('Page.navigate', { url: adres(panel) }, sessionId);
  await cdp('Page.bringToFront', {}, sessionId);
  await bekle(2500);
  const sonraki = await rendererPidleri();
  const yeniler = [...sonraki.keys()].filter((k) => !onceki.has(k));
  // Her sekme kendi tarayıcı bağlamında açılır; açılırken doğan renderer'lar (birden çok olabilir) o sekmenindir.
  return { panel, targetId, sessionId, pidler: yeniler, browserContextId };
}

async function metrikler(s) {
  const { metrics } = await cdp('Performance.getMetrics', {}, s.sessionId);
  return Object.fromEntries(metrics.map((m) => [m.name, m.value]));
}

// Ölçümde (--olc) W3'ün beş paneli; ekran görüntüsünde senaryo paneli de.
const paneller = (a.paneller ?? (a.olc ? 'index,durum,grafik,harita,sonar,atis' : 'index,durum,grafik,harita,sonar,atis,senaryo')).split(',');
try {
  if (a.ekran) {
    const dizin = resolve(a.ekran);
    mkdirSync(dizin, { recursive: true });
    for (const [i, p] of paneller.entries()) {
      if (i > 0) {
        await sunucuDurdur(sunucu);
        sunucu = await sunucuBaslat();
      }
      const s = await sekmeAc(p);
      await bekle(Number(a.bekle) * 1000);
      if (a.etkilesim && (p === 'durum' || p === 'senaryo')) {
        const ifade =
          p === 'durum'
            ? "(async () => { const d = (k) => document.querySelector(`[data-komut=${k}]`); if (d('baslat').getAttribute('aria-disabled') === 'false') { d('baslat').click(); await new Promise((r) => setTimeout(r, 2500)); } d('duraklat').click(); })()"
            : "(async () => { document.querySelector('[data-ad=\"hatali.toml\"]').click(); await new Promise((r) => setTimeout(r, 600)); document.querySelector('#dogrula').click(); })()";
        await cdp('Runtime.evaluate', { expression: ifade, awaitPromise: true }, s.sessionId);
        await bekle(1500);
      }
      await cdp('Page.bringToFront', {}, s.sessionId);
      await bekle(300);
      const { data } = await cdp('Page.captureScreenshot', { format: 'png' }, s.sessionId);
      const dosya = join(dizin, `${a.onek}${p}-${G}x${Y}.png`);
      writeFileSync(dosya, Buffer.from(data, 'base64'));
      const m = await metrikler(s);
      console.log(`${dosya}  (JS yığını ${(m.JSHeapUsedSize / 1048576).toFixed(1)} MiB)`);
      await cdp('Target.closeTarget', { targetId: s.targetId });
      await cdp('Target.disposeBrowserContext', { browserContextId: s.browserContextId });
    }
  }
  if (a.kopma) {
    // Kabul 5: bağlantı kopar (sunucu süreci durur), şerit gösterir; sunucu dönünce panel kendiliğinden toparlar.
    const dizin = resolve(a.kopma);
    mkdirSync(dizin, { recursive: true });
    const p = paneller.find((x) => x !== 'index') ?? 'durum';
    const s = await sekmeAc(p);
    await bekle(Number(a.bekle) * 1000);
    const cek = async (ad) => {
      await cdp('Page.bringToFront', {}, s.sessionId);
      await bekle(300);
      const { data } = await cdp('Page.captureScreenshot', { format: 'png' }, s.sessionId);
      const dosya = join(dizin, `kopma-${ad}-${p}-${G}x${Y}.png`);
      writeFileSync(dosya, Buffer.from(data, 'base64'));
      console.log(dosya);
    };
    const durum = async () => (await cdp('Runtime.evaluate', { expression: "document.querySelector('.baglanti-hucre')?.innerText.replace(/\\s+/g,' ')", returnByValue: true }, s.sessionId)).result.value;
    await cek('1-bagli');
    await sunucuDurdur(sunucu);
    await bekle(3500);
    console.log('kopukken şerit:', await durum());
    await cek('2-kopuk');
    sunucu = await sunucuBaslat();
    const t0 = performance.now();
    let toparlandi = null;
    for (let i = 0; i < 100; i++) {
      await bekle(100);
      if (/BAĞLI/.test(await durum())) {
        toparlandi = (performance.now() - t0) / 1000;
        break;
      }
    }
    console.log(`sunucu döndükten sonra yeniden bağlanma: ${toparlandi === null ? 'olmadı (10 s)' : toparlandi.toFixed(1) + ' s'}`);
    await bekle(4000);
    await cek('3-toparlandi');
  }
  if (a.olc) {
    const sure = Number(a.olc);
    const sekmeler = [];
    for (const p of paneller.filter((x) => x !== 'index')) sekmeler.push(await sekmeAc(p));
    await bekle(10_000); // ısınma
    const t0 = performance.now();
    const cpu0 = await rendererPidleri();
    const m0 = await Promise.all(sekmeler.map(metrikler));
    const gpu0 = (await cdp('SystemInfo.getProcessInfo')).processInfo.filter((x) => x.type === 'GPU').reduce((s, x) => s + x.cpuTime, 0);
    await bekle(sure * 1000);
    const dt = (performance.now() - t0) / 1000;
    const cpu1 = await rendererPidleri();
    const m1 = await Promise.all(sekmeler.map(metrikler));
    const gpu1 = (await cdp('SystemInfo.getProcessInfo')).processInfo.filter((x) => x.type === 'GPU').reduce((s, x) => s + x.cpuTime, 0);
    console.log(`\nÖlçüm: ${sekmeler.length} sekme aynı anda açık, ${dt.toFixed(1)} s, ${G}×${Y}, başsız Chrome ${surum.Browser}`);
    console.log('panel     işlemci(süreçler) ana iş parçacığı  JS yığını   süreç RSS (toplam)');
    for (let i = 0; i < sekmeler.length; i++) {
      const s = sekmeler[i];
      // Sekmenin süreçleri: sekme açılırken bu Chrome'da doğan renderer'lar (toplam)
      let cpu = 0;
      let rss = 0;
      for (const pid of s.pidler) {
        if (cpu0.has(pid) && cpu1.has(pid)) cpu += ((cpu1.get(pid) - cpu0.get(pid)) / dt) * 100;
        try {
          rss += Number(execFileSync('ps', ['-o', 'rss=', '-p', String(pid)]).toString().trim()) / 1024;
        } catch {
          /* süreç kapanmış */
        }
      }
      if (s.pidler.length === 0) cpu = rss = NaN;
      const anaIs = ((m1[i].TaskDuration - m0[i].TaskDuration) / dt) * 100;
      console.log(
        `${s.panel.padEnd(9)} ${cpu.toFixed(1).padStart(8)} %      ${anaIs.toFixed(1).padStart(8)} %     ${(m1[i].JSHeapUsedSize / 1048576).toFixed(1).padStart(6)} MiB  ${rss.toFixed(0).padStart(6)} MiB`,
      );
    }
    console.log(`GPU süreci (bütün sekmeler, ortak): ${(((gpu1 - gpu0) / dt) * 100).toFixed(1)} %`);
  }
} finally {
  try {
    await cdp('Browser.close');
  } catch {
    /* yok say */
  }
  tarayici.kill();
  sunucu?.kill('SIGTERM');
  await bekle(300);
  rmSync(profil, { recursive: true, force: true });
}
process.exit(0);
