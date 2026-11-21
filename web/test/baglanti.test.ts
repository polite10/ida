// Kabul 3 ve 5: gerçek WebSocket üzerinden sahte sunucu ↔ istemci kitaplığı.
// Erişim koruması, abonelik, bütün kanalların çözülmesi, kopma ve kendiliğinden
// toparlanma, olay boşluğunun HTTP'den doldurulması.

import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { after, test } from 'node:test';
import { sahteSunucuBaslat, type SahteSunucu } from '../arac/sahte-a3/sunucu';
import { A3Baglanti, type BaglantiBilgisi } from '../src/a3/baglanti';
import { KANALLAR, type KanalAdi } from '../src/a3/katalog';
import type { OlayKaydi } from '../src/a3/olay';

const KATALOG = resolve(process.cwd(), '..', 'docs', 'sozlesmeler', 'sema', 'a3-kanallar.json');
const JETON = 'b'.repeat(64);
const bekle = (ms: number): Promise<void> => new Promise((r) => setTimeout(r, ms));

async function kosul(f: () => boolean, ms: number, ne: string): Promise<void> {
  const son = Date.now() + ms;
  while (Date.now() < son) {
    if (f()) return;
    await bekle(25);
  }
  assert.fail(`zaman aşımı: ${ne}`);
}

const acik: (SahteSunucu | A3Baglanti)[] = [];
after(async () => {
  for (const x of acik) {
    if (x instanceof A3Baglanti) x.kapat();
    else await x.kapat();
  }
});

test('erişim koruması: jeton, Host, Origin, OPTIONS, CORS (A3-K06…K10)', async () => {
  const s = await sahteSunucuBaslat({ port: 0, jeton: JETON, katalogYolu: KATALOG, statik: null });
  acik.push(s);
  const u = `http://127.0.0.1:${s.port}/api/v1/surum`;
  assert.equal((await fetch(u)).status, 403);
  assert.equal((await fetch(u, { headers: { Authorization: 'Bearer ' + 'c'.repeat(64) } })).status, 403);
  const iyi = await fetch(u, { headers: { Authorization: 'Bearer ' + JETON } });
  assert.equal(iyi.status, 200);
  assert.equal(iyi.headers.get('access-control-allow-origin'), null);
  const y = (await iyi.json()) as { a3_surumu: string; katalog_ozeti: string };
  assert.equal(y.a3_surumu, '1.0');
  assert.match(y.katalog_ozeti, /^sha256:[0-9a-f]{64}$/);
  assert.equal((await fetch(u, { headers: { Authorization: 'Bearer ' + JETON, Origin: 'http://kotu.example' } })).status, 403);
  assert.equal((await fetch(u, { method: 'OPTIONS', headers: { Authorization: 'Bearer ' + JETON } })).status, 405);
  const ret = (await (await fetch(u)).json()) as { sonuc: string; hata: { kod: string } };
  assert.equal(ret.sonuc, 'ret');
  assert.equal(ret.hata.kod, 'erisim_reddedildi');
  // A3-K07: alt protokolde jeton yoksa el sıkışma reddedilir
  const ws = new WebSocket(`ws://127.0.0.1:${s.port}/ws`, ['ida-a3.v1']);
  const sonuc = await new Promise<string>((r) => {
    ws.onopen = () => r('acildi');
    ws.onerror = () => r('hata');
  });
  assert.equal(sonuc, 'hata');
  // Yanlış jetonlu istemci kitaplığı: erişim reddedildi, yeniden denemez
  const b = new A3Baglanti({ taban: `http://127.0.0.1:${s.port}`, jeton: 'c'.repeat(64) });
  acik.push(b);
  b.baslat();
  await kosul(() => b.bilgi.durum === 'erisim_reddedildi', 3000, 'erisim_reddedildi');
});

