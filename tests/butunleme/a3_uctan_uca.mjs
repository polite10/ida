// W4-bagla-a3 kabul 3: `ida_sim --kip sil --senaryo scenarios/z1.toml --a3-port 0 --a3-bekle` +
// panelin A3 istemcisi (web/src/a3; Node'un WebSocket ve fetch'i; web/'e dosya eklenmez):
// sürüm → WebSocket → merhaba → abonelik → baslat → beş panelin kanallarında gerçek değerler
// (fizik/durum derinliği sim'in CSV'siyle aynı adımlarda) → duraklat → adımla (2 s'ye) → duraklatılmışken
// ARM (onay jetonu, emniyet revizyonu; A3-K58: beklemeden uygulanır, tele sürdürmede çıkar) → adımla →
// ARM'ın kart yanıtı ve kart/durum → sürdür → bitti (W4-duraklat kapsam 6: hızdan bağımsız kurulum).
// W5-web-komut kabul 2: ikinci bir `ida_sim` ile panelin komut katmanı (KomutYoneticisi; düğmelerin
// kullandığı tek yol) başlat → duraklat → adımla → sürdür → hız → durdur zincirini koşar; her adımda
// komutun sonucu (komut_sonucu / GET /komutlar) ve sunucunun bildirdiği durum (kanal ve GET /durum) doğrulanır.
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

