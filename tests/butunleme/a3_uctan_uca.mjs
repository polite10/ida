// W4-bagla-a3 kabul 3: `ida_sim --kip sil --senaryo scenarios/z1.toml --a3-port 0 --a3-bekle` +
// panelin A3 istemcisi (web/src/a3; Node'un WebSocket ve fetch'i; web/'e dosya eklenmez):
// sürüm → WebSocket → merhaba → abonelik → baslat → beş panelin kanallarında gerçek değerler
// (fizik/durum derinliği sim'in CSV'siyle aynı adımlarda) → hız 8 → duraklat/sürdür/adımla reddi
// (W4-K28 (1)) → ARM (onay jetonu, emniyet revizyonu) → ARM'ın kart yanıtı ve kart/durum → bitti.
// Kullanım: node a3_uctan_uca.mjs <ida_sim> <depo kökü> <web dizini> <çıktı dizini>
// Çıkış: 0 geçti, 1 kaldı, 77 atlandı (web/node_modules yok ya da Node'da WebSocket yok).

import { spawn } from 'node:child_process';
import { mkdirSync, readFileSync } from 'node:fs';
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
  else console.log(`  GEÇTİ ${ne}`);
};

mkdirSync(cikti, { recursive: true });
const csvYolu = join(cikti, 'a3_uctan_uca.csv');
const surec = spawn(idaSim, ['--kip', 'sil', '--senaryo', 'scenarios/z1.toml', '--kok', kok, '--a3-port', '0', '--a3-bekle',
  '--csv', csvYolu, '--gunluk', '1'], { stdio: ['ignore', 'pipe', 'pipe'] });