test('bağlantı, abonelik ve bütün kanalların çözülmesi; kopma ve kendiliğinden toparlanma', async () => {
  let s = await sahteSunucuBaslat({ port: 0, jeton: JETON, katalogYolu: KATALOG, statik: null, kip: 'hil_1x', atla: 95 });
  const port = s.port;
  const b = new A3Baglanti({ taban: `http://127.0.0.1:${port}`, jeton: JETON, ilkGecikmeMs: 100, azamiGecikmeMs: 400 });
  acik.push(b);
  const durumlar: BaglantiBilgisi['durum'][] = [];
  b.durumDinle((x) => {
    if (durumlar.at(-1) !== x.durum) durumlar.push(x.durum);
  });
  const sayac = new Map<string, number>();
  for (const k of KANALLAR) {
    b.abone(k.ad as KanalAdi, () => sayac.set(k.ad, (sayac.get(k.ad) ?? 0) + 1), k.sinif === 'durum' ? 50 : undefined);
  }
  const olaylar: OlayKaydi[] = [];
  b.olaylar((o) => olaylar.push(o));
  b.baslat();
  await kosul(() => b.bilgi.durum === 'bagli', 3000, 'bagli');
  assert.equal(b.bilgi.merhaba?.a3_surumu, '1.0');
  assert.deepEqual(b.bilgi.uyarilar, [], 'katalog özeti istemcinin derlendiği katalogla aynı olmalı');
  await bekle(2500);
  // abonelik yanıtları: hz 50 isteyen durum kanalında etkin hız min(50, azami, tavan)
  assert.equal(b.abonelikSonuclari.get('fizik/durum')?.etkin_hz, 50);
  assert.equal(b.abonelikSonuclari.get('ariza/etkin')?.etkin_hz, 2);
  assert.equal(b.abonelikSonuclari.get('sonar/ping360')?.etkin_hz, null);
  for (const k of ['calistirma/durum', 'sayac/gercek_zaman', 'fizik/durum', 'kart/durum', 'yer_istasyonu/durum', 'atis/durum', 'emniyet/girdiler', 'sonar/ping360', 'sonar/ping1d', 'sensor/imu', 'sensor/basinc', 'baglanti/durum']) {
    assert.ok((sayac.get(k) ?? 0) > 0, `${k} gelmedi`);
  }
  // 50 Hz fizik yaklaşık 2,5 s'de ~125 çerçeve; abone olunmayan bir kanal gelmez kuralı ayrı sınanır
  assert.ok((sayac.get('fizik/durum') ?? 0) > 60);
  assert.equal(b.cozucu.sayaclar.istemci_bozuk_cerceve, 0);
  assert.equal(b.cozucu.sayaclar.istemci_gecersiz_alan, 0);
  assert.equal(b.cozucu.sayaclar.istemci_bilinmeyen_kanal, 0);

  // ---- Kopma ----
  const kopmaOncesi = olaylar.length;
  await s.kapat();
  await kosul(() => b.bilgi.durum === 'kopuk', 3000, 'kopuk');
  assert.equal(b.bilgi.kapanisKodu, 1001);
  // ---- Sunucu döner (aynı port ve geliştirme jetonu) ----
  s = await sahteSunucuBaslat({ port, jeton: JETON, katalogYolu: KATALOG, statik: null, kip: 'hil_1x' });
  acik.push(s);
  await kosul(() => b.bilgi.durum === 'bagli', 5000, 'yeniden bagli');
  assert.equal(b.bilgi.yenidenBaglanma, 1);
  const once = sayac.get('fizik/durum') ?? 0;
  await bekle(3500);
  assert.ok((sayac.get('fizik/durum') ?? 0) > once + 20, 'toparlanınca veri akmalı');
  // Yeni süreçte olay sırası 1'den başlar (A3-K27); kopmadan SONRA eklenen olaylar
  // yeni süreçten gelmeli, 1'den başlayıp boşluksuz artmalı.
  const yeni = olaylar.slice(kopmaOncesi);
  assert.ok(yeni.length >= 2, `yeni süreçten olay gelmedi (${yeni.length})`);
  assert.equal(yeni[0]!.sira, 1, 'yeni sürecin ilk olayı');
  for (let i = 1; i < yeni.length; i++) assert.equal(yeni[i]!.sira, yeni[i - 1]!.sira + 1);
  assert.ok(yeni.some((o) => o.tur === 'calistirma_durumu_degisti'));
  assert.ok(durumlar.includes('kopuk') && durumlar.at(-1) === 'bagli');
});