// ---- W5-web-komut: panelin komut katmanıyla çalıştırma zinciri (gerçek sim) ----
async function panelKomutZinciri() {
  console.log('panel komut katmanı (W5-web-komut):');
  const sur2 = spawn(idaSim, ['--kip', 'sil', '--senaryo', 'scenarios/z1.toml', '--kok', kok, '--a3-port', '0', '--a3-bekle', '--gunluk', '1'],
    { stdio: ['ignore', 'pipe', 'pipe'] });
  let err2 = '';
  sur2.stderr.on('data', (d) => { err2 += d.toString(); });
  const satirlar2 = createInterface({ input: sur2.stdout });
  let b2 = null;
  let yon = null;
  const olay2 = [];
  try {
    const adres2 = await new Promise((coz, red) => {
      const t = setTimeout(() => red(new Error('ikinci sim: açılış adresi gelmedi')), 20000);
      satirlar2.once('line', (x) => { clearTimeout(t); coz(x.trim()); });
    });
    const mm = /^http:\/\/127\.0\.0\.1:(\d+)\/#erisim=([0-9a-f]{64})$/.exec(adres2);
    dogrula(mm !== null, 'ikinci sim açılış adresi');
    b2 = new a3.A3Baglanti({ taban: `http://127.0.0.1:${mm[1]}`, jeton: mm[2], gecmisOlay: 100 });
    b2.olaylar((o) => olay2.push(o));
    b2.baslat();
    await bekle(() => b2.bilgi.durum === 'bagli' && b2.son('calistirma/durum') !== undefined, 5000, 'ikinci bağlantı ve ilk durum çerçevesi');
    yon = new a3.KomutYoneticisi(b2, { sureMs: 20000, baslatSureMs: 30000 });
    const kanalDurum = () => {
      const c = b2.son('calistirma/durum');
      return c === undefined ? null : ['bos', 'yuklendi', 'hazirlaniyor', 'kosuyor', 'duraklatildi', 'bitti', 'durduruldu', 'hata', 'izleme'][c.yuk.durum];
    };
    // Ekranın (kanalın) durum_rev'i sunucunun yetkili değerine yetişmeden komut gönderilmez: komut
    // ekranın gösterdiği revizyonla gider (A3-K123); eski revizyon sunucuda reddedilir.
    const senkron = async () => {
      const hedef = (await b2.http.durum()).durum_rev;
      await bekle(() => Number(b2.son('calistirma/durum')?.yuk.durum_rev) === hedef, 5000, `kanal durum_rev ${hedef}`);
    };
    // Bir adım: komutu gönder, katmanın sonucunu, GET /komutlar kaydını ve sunucunun durumunu doğrula.
    const adim = async (ad, ek, beklenenDurum) => {
      await senkron();
      const r = await yon.gonder(ad, ek);
      dogrula(r.asama === 'uygulandi', `${ad}: komut katmanı sonucu uygulandı (${r.asama}${r.hata_kodu ? ' ' + r.hata_kodu : ''}; ${r.ileti})`);
      const kayit = await b2.http.komutDurumu(r.komut_no);
      dogrula(kayit.durum === 'uygulandi', `${ad}: GET /komutlar/${r.komut_no} uygulandi`);
      if (beklenenDurum !== null) {
        await bekle(() => kanalDurum() === beklenenDurum, 20000, `${ad}: kanal durumu ${beklenenDurum} (şimdi ${kanalDurum()})`);
        const d = await b2.http.durum();
        dogrula(d.durum === beklenenDurum, `${ad}: sunucunun bildirdiği durum ${beklenenDurum} (${d.durum})`);
        return d;
      }
      return b2.http.durum();
    };

    const d0 = await b2.http.durum();
    dogrula(d0.durum === 'yuklendi' && d0.run_id === null, 'ikinci sim: yuklendi, run_id null');
    // Düğme etkinliği sunucunun bildirdiği duruma uyar (Tablo 4.7a): yüklendide yalnız başlat/sıfırla/yükle
    const g0 = a3.dugmeler('yuklendi', 'sil');
    dogrula(g0.baslat.etkin && !g0.duraklat.etkin && !g0.surdur.etkin && !g0.adimla.etkin, 'yüklendi: başlat etkin, duraklat/sürdür/adımla etkin değil');
    // W5-K44: ekran durumu komuttan önce değişmedi
    dogrula(kanalDurum() === 'yuklendi', 'başlat öncesi ekran durumu yuklendi');

    // sıfırla yüklendi durumunda geçerlidir (A3-K67): yalnız durum_rev artar, run_id null kalır
    await adim('sifirla', {}, 'yuklendi');
    dogrula((await b2.http.durum()).run_id === null, 'sıfırla (yuklendi): run_id null');
    await adim('baslat', {}, 'kosuyor');
    dogrula(a3.dugmeler('kosuyor', 'sil').duraklat.etkin, 'koşuyor: duraklat etkin');
    const dKos = await b2.http.durum();
    dogrula(/^\d{8}-\d{6}-[0-9a-f]{6}$/.test(dKos.run_id ?? ''), `başlat: run_id atandı (${dKos.run_id})`);
    await adim('duraklat', {}, 'duraklatildi');
    const dDur = await b2.http.durum();
    dogrula(dDur.run_id === dKos.run_id, 'duraklat: aynı çalıştırma');
    await new Promise((coz) => setTimeout(coz, 250));
    const dDur2 = await b2.http.durum();
    dogrula(Number(dDur2.sim_t_ns) === Number(dDur.sim_t_ns), 'duraklatılmışken sim zamanı ilerlemedi');
    const gDur = a3.dugmeler('duraklatildi', 'sil');
    dogrula(gDur.surdur.etkin && gDur.adimla.etkin && !gDur.duraklat.etkin, 'duraklatıldı: sürdür ve adımla etkin, duraklat etkin değil');

    const t0 = Number(dDur2.sim_t_ns);
    const dAd = await adim('adimla', { periyot_sayisi: 40 }, null);
    await bekle(() => olay2.filter((o) => o.tur === 'calistirma_durumu_degisti' && o.ayrinti?.neden === 'adimla_tamamlandi').length >= 1, 20000, 'adımlama bitti');
    await senkron();
    const dAd2 = await b2.http.durum();
    dogrula(dAd2.durum === 'duraklatildi' && Number(dAd2.sim_t_ns) === t0 + 40 * 5e6, `adımla 40 periyot: sim zamanı tam 200 ms ilerledi, duraklatıldı (${dAd2.sim_t_ns})`);
    void dAd;

    await adim('surdur', {}, 'kosuyor');
    await adim('hiz', { carpan: 2 }, 'kosuyor');
    await bekle(() => b2.son('calistirma/durum')?.yuk.hedef_carpan === 2, 5000, 'hedef çarpan 2 (sunucudan)');
    // Sunucunun reddi görünür (Tablo 4.7a): duraklatilmis durumda `duraklat` "—" (geçersiz) → reddedildi gecersiz_durum.
    // Sunucunun kesin açık olduğu bu noktada denetlenir; durdur'dan sonra sunucuya istek atılmaz (aşağıya bak).
    const dRet0 = await b2.http.durum();
    await adim('duraklat', {}, 'duraklatildi');
    await senkron();
    const ret = await yon.gonder('duraklat');
    dogrula(ret.asama === 'reddedildi' && ret.hata_kodu === 'gecersiz_durum', `duraklatılmışken duraklat: reddedildi gecersiz_durum (${ret.asama} ${ret.hata_kodu})`);
    dogrula((await b2.http.durum()).durum === 'duraklatildi', 'reddedilen komut durumu değiştirmedi (duraklatildi)');
    void dRet0;
    await adim('surdur', {}, 'kosuyor');

    // durdur: ida_sim tek çalıştırma koşar ve çalıştırma durduruldu olunca düzgün kapanır (sim/sunucu/src/sunucu.cpp,
    // calistirma_durdur → a3_durdur_). Bu yüzden durdur'dan sonra sunucuya HTTP isteği (GET /durum, GET /komutlar)
    // atılmaz; sunucunun kapanma anına göre yarışır (W5-K43: hıza bağlı bekleme yok). Sonuç yalnız komut katmanının
    // komut_sonucu olayından ve kanalın son durumundan doğrulanır; kanal kapanmışsa bu kapanış olarak kabul edilir.
    await senkron();
    const rd = await yon.gonder('durdur');
    dogrula(rd.asama === 'uygulandi', `durdur: komut katmanı sonucu uygulandı (${rd.asama}${rd.hata_kodu ? ' ' + rd.hata_kodu : ''}; ${rd.ileti})`);
    dogrula(olay2.some((o) => o.tur === 'komut_sonucu' && o.ayrinti?.komut_no === rd.komut_no && o.ayrinti?.sonuc === 'uygulandi'),
      'durdur: komut_sonucu olayı uygulandi');
    await bekle(() => kanalDurum() === 'durduruldu' || b2.bilgi.durum !== 'bagli', 20000, `durdur: kanal durumu durduruldu ya da sunucu kapandı (şimdi ${kanalDurum()})`);
    dogrula(kanalDurum() === 'durduruldu' || b2.bilgi.durum !== 'bagli',
      `durdur: kanal son durumu durduruldu ya da bağlantı sunucu kapanışıyla düştü (${kanalDurum()}; ${b2.bilgi.durum})`);
    const gSon = a3.dugmeler('durduruldu', 'sil');
    dogrula(gSon.sifirla.etkin && !gSon.baslat.etkin && !gSon.duraklat.etkin, 'durduruldu: sıfırla etkin, başlat/duraklat etkin değil');
  } finally {
    yon?.kapat();
    b2?.kapat();
    sur2.kill('SIGTERM');
    await new Promise((coz) => { sur2.once('exit', coz); setTimeout(coz, 5000); });
    if (hatalar.length > 0) console.log(err2.split('\n').slice(-8).join('\n'));
  }
}

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
  // Kesin: yeniden deneme yok. Her komutta d, komuttan hemen önce okunan güncel anlık görüntüdür
  // (A3-K122: durum_rev komutla artar; anlık görüntü komut_sonucu olayından önce yenilenir).
  const kontrol = (ad, ek) => http.calistirma(ad, { run_id: d.run_id, durum_rev: d.durum_rev }, ek);
  dogrula(d.durum === 'kosuyor' && /^\d{8}-\d{6}-[0-9a-f]{6}$/.test(d.run_id ?? ''), `kosuyor, run_id ${d.run_id}`);
  // W4-duraklat (kapsam 6): hızdan bağımsız kurulum. SIL olabildiğince hızlı koşar; koşu görülür
  // görülmez hız 8 verilir (A3-K66; yalnız bekleme, bayt akışı değişmez) ki duraklatma koşu bitmeden
  // yerleşsin; sonra çalıştırma duraklatılır ve sim zamanı yalnız adımlamayla ilerler. ARM'ın sim
  // zamanı ve kart yanıtı duvar hızına bağlı değildir. Kanal değerleri adım numarasıyla eşlenir.
  const hiz = await kontrol('hiz', { carpan: 8 });
  await bekle(() => sonuc(hiz.komut_no) !== undefined, 5000, 'hız sonucu');
  dogrula(sonuc(hiz.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'hız 8 uygulandı');
  const revOnce = d.durum_rev;
  d = await http.durum();
  dogrula(d.durum_rev === revOnce + 1, `hız sonucundan hemen sonra GET /durum durum_rev = önceki + 1 (${revOnce} -> ${d.durum_rev}; A3-K122)`);
  const dur = await kontrol('duraklat');
  await bekle(() => sonuc(dur.komut_no) !== undefined, 5000, 'duraklat sonucu');
  dogrula(sonuc(dur.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'duraklat uygulandı (Tablo 4.7a)');
  d = await http.durum();
  dogrula(d.durum === 'duraklatildi', `duraklatildi (sim zamanı ${Number(d.sim_t_ns) / 1e9} s)`);
  await new Promise((coz) => setTimeout(coz, 300));
  const dDon = await http.durum();
  dogrula(dDon.durum === 'duraklatildi' && Number(dDon.sim_t_ns) === Number(d.sim_t_ns) && dDon.durum_rev === d.durum_rev,
    'duraklatılmışken sim zamanı ilerlemedi (300 ms)');
  let hataDur = null;
  try {
    await kontrol('duraklat');
  } catch (h) {
    hataDur = h;
  }
  dogrula(hataDur?.kod === 'gecersiz_durum', `duraklatılmışken duraklat: gecersiz_durum (${hataDur?.kod})`);
  // Adımla: sim zamanı 2 s'ye (en az bir periyot); her periyot 5 ms, dört fizik adımı (A3-K65).
  const adimlaBitti = () => olaylar.filter((o) => o.tur === 'calistirma_durumu_degisti' && o.ayrinti?.neden === 'adimla_tamamlandi').length;
  const n1 = Math.max(1, Math.min(10000, Math.ceil((2e9 - Number(d.sim_t_ns)) / 5e6)));
  const t0 = Number(d.sim_t_ns);
  const ad1 = await kontrol('adimla', { periyot_sayisi: n1 });
  await bekle(() => sonuc(ad1.komut_no) !== undefined && adimlaBitti() >= 1, 20000, 'adımlama bitti');
  d = await http.durum();
  dogrula(d.durum === 'duraklatildi' && Number(d.sim_t_ns) === t0 + n1 * 5e6,
    `adımla ${n1} periyot: sim zamanı tam ${n1} · 5 ms ilerledi, yeniden duraklatıldı (${Number(d.sim_t_ns) / 1e9} s)`);

  // ARM (genişletici; onay jetonu ve emniyet revizyonu: A3-K103, K118) duraklatılmışken: A3-K58'e göre
  // beklemeden uygulanır (uygulama adımı o anki adım); karta sürdürmeden sonra gider (A1-K41).
  const adimDurak = Number(d.adim_no);
  const ist = { 'Ida-Istemci': String(istemci) };
  const jeton = await http.istek('POST', '/yer_istasyonu/onay_jetonu', { beklenen_run_id: d.run_id, komut: 'arm', parametreler: {} }, ist);
  const arm = await http.komut('/yer_istasyonu/komut', {
    beklenen_run_id: d.run_id, komut: 'arm', parametreler: {}, onay: true, onay_jetonu: jeton.onay_jetonu,
    beklenen_emniyet_rev: jeton.emniyet_rev,
  });
  dogrula(arm.sonuc === 'kuyrukta', 'ARM 202 kuyrukta');
  await bekle(() => sonuc(arm.komut_no) !== undefined, 5000, 'ARM komut_sonucu');
  dogrula(sonuc(arm.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'ARM ağ geçidinde duraklatılmışken uygulandı (A3-K58)');
  dogrula(Number(sonuc(arm.komut_no)?.ayrinti?.uygulama_adimi) === adimDurak,
    `ARM uygulama adımı duraklatılan sınır (${sonuc(arm.komut_no)?.ayrinti?.uygulama_adimi} / ${adimDurak})`);
  const yiKomut = () => olaylar.find((o) => o.tur === 'yer_istasyonu_komutu' && o.ayrinti?.komut_no === arm.komut_no);
  await bekle(() => yiKomut() !== undefined, 5000, 'yer_istasyonu_komutu');
  dogrula(yiKomut()?.ayrinti?.durum === 'karta_iletildi', `yer_istasyonu_komutu: ${yiKomut()?.ayrinti?.durum}`);
  const yanit = () => olaylar.find((o) => o.tur === 'yer_istasyonu_yaniti' && o.ayrinti?.komut_no === arm.komut_no);
  await new Promise((coz) => setTimeout(coz, 200));
  dogrula(yanit() === undefined, 'duraklatılmışken kart yanıtı yok (komut tele çıkmadı; A1-K41)');
  d = await http.durum();
  const ad2 = await kontrol('adimla', { periyot_sayisi: 200 });
  await bekle(() => sonuc(ad2.komut_no) !== undefined, 5000, 'adımla (2) sonucu');
  await bekle(() => yanit() !== undefined, 10000, 'ARM kart yanıtı (yer_istasyonu_yaniti)');
  const y = yanit().ayrinti;
  console.log(`  ARM kart yanıtı: ${y.sonuc} (ack ${y.veri?.ack}, reason ${y.veri?.reason})`);
  dogrula(['kabul', 'ret'].includes(y.sonuc) && typeof y.veri?.ack === 'string', 'ARM yanıtı kartın sonucunu taşır');
  if (y.sonuc === 'kabul') {
    await bekle(() => kart.length > 0 && kart[kart.length - 1].silahli === 1, 5000, 'kart/durum silahlı');
    dogrula(true, 'kart/durum: silahli = 1');
  }
  await bekle(() => adimlaBitti() >= 2, 20000, 'adımlama (2) bitti');
  d = await http.durum();
  dogrula(d.durum === 'duraklatildi', 'adımlamadan sonra yeniden duraklatıldı');
  const sur = await kontrol('surdur');
  await bekle(() => sonuc(sur.komut_no) !== undefined, 5000, 'sürdür sonucu');
  dogrula(sonuc(sur.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'sürdür uygulandı');
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
  await panelKomutZinciri();
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
