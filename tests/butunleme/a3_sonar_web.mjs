// W4-bagla-sonar kabul 5: `ida_sim --senaryo scenarios/sonar-havuz.toml --sonar --a3-port 0 --a3-bekle`
// + panelin A3 istemcisi (web/src/a3; Node'un WebSocket ve fetch'i; web/'e dosya eklenmez).
// Sahne (W4-sonar-sahne): depodaki senaryo olduğu gibi, A7 kümesi `havuz` (havuz duvarları ağdan).
// `sonar/ping360` ve `sonar/ping1d` kanallarında çözülen her değer, sim'in aynı kayıtları yazdığı
// sonar CSV'siyle (`--sonar-csv`: karta giden ölçümün kendisi) alan alan ve örnek baytlarının
// CRC-32'siyle eşleşir.
// Kullanım: node a3_sonar_web.mjs <ida_sim> <depo kökü> <web dizini> <çıktı dizini>
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

// CRC-32/ISO-HDLC (sim'in `kopru::crc32_iso_hdlc`'si ile aynı).
const CRC_TABLO = new Uint32Array(256).map((_, n) => {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});
const crc32 = (b) => {
  let c = 0xffffffff;
  for (const x of b) c = CRC_TABLO[(c ^ x) & 0xff] ^ (c >>> 8);
  return ((c ^ 0xffffffff) >>> 0).toString(16).padStart(8, '0');
};