test('büyük sürüm farkında bağlanmaz, küçük sürüm farkında uyarıyla bağlanır (A3-K15)', async () => {
  const sahte = (surum: string): typeof fetch =>
    (async () =>
      new Response(
        JSON.stringify({ sonuc: 'kabul', a3_surumu: surum, katalog_ozeti: 'sha256:' + '0'.repeat(64), ws_alt_protokolu: 'ida-a3.v1', a6_surumu: '1.0', sim_surumu: 't', platform: 'macos-arm64' }),
        { status: 200, headers: { 'Content-Type': 'application/json' } },
      )) as typeof fetch;
  const b = new A3Baglanti({ taban: 'http://127.0.0.1:1', jeton: JETON, fetchIslevi: sahte('2.0') });
  acik.push(b);
  b.baslat();
  await kosul(() => b.bilgi.durum === 'surum_uyusmazligi', 2000, 'surum_uyusmazligi');

  class SessizWS {
    binaryType = '';
    readyState = 0;
    onmessage: unknown = null;
    onclose: unknown = null;
    onerror: unknown = null;
    close(): void {}
    send(): void {}
  }
  const k = new A3Baglanti({ taban: 'http://127.0.0.1:1', jeton: JETON, fetchIslevi: sahte('1.7'), WebSocketSinifi: SessizWS as unknown as typeof WebSocket });
  acik.push(k);
  k.baslat();
  await kosul(() => k.bilgi.uyarilar.length === 2, 2000, 'küçük sürüm ve katalog uyarıları');
  assert.notEqual(k.bilgi.durum, 'surum_uyusmazligi');
});

function siraDenetle(olaylar: OlayKaydi[]): void {
  const siralar = olaylar.map((o) => o.sira);
  for (let i = 1; i < siralar.length; i++) assert.equal(siralar[i], siralar[i - 1]! + 1, `olay sırası boşluksuz ve yinelenmesiz: ${siralar.join(',')}`);
}

test('ilk bağlantıda geçmiş olaylar HTTP\'den yüklenir (A3-K45)', async () => {
  const s = await sahteSunucuBaslat({ port: 0, jeton: JETON, katalogYolu: KATALOG, statik: null });
  acik.push(s);
  await bekle(2300); // sahte sunucu koşuyu başlatsın: hazirlaniyor, ön denetim, kosuyor
  const b = new A3Baglanti({ taban: `http://127.0.0.1:${s.port}`, jeton: JETON, gecmisOlay: 50 });
  acik.push(b);
  const olaylar: OlayKaydi[] = [];
  b.olaylar((o) => olaylar.push(o));
  b.baslat();
  await kosul(() => olaylar.length >= 3, 3000, 'geçmiş olaylar');
  assert.ok(olaylar.slice(0, 3).every((o) => o.gecmisten));
  assert.ok(olaylar.some((o) => o.tur === 'on_denetim_sonucu'));
  siraDenetle(olaylar);
});

test('kopukken üretilen olaylar yeniden bağlanınca HTTP\'den doldurulur, sıra boşluksuz (A3-K45)', async () => {
  // 98,6 s'ye atla: 100 s'de senaryonun emniyet girdisi olayları gelir (kopukluk penceresinin içinde)
  const s = await sahteSunucuBaslat({ port: 0, jeton: JETON, katalogYolu: KATALOG, statik: null, atla: 98.6 });
  acik.push(s);
  const b = new A3Baglanti({ taban: `http://127.0.0.1:${s.port}`, jeton: JETON, ilkGecikmeMs: 2500, azamiGecikmeMs: 2500 });
  acik.push(b);
  const olaylar: OlayKaydi[] = [];
  b.olaylar((o) => olaylar.push(o));
  b.baslat();
  await kosul(() => b.bilgi.durum === 'bagli', 3000, 'bagli');
  s.baglantilariKopar(); // ağ kopması: sunucu süreci sürer, olay üretir
  await kosul(() => b.bilgi.durum === 'kopuk', 2000, 'kopuk');
  await kosul(() => b.bilgi.durum === 'bagli', 6000, 'yeniden bagli');
  await kosul(() => olaylar.some((o) => o.tur === 'emniyet_girdisi_degisti'), 4000, 'kopukken üretilen emniyet olayı');
  const e = olaylar.find((o) => o.tur === 'emniyet_girdisi_degisti')!;
  assert.equal(e.gecmisten, true, 'kopukken üretilen olay HTTP geçmişinden gelmeli');
  assert.equal(e.onem, 'emniyet');
  siraDenetle(olaylar);
});
