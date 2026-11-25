// W5-a3-senaryo kabul 3–5: `ida_sim --a3-bekle --a3-surekli` + panelin A3 istemcisi (web/src/a3; Node'un
// WebSocket ve fetch'i): senaryoları listele → doğrula → yükle → başlat → bitince sıfırla → yeniden başlat;
// koşuyu yarıda durdur → sıfırla → yeniden koştur; geçersiz adlar 400; süreç Durdur'dan ve bitişten sonra
// açık kalır, SIGTERM ile kapanır. Yüklenen senaryonun ve sıfırlanan koşunun CSV'si, taze bir `ida_sim`
// sürecinin aynı senaryoyu komut satırından koşturmasıyla bayt bayt aynıdır (W5-K47).
// Kullanım: node a3_senaryo_uctan_uca.mjs <ida_sim> <depo kökü> <web dizini> <çıktı dizini>
// Çıkış: 0 geçti, 1 kaldı, 77 atlandı (web/node_modules yok ya da Node'da WebSocket yok).

import { spawn } from 'node:child_process';
import { mkdirSync, readFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';
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

// Taze süreç: senaryoyu komut satırından koşturur, çıkışını bekler (olayla: 'exit').
async function tazeKosu(senaryo, csv) {
  const s = spawn(idaSim, ['--senaryo', `scenarios/${senaryo}`, '--kok', kok, '--csv', csv, '--gunluk', '0'], { stdio: 'ignore' });
  const kod = await new Promise((coz) => s.on('exit', coz));
  return kod;
}
const okuCsv = (y) => readFileSync(y, 'utf8');

const csvA3 = join(cikti, 'a3_senaryo.csv');
const surec = spawn(idaSim, ['--senaryo', 'scenarios/z1.toml', '--kok', kok, '--a3-port', '0', '--a3-bekle', '--a3-surekli',
  '--csv', csvA3, '--gunluk', '1'], { stdio: ['ignore', 'pipe', 'pipe'] });
let stderr = '';
let bostaSayisi = 0;
const bostaBekleyen = [];
const stderrSatirlari = createInterface({ input: surec.stderr });
stderrSatirlari.on('line', (s) => {
  stderr += s + '\n';
  if (s.includes('bekleniyor (--a3-surekli)')) {
    bostaSayisi += 1;
    for (const f of bostaBekleyen.splice(0)) f();
  }
});
const bitti = new Promise((coz) => surec.on('exit', (kod) => coz(kod)));
const adresSatiri = createInterface({ input: surec.stdout });
const adres = await new Promise((coz, red) => {
  const t = setTimeout(() => red(new Error('açılış adresi gelmedi')), 20000);
  adresSatiri.once('line', (s) => {
    clearTimeout(t);
    coz(s.trim());
  });
});
// Çalıştırma bitişini (süreklilik beklemesine girildi) olayla bekler: CSV bu noktada yazılmıştır.
const bostaBekle = (n) => new Promise((coz) => {
  const kontrol = () => (bostaSayisi >= n ? coz() : bostaBekleyen.push(kontrol));
  kontrol();
});
const m = /^http:\/\/127\.0\.0\.1:(\d+)\/#erisim=([0-9a-f]{64})$/.exec(adres);
dogrula(m !== null, 'açılış adresi standart çıktıda');
let b = null;
try {
  b = new a3.A3Baglanti({ taban: `http://127.0.0.1:${m[1]}`, jeton: m[2], gecmisOlay: 100 });
  b.baslat();
  await bekle(() => b.bilgi.durum === 'bagli', 5000, 'bağlantı');
  const http = b.http;
  await http.istemciKaydet('a3_senaryo_uctan_uca');
  const durum = () => http.durum();
  // `yuklendi`, yeni dünyanın kuruluşu bitince sayılır: geçiş penceresinde durum `yuklendi` ama senaryo özeti
  // null'dır ve `baslat` reddedilir (W5-K58).
  const durumBekle = (ad) => bekle(async () => false, 1, 'x').catch(() => {}).then(async () => {
    const son = Date.now() + 120000;
    while (Date.now() < son) {
      const d = await durum();
      if (d.durum === ad && (ad !== 'yuklendi' || typeof d.senaryo_ozeti === 'string')) return true;
      await new Promise((c) => setTimeout(c, 5));
    }
    return false;
  });
  const kontrol = async (ad, ek = {}) => {
    const d = await durum();
    return http.calistirma(ad, { run_id: d.run_id, durum_rev: d.durum_rev }, ek);
  };

  // ---- liste, dosya, doğrula ----
  const liste = await http.istek('GET', '/senaryolar');
  const adlar = liste.senaryolar.map((s) => s.ad);
  dogrula(adlar.includes('z1.toml') && adlar.includes('ornek-havuz.toml') && adlar.every((a) => /^[a-z0-9_-]{1,64}\.toml$/.test(a)), `GET /senaryolar: ${adlar.length} dosya, hepsi kalıba uyuyor`);
  const dosya = await http.istek('GET', '/senaryolar/ornek-havuz.toml');
  dogrula(dosya.ad === 'ornek-havuz.toml' && dosya.icerik.includes('[meta]'), 'GET /senaryolar/{ad}');
  const dg = await http.istek('POST', '/senaryo/dogrula', { ad: 'ornek-havuz.toml' });
  dogrula(dg.gecerli === true && /^sha256:[0-9a-f]{64}$/.test(dg.senaryo_ozeti), 'POST /senaryo/dogrula: geçerli, özet');
  for (const kotu of ['..%2Fx.toml', '%2Fetc%2Fpasswd', 'Z1.toml', 'a.txt']) {
    let kod = null;
    try {
      await http.istek('GET', `/senaryolar/${kotu}`);
    } catch (h) {
      kod = h.http;
    }
    dogrula(kod === 400, `geçersiz ad ${kotu}: 400`);
  }
  let r422 = null;
  try {
    await http.komut('/senaryo/yukle', { beklenen_run_id: null, beklenen_durum_rev: (await durum()).durum_rev, icerik: 'bozuk = [' });
  } catch (h) {
    r422 = h;
  }
  dogrula(r422?.http === 422 && r422.kod === 'senaryo_gecersiz' && (await durum()).durum === 'yuklendi', 'geçersiz senaryo yükleme 422, durum yuklendi');

  // ---- yükle → başlat → bitince sıfırla → yeniden başlat ----
  const refCsv = join(cikti, 'ornek_taze.csv');
  dogrula((await tazeKosu('ornek-havuz.toml', refCsv)) === 0, 'taze süreçte komut satırından ornek-havuz.toml koştu');
  const d0 = await durum();
  const y = await http.komut('/senaryo/yukle', { beklenen_run_id: null, beklenen_durum_rev: d0.durum_rev, ad: 'ornek-havuz.toml' });
  dogrula(y.sonuc === 'kabul' && y.senaryo_ozeti === dg.senaryo_ozeti && (await durum()).senaryo_ozeti === dg.senaryo_ozeti, 'yükle: 200, durum yuklendi, yeni senaryo özeti');
  await kontrol('baslat');
  await bostaBekle(1);
  dogrula((await durum()).durum === 'bitti', 'çalıştırma bitti, süreç açık (sunucu yanıt veriyor)');
  dogrula(okuCsv(csvA3) === okuCsv(refCsv), 'yüklenen senaryonun CSV\'si taze süreçteki komut satırı koşusuyla bayt bayt aynı');
  await kontrol('sifirla');
  dogrula(await durumBekle('yuklendi'), 'bitti → sıfırla → yuklendi');
  await kontrol('baslat');
  await bostaBekle(2);
  dogrula(okuCsv(csvA3) === okuCsv(refCsv), 'sıfırla → başlat: ikinci koşunun CSV\'si ilkiyle aynı');

  // ---- koşuyu durdur → süreç açık → sıfırla → yeniden koştur ----
  await kontrol('sifirla');
  await durumBekle('yuklendi');
  await kontrol('baslat');
  await durumBekle('kosuyor');
  await kontrol('hiz', { carpan: 1 });
  await kontrol('durdur');
  dogrula(await durumBekle('durduruldu'), 'durdur → durduruldu');
  // Olayla beklenir (W5-K43): dünya durdu, kapanışını bitirdi ve süreklilik beklemesine girdi (stderr satırı).
  // Süreç bu noktadan sonra da açıktır ve yanıt verir.
  await bostaBekle(3);
  dogrula((await durum()).durum === 'durduruldu', 'durduruldu sonrası süreç açık (süreklilik beklemesinde), GET /durum yanıt veriyor');
  await kontrol('sifirla');
  dogrula(await durumBekle('yuklendi'), 'durduruldu → sıfırla → yuklendi');
  await kontrol('baslat');
  await bostaBekle(4);
  dogrula(okuCsv(csvA3) === okuCsv(refCsv), 'durdurup sıfırladıktan sonraki koşu da aynı CSV');
  b.kapat();
  surec.kill('SIGTERM');
  const cikis = await bitti;
  dogrula(cikis === 0 || process.platform === 'win32', `SIGTERM ile kapanış, çıkış kodu ${cikis}`);
} catch (h) {
  hatalar.push(String(h?.stack ?? h));
  surec.kill('SIGTERM');
}
for (const h of hatalar) console.log('  KALDI', h);
if (hatalar.length > 0) console.log(stderr.split('\n').slice(-15).join('\n'));
console.log(`a3_senaryo_uctan_uca: ${hatalar.length === 0 ? 'GEÇTİ' : 'KALDI'}`);
process.exit(hatalar.length === 0 ? 0 : 1);