mkdirSync(cikti, { recursive: true });
const sonarCsv = join(cikti, 'a3_sonar_web.sonar.csv');
const surec = spawn(idaSim, ['--senaryo', 'scenarios/sonar-havuz.toml', '--kok', kok, '--sonar', '--a3-port', '0',
  '--a3-bekle', '--sonar-csv', sonarCsv, '--gunluk', '1'], { stdio: ['ignore', 'pipe', 'pipe'] });
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
  const p360 = [];
  const p1d = [];
  const durum = [];
  b.abone('sonar/ping360', (c) => p360.push({ t: c.baslik.t_ns, y: c.yuk, dusme: c.baslik.bayraklar }));
  b.abone('sonar/ping1d', (c) => p1d.push({ t: c.baslik.t_ns, y: c.yuk }), 50);
  b.abone('calistirma/durum', (c) => durum.push(c.yuk));
  const olaylar = [];
  b.olaylar((o) => olaylar.push(o));
  b.baslat();
  await bekle(() => b.bilgi.durum === 'bagli', 5000, 'bağlantı');
  // calistirma/durum zorunlu aboneliktir: istemci onu abone_ol'a koymaz; yanıtlanan kanallar ping360 ve ping1d (2).
  await bekle(() => b.abonelikSonuclari.has('sonar/ping360') && b.abonelikSonuclari.has('sonar/ping1d'), 3000, 'abonelik yanıtları');
  for (const [k, y] of b.abonelikSonuclari) dogrula(y.sonuc === 'kabul', `abonelik kabul: ${k}`);
  const http = b.http;
  const sonuc = (no) => olaylar.find((o) => o.tur === 'komut_sonucu' && o.ayrinti?.komut_no === no);
  const d0 = await http.durum();
  const baslat = await http.calistirma('baslat', { run_id: null, durum_rev: d0.durum_rev });
  await bekle(() => sonuc(baslat.komut_no) !== undefined, 30000, 'baslat sonucu');
  dogrula(sonuc(baslat.komut_no)?.ayrinti?.sonuc === 'uygulandi', 'baslat uygulandı');
  await bekle(() => olaylar.some((o) => o.tur === 'calistirma_durumu_degisti' && o.ayrinti?.sonraki === 'bitti'), 120000, 'bitti');
  const kod = await bitti;
  dogrula(kod === 0, `ida_sim çıkış kodu 0 (${kod})`);
  b.kapat();
  dogrula(durum.some((d) => d.kademe_sonar === 1), 'calistirma/durum kademe_sonar = 1 (sade)');

  // Sim'in sonar CSV'si: karta giden kayıtların kendisi (arıza zinciri sonrası).
  const csv = readFileSync(sonarCsv, 'utf8').trim().split('\n');
  const baslik = csv[0].split(',');
  const kayit = new Map();
  for (const s of csv.slice(1)) {
    const a = s.split(',');
    const r = Object.fromEntries(baslik.map((k, i) => [k, a[i]]));
    kayit.set(`${r.tur}/${r.sensor_id}/${r.seq}`, r);
  }
  let p360Esit = 0;
  for (const { t, y } of p360) {
    const r = kayit.get(`ping360/${y.sensor_id}/${y.seq}`);
    const esit = r !== undefined && t === BigInt(r.t_meas_ns) && y.aci_grad === Number(r.aci_grad) &&
      y.n_ornek === Number(r.n_ornek) && y.ornekler.length === Number(r.veri_n) &&
      y.ornek_periyodu === Number(r.ornek_periyodu) && y.darbe_suresi_us === Number(r.darbe_suresi_us) &&
      y.frekans_khz === Number(r.frekans_khz) && y.kazanc === Number(r.kazanc) && y.flags === Number(r.flags) &&
      y.kademe === 1 && crc32(y.ornekler) === r.veri_crc32 && y.ses_hizi_mps > 1400 && y.ses_hizi_mps < 1600;
    if (esit) p360Esit += 1;
    else if (hatalar.length < 5) hatalar.push(`ping360 seq ${y.seq} CSV ile eşleşmedi`);
  }
  const csvP360 = [...kayit.values()].filter((r) => r.tur === 'ping360').length;
  dogrula(p360.length > 100 && p360Esit === p360.length,
    `sonar/ping360: ${p360Esit}/${p360.length} satır CSV ile alan alan ve CRC-32 ile aynı (CSV'de ${csvP360})`);
  let p1dEsit = 0;
  for (const { t, y } of p1d) {
    const r = kayit.get(`ping1d/${y.sensor_id}/${y.seq}`);
    const gecerli = r !== undefined && (Number(r.flags) & 1) === 1;
    const esit = r !== undefined && t === BigInt(r.t_meas_ns) - BigInt(r.tx_offset_ns) &&
      (gecerli ? y.mesafe_m === Math.fround(Number(r.mesafe_m)) : y.mesafe_m === null) &&
      Math.abs(y.guven - Number(r.guven) / 100) <= 1e-7 && y.kademe === 1 &&
      y.profil.length === Number(r.veri_n) && crc32(y.profil) === r.veri_crc32 && y.flags === Number(r.flags);
    if (esit) p1dEsit += 1;
    else if (hatalar.length < 10) hatalar.push(`ping1d seq ${y.seq} CSV ile eşleşmedi (${JSON.stringify({ m: y.mesafe_m, g: y.guven, t: String(t) })}, ${JSON.stringify(r)})`);
  }
  dogrula(p1d.length > 20 && p1dEsit === p1d.length, `sonar/ping1d: ${p1dEsit}/${p1d.length} kayıt CSV ile aynı (gönderim anı, mesafe, güven, profil)`);
  console.log(`sonar uçtan uca: ping360 ${p360.length}, ping1d ${p1d.length}`);
  cikisKodu = hatalar.length === 0 ? 0 : 1;
} catch (h) {
  hatalar.push(String(h?.stack ?? h));
} finally {
  surec.kill('SIGTERM');
}
for (const h of hatalar) console.log('  KALDI', h);
if (hatalar.length > 0) console.log(stderr.split('\n').slice(-15).join('\n'));
console.log(`a3_sonar_web: ${hatalar.length === 0 ? 'GEÇTİ' : 'KALDI'}`);
process.exit(hatalar.length === 0 ? cikisKodu : 1);