let stderr = '';
surec.stderr.on('data', (d) => { stderr += d.toString(); });
const bitti = new Promise((coz) => surec.on('exit', (kod) => coz(kod)));
const satirlar = createInterface({ input: surec.stdout });
const adres = await new Promise((coz, red) => {
  const t = setTimeout(() => red(new Error('açılış adresi gelmedi')), 20000);
  satirlar.once('line', (s) => {
    clearTimeout(t);
    coz(s.trim());
  });
});
const m = /^http:\/\/127\.0\.0\.1:(\d+)\/#erisim=([0-9a-f]{64})$/.exec(adres);
dogrula(m !== null, 'açılış adresi standart çıktıda (A3-K05)');
let cikisKodu = 1;
try {
  const b = new a3.A3Baglanti({ taban: `http://127.0.0.1:${m[1]}`, jeton: m[2], gecmisOlay: 100 });
  const say = new Map();
  const fizik = [];
  const kart = [];
  const KANALLAR = ['calistirma/durum', 'sayac/gercek_zaman', 'fizik/durum', 'kart/durum', 'atis/durum',
    'emniyet/girdiler', 'ariza/etkin', 'sensor/basinc', 'sensor/dvl', 'sensor/batarya', 'baglanti/durum',
    'kart/cekirdek_sayaclari'];
  for (const k of KANALLAR) {
    b.abone(k, (c) => {
      say.set(k, (say.get(k) ?? 0) + 1);
      if (k === 'fizik/durum') fizik.push({ adim: Number(c.yuk.adim_no), d: c.yuk.govdeler[0].p_n[2], t: c.baslik.t_ns });
      if (k === 'kart/durum') kart.push(c.yuk);
    }, k === 'fizik/durum' || k === 'kart/durum' ? 50 : undefined);
  }
  const olaylar = [];
  b.olaylar((o) => olaylar.push(o));
  b.baslat();
  await bekle(() => b.bilgi.durum === 'bagli', 5000, 'bağlantı');
  dogrula(b.bilgi.surum?.a3_surumu === '1.0', 'GET /surum (A3-K15)');
  dogrula(b.bilgi.merhaba?.tur === 'merhaba', 'merhaba (A3-K17)');
  await bekle(() => b.abonelikSonuclari.size >= KANALLAR.length - 2, 3000, 'abonelik yanıtları');
  for (const [k, y] of b.abonelikSonuclari) dogrula(y.sonuc === 'kabul', `abonelik kabul: ${k}`);

  const http = b.http;
  const istemci = await http.istemciKaydet('a3_uctan_uca');
  const sonuc = (no) => olaylar.find((o) => o.tur === 'komut_sonucu' && o.ayrinti?.komut_no === no);
  const d0 = await http.durum();
  dogrula(d0.durum === 'yuklendi' && d0.run_id === null, 'yuklendi, run_id null (A3-K124)');
  const baslat = await http.calistirma('baslat', { run_id: null, durum_rev: d0.durum_rev });
  await bekle(() => sonuc(baslat.komut_no) !== undefined, 20000, 'baslat sonucu (ön denetim: A1 el sıkışması)');
  dogrula(sonuc(baslat.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'baslat uygulandı (A3-K62)');
  let d = await http.durum();
  dogrula(d.durum === 'kosuyor' && /^\d{8}-\d{6}-[0-9a-f]{6}$/.test(d.run_id ?? ''), `kosuyor, run_id ${d.run_id}`);
  // Duraklatma bu dilimde yok (W4-K28 (1)). Hız bağımsızlığı: SIL olabildiğince hızlı koşar; koşu
  // görülür görülmez hız çarpanı 8 verilir (A3-K66; yalnız bekleme, bayt akışı değişmez), böylece
  // 90 s'lik senaryo ≈ 11 s duvar sürer ve ARM koşu sürerken işlenir. Kanal değerleri adım
  // numarasıyla eşlenir (duvar zamanına bakılmaz).
  const hiz = await http.calistirma('hiz', { run_id: d.run_id, durum_rev: d.durum_rev }, { carpan: 8 });
  await bekle(() => sonuc(hiz.komut_no) !== undefined, 5000, 'hız sonucu');
  dogrula(sonuc(hiz.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'hız 8 uygulandı');
  d = await http.durum();
  dogrula(d.durum === 'kosuyor', `hızdan sonra koşuyor (sim zamanı ${d.sim_t_ns / 1e9} s)`);
  // W4-K28 (1): duraklat, sürdür, adımla girişte gecersiz_durum; durum ve durum_rev değişmez.
  for (const [ad, ek] of [['duraklat', {}], ['surdur', {}], ['adimla', { periyot_sayisi: 1 }]]) {
    let hata = null;
    try {
      await http.calistirma(ad, { run_id: d.run_id, durum_rev: d.durum_rev }, ek);
    } catch (h) {
      hata = h;
    }
    dogrula(hata?.kod === 'gecersiz_durum' && String(hata?.message).includes('bu sürümde desteklenmiyor'),
      `${ad}: gecersiz_durum, "bu sürümde desteklenmiyor" (${hata?.kod})`);
  }
  const dSonra = await http.durum();
  dogrula(dSonra.durum === 'kosuyor' && dSonra.durum_rev === d.durum_rev, 'reddedilen isteklerden sonra durum ve durum_rev aynı');
  // Kart kestirimi otursun diye 2 s sim zamanını bekle (fizik/durum adım numarasıyla).
  await bekle(() => fizik.length > 0 && fizik[fizik.length - 1].adim >= 1600, 10000, '2 s sim zamanı');

  // ARM (genişletici; onay jetonu ve emniyet revizyonu: A3-K103, K118)
  const ist = { 'Ida-Istemci': String(istemci) };
  const jeton = await http.istek('POST', '/yer_istasyonu/onay_jetonu', { beklenen_run_id: d.run_id, komut: 'arm', parametreler: {} }, ist);
  const arm = await http.komut('/yer_istasyonu/komut', {
    beklenen_run_id: d.run_id, komut: 'arm', parametreler: {}, onay: true, onay_jetonu: jeton.onay_jetonu,
    beklenen_emniyet_rev: jeton.emniyet_rev,
  });
  dogrula(arm.sonuc === 'kuyrukta', 'ARM 202 kuyrukta');
  await bekle(() => sonuc(arm.komut_no) !== undefined, 5000, 'ARM komut_sonucu');
  dogrula(sonuc(arm.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'ARM ağ geçidinde uygulandı');
  const yiKomut = () => olaylar.find((o) => o.tur === 'yer_istasyonu_komutu' && o.ayrinti?.komut_no === arm.komut_no);
  await bekle(() => yiKomut() !== undefined, 5000, 'yer_istasyonu_komutu');
  dogrula(yiKomut()?.ayrinti?.durum === 'karta_iletildi', `yer_istasyonu_komutu: ${yiKomut()?.ayrinti?.durum}`);
  const yanit = () => olaylar.find((o) => o.tur === 'yer_istasyonu_yaniti' && o.ayrinti?.komut_no === arm.komut_no);
  await bekle(() => yanit() !== undefined, 10000, 'ARM kart yanıtı (yer_istasyonu_yaniti)');
  const y = yanit().ayrinti;
  console.log(`  ARM kart yanıtı: ${y.sonuc} (ack ${y.veri?.ack}, reason ${y.veri?.reason})`);
  dogrula(['kabul', 'ret'].includes(y.sonuc) && typeof y.veri?.ack === 'string', 'ARM yanıtı kartın sonucunu taşır');
  if (y.sonuc === 'kabul') {
    await bekle(() => kart.length > 0 && kart[kart.length - 1].silahli === 1, 5000, 'kart/durum silahlı');
    dogrula(true, 'kart/durum: silahli = 1');
  }
  await bekle(() => olaylar.some((o) => o.tur === 'calistirma_durumu_degisti' && o.ayrinti?.sonraki === 'bitti'), 60000, 'bitti');
  const kod = await bitti;
  dogrula(kod === 0, `ida_sim çıkış kodu 0 (${kod})`);
  dogrula(b.bilgi.kapanisKodu === 1001 || process.platform === 'win32', `kapanış 1001 (A3-K49): ${b.bilgi.kapanisKodu}`);
  b.kapat();

  // Değerler sim'in CSV'siyle aynı adımlarda (fizik/durum t_ns = adim_no · 1,25 ms; derinlik %.6f).
  const csv = readFileSync(csvYolu, 'utf8').trim().split('\n').slice(1).map((s) => s.split(','));
  const satir = new Map(csv.map((c) => [Math.round(Number(c[0]) / 0.00125), Number(c[2])]));
  let eslesen = 0;
  let denenen = 0;
  for (const f of fizik) {
    if (f.adim === 0 || f.adim % 4 !== 0) continue;
    denenen += 1;
    if (satir.has(f.adim) && Math.abs(satir.get(f.adim) - f.d) <= 5.1e-7 && f.t === BigInt(f.adim) * 1250000n) eslesen += 1;
  }
  dogrula(denenen >= 5 && eslesen === denenen, `fizik/durum derinliği CSV ile aynı adımda (${eslesen}/${denenen})`);
  // scenarios/z1.toml yerleşik Z1'in GUIDED_SP'siz biçimidir (W4-K17): ARM ve kip var, dalış hedefi
  // yok; araç yüzer dengede kalır. Kartın kestirdiği derinlik yer gerçeğiyle tutarlı olmalı.
  const enDerin = Math.max(...fizik.map((f) => f.d));
  const enSig = Math.min(...fizik.map((f) => f.d));
  dogrula(enSig > -0.05 && enDerin < 0.3, `senaryoyla tutarlı: araç yüzer dengede (derinlik ${enSig.toFixed(3)}…${enDerin.toFixed(3)} m)`);
  const sonKart = kart[kart.length - 1];
  const sonFizik = fizik[fizik.length - 1];
  dogrula(sonKart !== undefined && Math.abs(sonKart.derinlik_m - sonFizik.d) < 0.1,
    `kart/durum derinlik kestirimi yer gerçeğine yakın (${sonKart?.derinlik_m?.toFixed(3)} / ${sonFizik.d.toFixed(3)} m)`);
  const eksik = KANALLAR.filter((k) => !say.has(k));
  dogrula(eksik.length === 0, `beş panelin kanalları geldi (eksik: ${eksik.join(', ') || 'yok'})`);
  dogrula(b.bilgi.olayBosluklari.length === 0, 'olay boşluğu yok');
  console.log(`uçtan uca: ${[...say.entries()].map(([k, n]) => `${k} ${n}`).join(', ')}`);
  cikisKodu = hatalar.length === 0 ? 0 : 1;
} catch (h) {
  hatalar.push(String(h?.stack ?? h));
} finally {
  surec.kill('SIGTERM');
}
for (const h of hatalar) console.log('  KALDI', h);
if (hatalar.length > 0) console.log(stderr.split('\n').slice(-15).join('\n'));
console.log(`a3_uctan_uca: ${hatalar.length === 0 ? 'GEÇTİ' : 'KALDI'}`);
process.exit(hatalar.length === 0 ? cikisKodu : 1);
