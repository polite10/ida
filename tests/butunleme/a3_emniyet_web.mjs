// W5-web-emniyet kabul 2: `ida_sim --kip sil --senaryo scenarios/z1.toml --a3-port 0 --a3-bekle`
// + panelin emniyet komut katmanı (web/src/a3/emniyet-istek.ts; Node'un WebSocket ve fetch'i).
// Acil durdurmaya bas → sunucunun `emniyet/girdiler` kanalında etkisi görülür; kaldırma jetonsuz
// reddedilir, jetonla kabul edilir. Zamana değil olaya bağlı bekler (W5-K43); sabit uyku yok.
// Kullanım: node a3_emniyet_web.mjs <ida_sim> <depo kökü> <web dizini> <çıktı dizini>
// Çıkış: 0 geçti, 1 kaldı, 77 atlandı.

import { spawn } from 'node:child_process';
import { mkdirSync } from 'node:fs';
import { createInterface } from 'node:readline';
import { ATLA, a3Kitapligi, bekle } from '../../sim/a3/test/ortak.mjs';

const [idaSim, kok, web, cikti] = process.argv.slice(2);
const a3 = await a3Kitapligi(web, cikti);
if (a3 === null) {
  console.log('ATLANDI: web/node_modules/esbuild yok (web/ dizininde npm ci)');
  process.exit(ATLA);
}
if (typeof globalThis.WebSocket !== 'function') {
  console.log('ATLANDI: bu Node sürümünde yerleşik WebSocket yok');
  process.exit(ATLA);
}
const hatalar = [];
const dogrula = (kosul, ne) => {
  if (!kosul) hatalar.push(ne);
  console.log(`  ${kosul ? 'GEÇTİ' : 'KALDI'} ${ne}`);
};

mkdirSync(cikti, { recursive: true });
const surec = spawn(idaSim, ['--kip', 'sil', '--senaryo', 'scenarios/z1.toml', '--kok', kok, '--a3-port', '0', '--a3-bekle', '--gunluk', '1'],
  { stdio: ['ignore', 'pipe', 'pipe'] });
