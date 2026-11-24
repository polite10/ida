// W4-a3 kabul 3: ida_a3_sahte + panelin A3 istemcisi (web/src/a3, Node'un WebSocket ve fetch'i):
// sürüm → WebSocket → merhaba → abonelik → beş panelin kanalları → komut gönder/sonuç al.
// Kullanım: node uctan_uca.mjs <ida_a3_sahte> <web dizini> <çıktı dizini>
// Çıkış: 0 geçti, 1 kaldı, 77 atlandı (web/node_modules yok).

import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { ATLA, a3Kitapligi, bekle } from './ortak.mjs';

const [sahte, web, cikti] = process.argv.slice(2);
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
};

// Sunucu: port 0 (işletim sistemi atar); açılış adresi standart çıktıda bir kez (A3-K05).
const surec = spawn(sahte, ['--port', '0'], { stdio: ['ignore', 'pipe', 'inherit'] });
const satirlar = createInterface({ input: surec.stdout });
const adres = await new Promise((coz, red) => {
  const t = setTimeout(() => red(new Error('açılış adresi gelmedi')), 5000);
  satirlar.once('line', (s) => {
    clearTimeout(t);
    coz(s.trim());
  });
});
const m = /^http:\/\/127\.0\.0\.1:(\d+)\/#erisim=([0-9a-f]{64})$/.exec(adres);
dogrula(m !== null, `açılış adresi biçimi: ${adres.replace(/erisim=.*/, 'erisim=…')}`);
let cikisKodu = 1;
try {
  const taban = `http://127.0.0.1:${m[1]}`;
  const b = new a3.A3Baglanti({ taban, jeton: m[2], gecmisOlay: 100 });
  const ilk = new Map();
  const say = new Map();
  const PANEL_KANALLARI = [
    'calistirma/durum', 'sayac/gercek_zaman', 'fizik/durum', 'kart/durum', 'atis/durum', 'emniyet/girdiler',
    'ariza/etkin', 'sensor/basinc', 'sensor/dvl', 'sensor/batarya', 'sonar/ping360', 'sonar/ping1d',
    'baglanti/durum', 'kart/cekirdek_sayaclari',
  ];
  for (const k of PANEL_KANALLARI) {
    b.abone(k, (c) => {
      if (!ilk.has(k)) ilk.set(k, c);
      say.set(k, (say.get(k) ?? 0) + 1);
    }, k === 'fizik/durum' || k === 'kart/durum' ? 25 : undefined);
  }
  const olaylar = [];
  b.olaylar((o) => olaylar.push(o));
  b.baslat();
  await bekle(() => b.bilgi.durum === 'bagli', 5000, 'bağlantı');
  dogrula(b.bilgi.surum?.a3_surumu === '1.0' && b.bilgi.surum?.ws_alt_protokolu === 'ida-a3.v1', 'GET /surum');
  dogrula(b.bilgi.uyarilar.length === 0, `uyarı yok (katalog özeti aynı): ${b.bilgi.uyarilar.join('; ')}`);
  dogrula(b.bilgi.merhaba?.tur === 'merhaba' && b.bilgi.merhaba.yayin_tavani_hz === 50, 'merhaba');
  await bekle(() => b.abonelikSonuclari.size >= PANEL_KANALLARI.length - 2, 3000, 'abonelik yanıtları');
  for (const [k, y] of b.abonelikSonuclari) dogrula(y.sonuc === 'kabul', `abonelik kabul: ${k}`);

  // Komut: çalıştırmayı başlat (A3-K50…K57)
  const http = b.http;
  await http.istemciKaydet('uctan_uca');
  const d0 = await http.durum();
  const baslat = await http.komut('/calistirma/baslat', { beklenen_run_id: d0.run_id, beklenen_durum_rev: d0.durum_rev });
  dogrula(baslat.sonuc === 'kuyrukta' && baslat.komut_no >= 1, 'baslat 202 kuyrukta');
  const sonuc = (no) => olaylar.find((o) => o.tur === 'komut_sonucu' && o.ayrinti?.komut_no === no);
  await bekle(() => sonuc(baslat.komut_no) !== undefined, 3000, 'baslat komut_sonucu');
  dogrula(sonuc(baslat.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'baslat uygulandı');
  await bekle(() => PANEL_KANALLARI.every((k) => ilk.has(k)), 5000, `beş panelin kanalları: eksik ${PANEL_KANALLARI.filter((k) => !ilk.has(k)).join(', ')}`);
  const d1 = await http.durum();
  dogrula(d1.durum === 'kosuyor' && typeof d1.run_id === 'string', 'kosuyor');
  const fizik = b.son('fizik/durum');
  dogrula(fizik !== undefined && fizik.yuk.govdeler.length === 2 && fizik.yuk.iticiler.length === 6, 'fizik/durum çözüldü');
  dogrula((fizik.baslik.bayraklar & a3.BAYRAK.YER_GERCEGI) !== 0, 'fizik/durum yer_gercegi bayrağı');
  const ping = b.son('sonar/ping360');
  dogrula(ping !== undefined && ping.yuk.ornekler.length === ping.yuk.n_ornek, 'Ping360 satırı');

  // EMNİYET: kısıtlayan tek adım (A3-K92)
  const acil = await http.komut('/emniyet/degistir', { beklenen_run_id: d1.run_id, girdi: 'acil_durdurma', hedef_durum: 'basili' });
  await bekle(() => sonuc(acil.komut_no) !== undefined, 3000, 'acil durdurma komut_sonucu');
  dogrula(sonuc(acil.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'acil durdurma uygulandı');
  await bekle(() => olaylar.some((o) => o.tur === 'emniyet_girdisi_degisti' && o.ayrinti?.sonraki === 'basili'), 3000, 'emniyet_girdisi_degisti');
  await bekle(() => b.son('emniyet/girdiler')?.yuk.acil_durdurma === 1, 3000, 'emniyet/girdiler basılı');
  const rev = b.son('emniyet/girdiler').yuk.emniyet_rev;

  // EMNİYET: gevşeten iki adım; revizyonsuz reddedilir (A3-K93, K118)
  const ist = { 'Ida-Istemci': String(await http.istemciKaydet('uctan_uca_2')) };
  const jeton1 = await http.istek('POST', '/emniyet/onay_jetonu', { beklenen_run_id: d1.run_id, girdi: 'acil_durdurma', hedef_durum: 'serbest' }, ist);
  let red = null;
  try {
    await http.komut('/emniyet/degistir', { beklenen_run_id: d1.run_id, girdi: 'acil_durdurma', hedef_durum: 'serbest', onay: true, onay_jetonu: jeton1.onay_jetonu });
  } catch (h) {
    red = h;
  }
  dogrula(red?.kod === 'gecersiz_istek' && red?.http === 400, `revizyonsuz gevşetme 400: ${red?.kod}`);
  const jeton2 = await http.istek('POST', '/emniyet/onay_jetonu', { beklenen_run_id: d1.run_id, girdi: 'acil_durdurma', hedef_durum: 'serbest' }, ist);
  const serbest = await http.komut('/emniyet/degistir', {
    beklenen_run_id: d1.run_id, girdi: 'acil_durdurma', hedef_durum: 'serbest', onay: true,
    onay_jetonu: jeton2.onay_jetonu, beklenen_emniyet_rev: jeton2.emniyet_rev,
  });
  dogrula(jeton2.emniyet_rev === rev, 'jetonun revizyonu yayınlanan revizyon');
  await bekle(() => sonuc(serbest.komut_no) !== undefined, 3000, 'serbest komut_sonucu');
  dogrula(sonuc(serbest.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'gevşetme uygulandı');
  await bekle(() => b.son('emniyet/girdiler')?.yuk.acil_durdurma === 0, 3000, 'emniyet/girdiler serbest');

  // Olay geçmişi (A3-K45) ve kapanış (A3-K49)
  dogrula(b.bilgi.olayBosluklari.length === 0, 'olay boşluğu yok');
  const sira = olaylar.filter((o) => !o.gecmisten).map((o) => o.sira);
  dogrula(sira.every((s, i) => i === 0 || s === sira[i - 1] + 1), 'canlı olay sırası boşluksuz');
  surec.kill('SIGTERM');
  await bekle(() => b.bilgi.kapanisKodu !== null, 5000, 'kapanış');
  dogrula(b.bilgi.kapanisKodu === 1001 || process.platform === 'win32', `kapanış kodu 1001: ${b.bilgi.kapanisKodu}`);
  b.kapat();
  console.log(`uçtan uca: ${[...say.entries()].map(([k, n]) => `${k} ${n}`).join(', ')}`);
  cikisKodu = hatalar.length === 0 ? 0 : 1;
} catch (h) {
  hatalar.push(String(h?.stack ?? h));
} finally {
  surec.kill('SIGTERM');
}
for (const h of hatalar) console.log('  KALDI', h);
console.log(`uctan_uca_web: ${hatalar.length === 0 ? 'GEÇTİ' : 'KALDI'}`);
process.exit(hatalar.length === 0 ? cikisKodu : 1);