let stderr = '';
surec.stderr.on('data', (d) => { stderr += d.toString(); });
const bitti = new Promise((coz) => surec.once('close', (kod) => coz(kod)));
let b = null;
let komutlar = null;
let em = null;
try {
  const adres = await new Promise((coz, red) => {
    const satirlar = createInterface({ input: surec.stdout });
    const temizle = () => {
      clearTimeout(t);
      satirlar.close();
      surec.off('error', hata);
      surec.off('close', erkenBitti);
    };
    const hata = (h) => { temizle(); red(h); };
    const erkenBitti = (kod) => hata(new Error(`sim açılış adresinden önce bitti (çıkış ${kod})`));
    const t = setTimeout(() => hata(new Error('açılış adresi gelmedi')), 20000);
    surec.once('error', hata);
    surec.once('close', erkenBitti);
    satirlar.once('line', (s) => { temizle(); coz(s.trim()); });
  });
  const m = /^http:\/\/127\.0\.0\.1:(\d+)\/#erisim=([0-9a-f]{64})$/.exec(adres);
  dogrula(m !== null, 'açılış adresi standart çıktıda (A3-K05)');
  if (m === null) throw new Error('sim geçerli A3 açılış adresi vermedi');
  b = new a3.A3Baglanti({ taban: `http://127.0.0.1:${m[1]}`, jeton: m[2], gecmisOlay: 100 });
  komutlar = new a3.KomutYoneticisi(b);
  em = new a3.EmniyetIstekleri(b, komutlar);
  let girdi = null;
  b.abone('emniyet/girdiler', (c) => { girdi = c.yuk; });
  const olaylar = [];
  b.olaylar((o) => olaylar.push(o));
  b.baslat();
  await bekle(() => b.bilgi.durum === 'bagli', 10000, 'bağlantı');
  const http = b.http;
  await http.istemciKaydet('a3_emniyet_web');
  const sonuc = (no) => olaylar.find((o) => o.tur === 'komut_sonucu' && o.ayrinti?.komut_no === no);

  // Canlı çalıştırma gerekir (A3-K91): başlat, sonra duraklat (emniyet işlemleri duraklatılmışken de işlenir; A3-K95).
  const d0 = await http.durum();
  const baslat = await http.calistirma('baslat', { run_id: null, durum_rev: d0.durum_rev });
  await bekle(() => sonuc(baslat.komut_no) !== undefined, 30000, 'baslat sonucu');
  let d = await http.durum();
  const dur = await http.calistirma('duraklat', { run_id: d.run_id, durum_rev: d.durum_rev });
  await bekle(() => sonuc(dur.komut_no) !== undefined, 5000, 'duraklat sonucu');
  await bekle(() => girdi !== null, 10000, 'ilk emniyet/girdiler çerçevesi (çalıştırma başlayınca yayınlanır)');
  d = await http.durum();
  dogrula(d.durum === 'duraklatildi', `duraklatıldı (${d.durum})`);

  const BAS = { girdi: 'acil_durdurma', hedef: 'basili' };
  const KALDIR = { girdi: 'acil_durdurma', hedef: 'serbest' };
  const bas0 = girdi.acil_durdurma;
  dogrula(bas0 === 0 || bas0 === 'serbest', `başlangıçta acil durdurma serbest (${bas0})`);
  // Başlangıç revizyonu yetkili uçtan okunur: kanal çerçevesi en çok 100 ms (10 Hz) bayat olabilir ve
  // hazırlık oturumunun sonu revizyonu kanaldan önce artırır (A3-K117 md. 4; yarış değil, seyreltme).
  const rev0 = Number((await http.istek('GET', '/emniyet')).emniyet_rev);

  // 1) Acil durdurmaya bas: tek adım; sunucunun kanalında etkisi görülür.
  const r1 = await em.artir(BAS);
  dogrula(r1.asama === 'uygulandi', `acil durdurmaya bas: ${r1.asama} ${r1.ileti ?? ''}`);
  await bekle(() => girdi.acil_durdurma === 1 || girdi.acil_durdurma === 'basili', 5000, 'kanalda acil durdurma basılı');
  await bekle(() => olaylar.some((o) => o.tur === 'emniyet_girdisi_degisti' && o.ayrinti?.girdi === 'acil_durdurma' && o.ayrinti?.sonraki === 'basili'), 5000, 'emniyet_girdisi_degisti olayı');
  dogrula(true, 'sunucu emniyet_girdisi_degisti olayını üretti (acil_durdurma → basili)');
  const olay1 = olaylar.find((o) => o.tur === 'emniyet_girdisi_degisti' && o.ayrinti?.girdi === 'acil_durdurma' && o.ayrinti?.sonraki === 'basili');
  dogrula(Number(olay1?.ayrinti?.emniyet_rev) === rev0 + 1, `olaydaki emniyet_rev yetkili revizyondan tam bir fazla (${rev0} -> ${olay1?.ayrinti?.emniyet_rev})`);
  await bekle(() => Number(girdi.emniyet_rev) === rev0 + 1, 5000, 'kanalda emniyet_rev yetkili değere yetişti');
  dogrula(Number(girdi.emniyet_rev) === rev0 + 1, `kanaldaki emniyet_rev yetişti (${rev0} -> ${girdi.emniyet_rev})`);
  const e1 = await http.istek('GET', '/emniyet');
  dogrula(e1.yetkili.acil_durdurma === 'basili', 'GET /emniyet yetkili durum: basili');

  // 1b) Panelin bildiği çalıştırma kimliği bayatsa da acil durdurma reddedilmez (A3-K54 1.0.1 notu, W4-K19 (a)):
  //     ham istek, bilerek başka bir `run_id` ile.
  const bayatKimlik = '19990101-000000-000000'; // şemaya uygun (A3-K124) ama güncel olmayan kimlik
  const revOnce = Number(girdi.emniyet_rev);
  let bayatSonuc = null;
  try {
    const y = await http.komut('/emniyet/degistir', { beklenen_run_id: bayatKimlik, girdi: 'acil_durdurma', hedef_durum: 'basili' });
    await bekle(() => sonuc(y.komut_no) !== undefined, 5000, 'bayat run_id ile acil durdurma sonucu');
    bayatSonuc = sonuc(y.komut_no).ayrinti;
  } catch (h) {
    bayatSonuc = { sonuc: 'istek hatası', hata_kodu: h?.kod ?? String(h) };
  }
  dogrula(bayatSonuc?.sonuc === 'uygulandi', `bayat run_id ile acil durdurma reddedilmez (${bayatSonuc?.sonuc} ${bayatSonuc?.hata_kodu ?? ''})`);
  await bekle(() => Number(girdi.emniyet_rev) > revOnce, 5000, 'kısıtlama revizyonu artırdı');
  dogrula(girdi.acil_durdurma === 1 || girdi.acil_durdurma === 'basili', 'bayat run_id sonrası acil durdurma basılı');

  // 2) Jetonsuz kaldırma: istemci tarafında engellenir, sunucuya hiçbir şey gitmez.
  const r2 = await em.gevset(KALDIR, true);
  dogrula(r2.asama === 'reddedildi' && r2.gonderildi_mi === false && r2.hata_kodu === 'jeton_yok', `jetonsuz kaldırma istemcide durdu (${r2.hata_kodu})`);
  dogrula(girdi.acil_durdurma === 1 || girdi.acil_durdurma === 'basili', 'jetonsuz kaldırma durumu değiştirmedi');

  // 2b) Sunucu da jetonsuz gevşetmeyi reddeder (ham istek; panel katmanı atlanarak).
  let ham = null;
  try {
    await http.komut('/emniyet/degistir', { beklenen_run_id: d.run_id, girdi: 'acil_durdurma', hedef_durum: 'serbest', beklenen_emniyet_rev: Number(girdi.emniyet_rev) });
  } catch (h) {
    ham = h;
  }
  dogrula(ham?.kod === 'onay_gerekli', `sunucu jetonsuz gevşetmeyi reddeder (${ham?.kod})`);
  dogrula(girdi.acil_durdurma === 1 || girdi.acil_durdurma === 'basili', 'ham ret sonrası hâlâ basılı');

  // 3) Jetonla kaldırma: iki adım, kabul edilir; gösterge sunucu bildirince değişir.
  const j = await em.jetonIste(KALDIR);
  dogrula('jeton' in j, `onay jetonu alındı (${j.hata?.kod ?? 'ok'})`);
  if ('jeton' in j) {
    dogrula(girdi.acil_durdurma === 1 || girdi.acil_durdurma === 'basili', 'jeton almak durumu değiştirmedi');
    const r3 = await em.gevset(KALDIR, true);
    dogrula(r3.asama === 'uygulandi', `jetonla kaldırma: ${r3.asama} ${r3.ileti ?? ''}`);
    await bekle(() => girdi.acil_durdurma === 0 || girdi.acil_durdurma === 'serbest', 5000, 'kanalda acil durdurma serbest');
    dogrula(girdi.acil_durdurma === 0 || girdi.acil_durdurma === 'serbest', 'sunucu bildirince acil durdurma serbest (kanaldan)');
    const e3 = await http.istek('GET', '/emniyet');
    dogrula(e3.yetkili.acil_durdurma === 'serbest', 'GET /emniyet yetkili durum: serbest');
    // 4) Jeton tek kullanımlık: aynı jetonla ikinci gönderim yapılmaz.
    const r4 = await em.gevset(KALDIR, true);
    dogrula(r4.asama === 'reddedildi' && r4.hata_kodu === 'jeton_yok', 'tüketilmiş jeton istemcide yeniden kullanılmaz');
    // 4b) Sunucu da tüketilmiş jetonu reddeder (ham istek; A3-K94 tek kullanım).
    let tekrar = null;
    try {
      await http.komut('/emniyet/degistir', { beklenen_run_id: d.run_id, girdi: 'acil_durdurma', hedef_durum: 'serbest', onay: true,
        onay_jetonu: j.jeton.jeton, beklenen_emniyet_rev: j.jeton.emniyet_rev });
    } catch (h) {
      tekrar = h;
    }
    dogrula(tekrar !== null && ['onay_jetonu_gecersiz', 'emniyet_rev_uyusmuyor'].includes(tekrar.kod), `sunucu tüketilmiş jetonu reddeder (${tekrar?.kod})`);
  }
} catch (h) {
  hatalar.push(`beklenmeyen hata: ${h?.stack ?? h}`);
} finally {
  em?.kapat();
  komutlar?.kapat();
  b?.kapat();
  if (surec.exitCode === null && surec.signalCode === null) surec.kill('SIGTERM');
  const zorlaKapat = setTimeout(() => surec.kill('SIGKILL'), 5000);
  try { await bitti; } finally { clearTimeout(zorlaKapat); }
}
if (hatalar.length > 0) {
  console.log(`KALDI (${hatalar.length}):\n- ${hatalar.join('\n- ')}`);
  console.log(stderr.split('\n').slice(-8).join('\n'));
  process.exit(1);
}
console.log('GEÇTİ');
process.exit(0);
