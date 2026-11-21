// sol incelemesi (docs/gorevler/inceleme/W3-web-sol.md) bulgularının karşı örnekleri.
// Her test, bulgunun karşı örneğini bağımsız baytlarla ya da sahte ortamla kurar.

import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { after, test } from 'node:test';
import { sahteSunucuBaslat, type SahteSunucu } from '../arac/sahte-a3/sunucu';
import { A3Baglanti } from '../src/a3/baglanti';
import { BAYRAK, Cozucu } from '../src/a3/cerceve';
import { kanalBul } from '../src/a3/cerceve';
import type { YukAtisDurumTuplerOge, YukEmniyetGirdiler, YukOlay } from '../src/a3/katalog';
import { cerceveKodla, yukKodla } from '../src/a3/kodlayici';
import type { OlayKaydi } from '../src/a3/olay';
import { aracNedHizi, eulerdenKuaterniyonTest } from './yardimci-inceleme';
import { girdiGosterimi, tupGosterimi } from '../src/ortak/emniyet';
import { Tampon } from '../src/paneller/grafik-tampon';
import { menzilHalkalari, ping360Menzil } from '../src/paneller/sonar-hesap';
import { Bayt, birlestir, cerceve } from './yardimci';

const KATALOG = resolve(process.cwd(), '..', 'docs', 'sozlesmeler', 'sema', 'a3-kanallar.json');
const JETON = 'd'.repeat(64);
const bekle = (ms: number): Promise<void> => new Promise((r) => setTimeout(r, ms));
async function kosul(f: () => boolean, ms: number, ne: string): Promise<void> {
  const son = Date.now() + ms;
  while (Date.now() < son) {
    if (f()) return;
    await bekle(15);
  }
  assert.fail(`zaman aşımı: ${ne}`);
}
const kapanacak: (() => unknown)[] = [];
after(async () => {
  for (const f of kapanacak) await f();
});

// ---------------------------------------------------------------- 1–3 emniyet

function emniyetBaytlari(o: { acil?: number; fis?: [number, number]; pim?: [number, number]; kAcil?: number; kFis?: [number, number]; kPim?: [number, number]; fark?: number }): Uint8Array {
  const b = new Bayt().u8(o.acil ?? 0);
  for (const v of o.fis ?? [0, 0]) b.u8(v);
  for (const v of o.pim ?? [1, 1]) b.u8(v);
  b.u64(0xffffffffffffffffn).u8(0).u8(o.kAcil ?? 0);
  for (const v of o.kFis ?? [0, 0]) b.u8(v);
  for (const v of o.kPim ?? [1, 1]) b.u8(v);
  return b.u8(o.fark ?? 0).u32(1).u8(0).bitir();
}

function emniyetCoz(baytlar: Uint8Array): { y: YukEmniyetGirdiler; gecersiz: readonly string[] } {
  const c = new Cozucu().ileti(cerceve(1n, 1, 14, baytlar));
  assert.equal(c.length, 1);
  return { y: c[0]!.yuk as YukEmniyetGirdiler, gecersiz: c[0]!.gecersiz };
}

test('B1: kartın beş geri okuması bilinmiyorsa (255) özet "eşit" değil "bilinmiyor"', () => {
  const { y, gecersiz } = emniyetCoz(emniyetBaytlari({ kAcil: 255, kFis: [255, 255], kPim: [255, 255], fark: 0 }));
  const g = girdiGosterimi(y, gecersiz);
  assert.equal(g.ozet.durum, 'bilinmiyor');
  assert.notEqual(g.ozet.sinif, 'iyi');
  assert.ok(g.satirlar.every((s) => s.fark === 'bilinmiyor'));
});

test('B1: tek bir girdinin geri okuması bilinmiyorsa da özet "eşit" değil', () => {
  const { y, gecersiz } = emniyetCoz(emniyetBaytlari({ kPim: [1, 255] }));
  const g = girdiGosterimi(y, gecersiz);
  assert.equal(g.ozet.durum, 'bilinmiyor');
  assert.equal(g.satirlar[4]!.fark, 'bilinmiyor');
  assert.equal(g.satirlar[0]!.fark, 'esit');
});

test('B1: bütün girdiler bilinen ve eşitse "eşit"; fark biti varsa "fark"', () => {
  const esit = girdiGosterimi(...Object.values(emniyetCoz(emniyetBaytlari({}))) as [YukEmniyetGirdiler, string[]]);
  assert.equal(esit.ozet.durum, 'esit');
  assert.equal(esit.ozet.sinif, 'iyi');
  const fark = girdiGosterimi(...Object.values(emniyetCoz(emniyetBaytlari({ pim: [1, 1], kPim: [1, 0], fark: 0b10000 }))) as [YukEmniyetGirdiler, string[]]);
  assert.equal(fark.ozet.durum, 'fark');
  assert.equal(fark.satirlar[4]!.fark, 'fark');
});

test('B2: değer listesi dışı emniyet değerleri olağan duruma çevrilmez, "geçersiz" gösterilir', () => {
  const { y, gecersiz } = emniyetCoz(emniyetBaytlari({ acil: 2, fis: [2, 0], pim: [1, 7], kAcil: 3, fark: 0 }));
  assert.ok(gecersiz.includes('acil_durdurma'), 'çözücü işaretlemeli');
  const g = girdiGosterimi(y, gecersiz);
  assert.equal(g.satirlar[0]!.yetkili.metin, 'geçersiz');
  assert.equal(g.satirlar[0]!.yetkili.sinif, 'gecersiz');
  assert.equal(g.satirlar[0]!.kart.metin, 'geçersiz');
  assert.equal(g.satirlar[1]!.yetkili.metin, 'geçersiz');
  assert.equal(g.satirlar[4]!.yetkili.metin, 'geçersiz');
  assert.equal(g.satirlar[2]!.yetkili.metin, 'çıkarılmış');
  assert.equal(g.ozet.durum, 'gecersiz');
  assert.notEqual(g.ozet.sinif, 'iyi');
});

test('B2: çözücünün geçersiz işareti tek başına da geçersiz gösterir', () => {
  const { y } = emniyetCoz(emniyetBaytlari({}));
  const g = girdiGosterimi(y, ['kart_emniyet_pimi[1]']);
  assert.equal(g.satirlar[4]!.kart.metin, 'geçersiz');
  assert.equal(g.ozet.durum, 'gecersiz');
});

test('B2: tüp dolu = 2 "BOŞ" değil "GEÇERSİZ"; süreli izin = 9 "yok" değil "geçersiz"', () => {
  const t: YukAtisDurumTuplerOge = { tup: 0, dolu: 2, sureli_izin_gecerli: 9, izin_kalan_ns: null, son_olay_sonucu: 0 };
  const g = tupGosterimi(t, () => false);
  assert.equal(g.dolu.metin, 'GEÇERSİZ');
  assert.equal(g.izin, 'geçersiz');
  const g2 = tupGosterimi({ ...t, dolu: 1, sureli_izin_gecerli: 0 }, (a) => a === 'dolu');
  assert.equal(g2.dolu.metin, 'GEÇERSİZ');
  assert.equal(tupGosterimi({ ...t, dolu: 0, sureli_izin_gecerli: 0 }).dolu.metin, 'BOŞ');
});

test('B3: bilinenden küçük kanal_surumu ile v1 alanı eksik gelen emniyet çerçevesi panele eksik nesne olarak ulaşmaz', () => {
  // v1 alanları her sürümde vardır (A3-K14 yalnız sona ekler); kanal_surumu 0 yoktur → bozuk.
  const tam = emniyetBaytlari({});
  const c = new Cozucu();
  const out = c.ileti(cerceve(1n, 1, 14, tam.slice(0, 5), 0));
  assert.equal(out.length, 0);
  assert.equal(c.sayaclar.istemci_bozuk_cerceve, 1);
});

test('B3: emniyet gösterimi eksik (null) alanlarda patlamaz, "yok" gösterir', () => {
  const { y } = emniyetCoz(emniyetBaytlari({}));
  const eksik = { ...y, silahlandirma_fisi: null, kart_emniyet_pimi: null, geri_okuma_farki: null } as unknown as YukEmniyetGirdiler;
  const g = girdiGosterimi(eksik, []);
  assert.equal(g.satirlar[1]!.yetkili.metin, 'yok');
  assert.equal(g.satirlar[4]!.kart.metin, 'yok');
  assert.notEqual(g.ozet.durum, 'esit');
});

// ---------------------------------------------------------------- 4 64 bit

test('B4: 64 bit alanlar kayıpsız (2^53, 2^53+1, yok değeri komşuları)', () => {
  const ol = (adim: bigint): Uint8Array => new Bayt().u16(1).u8(0).u8(0).u64(adim).u32(0).u32(0).u16(2).ham([123, 125]).bitir();
  const out = new Cozucu().ileti(
    birlestir(cerceve(9007199254740993n, 1, 3, ol(9007199254740992n)), cerceve(1n, 2, 3, ol(9007199254740993n)), cerceve(1n, 3, 3, ol(0xfffffffffffffffen))),
  );
  assert.equal(out.length, 3);
  assert.equal(out[0]!.baslik.t_ns, 9007199254740993n);
  assert.equal((out[0]!.yuk as YukOlay).adim_no, 9007199254740992n);
  assert.equal((out[1]!.yuk as YukOlay).adim_no, 9007199254740993n);
  assert.equal((out[2]!.yuk as YukOlay).adim_no, 0xfffffffffffffffen, '2^64-2 yok değeri değildir');
  // i64 yok = -1 komşusu -2; INT64_MIN+1
  const at = new Bayt().u8(0).u32(0).u64(1n).u8(1).u8(0).u8(1).u8(1).i64(-2n).u8(0).bitir();
  const a = new Cozucu().ileti(cerceve(1n, 1, 13, at));
  assert.equal((a[0]!.yuk as { tupler: { izin_kalan_ns: bigint }[] }).tupler[0]!.izin_kalan_ns, -2n);
  assert.equal((a[0]!.yuk as { kart_son_olay_kimligi: bigint }).kart_son_olay_kimligi, 1n);
});

test('B4: kodlayıcı 64 bit değeri kayıpsız yazar; güvensiz sayıyı reddeder', () => {
  const y = { tur: 1, onem: 0, kaynak: 0, adim_no: 9007199254740993n, komut_no: 0, istemci_id: 0, ayrinti: '{}' };
  const c = new Cozucu().ileti(cerceveKodla(kanalBul('olay')!, { t_ns: 9007199254740993n, sira: 1 }, yukKodla('yuk_olay', y)));
  assert.equal((c[0]!.yuk as YukOlay).adim_no, 9007199254740993n);
  assert.equal(c[0]!.baslik.t_ns, 9007199254740993n);
  assert.throws(() => yukKodla('yuk_olay', { ...y, adim_no: 2 ** 53 + 2 } as never));
});

// ---------------------------------------------------------------- 5–7 olay akışı (sahte WS + sahte fetch)

class SahteWS {
  static son: SahteWS | null = null;
  binaryType = '';
  readyState = 1;
  gonderilen: string[] = [];
  onmessage: ((e: { data: unknown }) => void) | null = null;
  onclose: ((e: { code: number; reason: string }) => void) | null = null;
  onerror: (() => void) | null = null;
  constructor(_u: string, _p: string[]) {
    SahteWS.son = this;
  }
  send(s: string): void {
    this.gonderilen.push(s);
  }
  close(): void {
    this.readyState = 3;
  }
  metin(o: unknown): void {
    this.onmessage?.({ data: JSON.stringify(o) });
  }
  ikili(u: Uint8Array): void {
    this.onmessage?.({ data: u.buffer.slice(u.byteOffset, u.byteOffset + u.byteLength) });
  }
  kopar(): void {
    this.readyState = 3;
    this.onclose?.({ code: 1006, reason: '' });
  }
}

const merhaba = (son: number): unknown => ({ tur: 'merhaba', a3_surumu: '1.0', katalog_ozeti: 'sha256:' + '0'.repeat(64), baglanti_id: 1, run_id: null, son_olay_sirasi: son, yayin_tavani_hz: 50 });

function olayCercevesi(sira: number, bayrak = 0): Uint8Array {
  const y = { tur: 22, onem: 1, kaynak: 0, adim_no: null, komut_no: 0, istemci_id: 0, ayrinti: `{"kod":"k","ileti":"${sira}"}` };
  return cerceveKodla(kanalBul('olay')!, { t_ns: 1, sira, bayraklar: bayrak }, yukKodla('yuk_olay', y as never));
}

const olayJson = (sira: number): unknown => ({ sira, t_ns: 1, tur: 'uyari', onem: 'uyari', kaynak: 'sim', adim_no: null, komut_no: 0, istemci_id: 0, ayrinti: { kod: 'k', ileti: String(sira) } });

type Cevap = { durum: number; govde: unknown } | (() => { durum: number; govde: unknown });

function sahteFetch(olaylar: (sonra: number) => Cevap): { f: typeof fetch; istekler: string[] } {
  const istekler: string[] = [];
  const f = (async (u: string | URL | Request) => {
    const url = new URL(String(u));
    istekler.push(url.pathname + url.search);
    if (url.pathname.endsWith('/surum')) {
      return new Response(JSON.stringify({ sonuc: 'kabul', a3_surumu: '1.0', katalog_ozeti: 'sha256:' + '0'.repeat(64), ws_alt_protokolu: 'ida-a3.v1', a6_surumu: '1.0', sim_surumu: 't', platform: 'macos-arm64' }), { status: 200 });
    }
    if (url.pathname.endsWith('/olaylar')) {
      let c = olaylar(Number(url.searchParams.get('sonra')));
      if (typeof c === 'function') c = c();
      return new Response(JSON.stringify(c.govde), { status: c.durum });
    }
    return new Response('{}', { status: 404 });
  }) as typeof fetch;
  return { f, istekler };
}

const etkinBaglantilar: A3Baglanti[] = [];

function baglantiKur(f: typeof fetch, ek: Record<string, unknown> = {}): { b: A3Baglanti; olaylar: OlayKaydi[] } {
  // Önceki testin bağlantısı yeniden denerken paylaşılan SahteWS.son'u ezmesin.
  for (const e of etkinBaglantilar.splice(0)) e.kapat();
  SahteWS.son = null;
  const b = new A3Baglanti({ taban: 'http://127.0.0.1:1', jeton: JETON, fetchIslevi: f, WebSocketSinifi: SahteWS as unknown as typeof WebSocket, ilkGecikmeMs: 5, azamiGecikmeMs: 10, ...ek } as never);
  kapanacak.push(() => b.kapat());
  etkinBaglantilar.push(b);
  const olaylar: OlayKaydi[] = [];
  b.olaylar((o) => olaylar.push(o));
  return { b, olaylar };
}

const sayfa = (a: number, z: number, tamam: boolean): { durum: number; govde: unknown } => ({
  durum: 200,
  govde: { sonuc: 'kabul', olaylar: Array.from({ length: z - a + 1 }, (_, i) => olayJson(a + i)), tamam },
});
const ret = (durum: number, kod: string): { durum: number; govde: unknown } => ({ durum, govde: { sonuc: 'ret', komut_no: null, hata: { kod, ileti: kod } } });

test('B5: geçici geçmiş hatası (500) canlı imleci ilerletmez; yeniden denenir; çok sayfalı geçmiş boşluksuz gelir', async () => {
  let deneme = 0;
  const { f } = sahteFetch((sonra) => {
    if (sonra === 100) {
      deneme++;
      return deneme === 1 ? ret(500, 'sunucu_hatasi') : sayfa(101, 105, false);
    }
    if (sonra === 105) return sayfa(106, 110, true);
    return sayfa(1, 0, true);
  });
  const { b, olaylar } = baglantiKur(f, { gecmisYenidenDenemeMs: 20 });
  b.baslat();
  await kosul(() => SahteWS.son !== null, 1000, 'ws');
  SahteWS.son!.metin(merhaba(100));
  SahteWS.son!.kopar();
  SahteWS.son = null;
  await kosul(() => SahteWS.son !== null, 1000, 'yeniden ws');
  SahteWS.son!.metin(merhaba(110));
  SahteWS.son!.ikili(olayCercevesi(111));
  await kosul(() => olaylar.length >= 11, 2000, '101..111');
  assert.deepEqual(
    olaylar.map((o) => o.sira),
    Array.from({ length: 11 }, (_, i) => 101 + i),
  );
  assert.ok(deneme >= 2, 'geçici hata yeniden denenmeli');
});

test('B5: kurtarılamayan boşluk (410) açıkça bildirilir', async () => {
  const { f } = sahteFetch(() => ret(410, 'gecmis_yok'));
  const { b, olaylar } = baglantiKur(f, { gecmisYenidenDenemeMs: 10 });
  b.baslat();
  await kosul(() => SahteWS.son !== null, 1000, 'ws');
  SahteWS.son!.metin(merhaba(100));
  SahteWS.son!.kopar();
  SahteWS.son = null;
  await kosul(() => SahteWS.son !== null, 1000, 'yeniden ws');
  SahteWS.son!.metin(merhaba(110));
  SahteWS.son!.ikili(olayCercevesi(111));
  await kosul(() => olaylar.length >= 1, 2000, '111');
  assert.deepEqual(b.bilgi.olayBosluklari, [{ sonra: 100, kadar: 110 }]);
  assert.ok(b.bilgi.uyarilar.some((u) => u.includes('101') && u.includes('110')));
});

test('B6: kayıttan (kaynak_kayit) gelen olaylar canlı sırayla elenmez; canlı imleci etkilemez', async () => {
  const { f } = sahteFetch(() => sayfa(1, 0, true));
  const { b, olaylar } = baglantiKur(f);
  b.baslat();
  await kosul(() => SahteWS.son !== null, 1000, 'ws');
  SahteWS.son!.metin(merhaba(100));
  SahteWS.son!.ikili(olayCercevesi(1, BAYRAK.KAYNAK_KAYIT));
  SahteWS.son!.ikili(olayCercevesi(5000, BAYRAK.KAYNAK_KAYIT));
  SahteWS.son!.ikili(olayCercevesi(101));
  await kosul(() => olaylar.length >= 3, 1000, 'üç olay');
  assert.deepEqual(
    olaylar.map((o) => [o.sira, o.kaynak_kayit]),
    [
      [1, true],
      [5000, true],
      [101, false],
    ],
  );
});

test('B7: 32 bit sıra sarımı: 0xFFFFFFFF sonrası 0 yenidir', async () => {
  const { f } = sahteFetch(() => sayfa(1, 0, true));
  const { b, olaylar } = baglantiKur(f);
  b.baslat();
  await kosul(() => SahteWS.son !== null, 1000, 'ws');
  SahteWS.son!.metin(merhaba(0xfffffffe));
  SahteWS.son!.ikili(birlestir(olayCercevesi(0xffffffff), olayCercevesi(0), olayCercevesi(1), olayCercevesi(0)));
  await kosul(() => olaylar.length >= 3, 1000, 'sarım');
  await bekle(50);
  assert.deepEqual(
    olaylar.map((o) => o.sira),
    [0xffffffff, 0, 1],
  );
});

test('B7: yeniden bağlanmada sarım yeni süreç sayılmaz; boşluk sarımdan önceki imleçten doldurulur', async () => {
  const { f, istekler } = sahteFetch((sonra) =>
    sonra === 0xfffffffe ? { durum: 200, govde: { sonuc: 'kabul', olaylar: [olayJson(0xffffffff), olayJson(0), olayJson(1), olayJson(2)], tamam: true } } : sayfa(1, 0, true),
  );
  const { b, olaylar } = baglantiKur(f);
  b.baslat();
  await kosul(() => SahteWS.son !== null, 1000, 'ws');
  SahteWS.son!.metin(merhaba(0xfffffffe));
  SahteWS.son!.kopar();
  SahteWS.son = null;
  await kosul(() => SahteWS.son !== null, 1000, 'yeniden ws');
  SahteWS.son!.metin(merhaba(2)); // sarımdan sonra 2: yeni (gerileme değil)
  SahteWS.son!.ikili(olayCercevesi(3));
  await kosul(() => olaylar.length >= 5, 1000, 'dolum');
  assert.ok(istekler.some((u) => u.includes('sonra=4294967294')), 'boşluk sarımdan önceki imleçten istenmeli: ' + istekler.join(' '));
  assert.deepEqual(
    olaylar.map((o) => o.sira),
    [0xffffffff, 0, 1, 2, 3],
  );
  assert.deepEqual(b.bilgi.olayBosluklari, []);
});

test('B7: gerçek gerileme (yeni sim süreci) yeni sürecin olaylarını baştan ister', async () => {
  const { f, istekler } = sahteFetch((sonra) => (sonra === 0 ? sayfa(1, 3, true) : sayfa(1, 0, true)));
  const { b, olaylar } = baglantiKur(f);
  b.baslat();
  await kosul(() => SahteWS.son !== null, 1000, 'ws');
  SahteWS.son!.metin(merhaba(500));
  SahteWS.son!.kopar();
  SahteWS.son = null;
  await kosul(() => SahteWS.son !== null, 1000, 'yeniden ws');
  SahteWS.son!.metin(merhaba(3));
  await kosul(() => olaylar.length >= 3, 1000, 'yeni süreç olayları');
  assert.ok(istekler.some((u) => u.includes('sonra=0')));
  assert.deepEqual(
    olaylar.map((o) => o.sira),
    [1, 2, 3],
  );
});

// ---------------------------------------------------------------- 8 NED hızı

test('B8: gövde hızı tam yönelimle NED hızına çevrilir (yunuslama +30°, u = 1)', () => {
  const q = eulerdenKuaterniyonTest(0, Math.PI / 6, 0);
  const v = aracNedHizi(q, [1, 0, 0, 0, 0, 0]);
  assert.ok(Math.abs(v[0] - 0.8660254) < 1e-6, `kuzey ${v[0]}`);
  assert.ok(Math.abs(v[1]) < 1e-9);
  assert.ok(Math.abs(v[2] + 0.5) < 1e-6, `aşağı ${v[2]}`);
});

// ---------------------------------------------------------------- 9 sonar

test('B9: geçersiz ya da sonlu olmayan ses hızı menzile girmez; halka sayısı sınırlı', () => {
  const t = { n_ornek: 500, ornek_periyodu: 3241 };
  assert.equal(ping360Menzil({ ...t, ses_hizi_mps: Infinity }), null);
  assert.equal(ping360Menzil({ ...t, ses_hizi_mps: Number.NaN }), null);
  assert.equal(ping360Menzil({ ...t, ses_hizi_mps: 0 }), null);
  assert.equal(ping360Menzil({ ...t, ses_hizi_mps: -1481 }), null);
  assert.equal(ping360Menzil({ ...t, ses_hizi_mps: 1481 }, ['ses_hizi_mps']), null);
  assert.ok(Math.abs(ping360Menzil({ ...t, ses_hizi_mps: 1481 })! - 29.96950674375) < 1e-9);
  assert.ok(menzilHalkalari(1e6).length <= 20);
  assert.ok(menzilHalkalari(1e30).length <= 20);
  assert.deepEqual(menzilHalkalari(Infinity), []);
});

// ---------------------------------------------------------------- 10–11 sahte sunucu

async function sunucu(ek: Record<string, unknown> = {}): Promise<SahteSunucu> {
  const s = await sahteSunucuBaslat({ port: 0, jeton: JETON, katalogYolu: KATALOG, statik: null, ...ek });
  kapanacak.push(() => s.kapat());
  return s;
}

function istemci(port: number): Promise<{ ws: WebSocket; metinler: unknown[]; ikililer: ArrayBuffer[]; kapanis: { kod: number | null } }> {
  return new Promise((coz, red) => {
    const ws = new WebSocket(`ws://127.0.0.1:${port}/ws`, ['ida-a3.v1', 'ida-erisim.' + JETON]);
    ws.binaryType = 'arraybuffer';
    const metinler: unknown[] = [];
    const ikililer: ArrayBuffer[] = [];
    const kapanis = { kod: null as number | null };
    ws.onmessage = (e) => (typeof e.data === 'string' ? metinler.push(JSON.parse(e.data)) : ikililer.push(e.data as ArrayBuffer));
    ws.onclose = (e) => (kapanis.kod = e.code);
    ws.onopen = () => coz({ ws, metinler, ikililer, kapanis });
    ws.onerror = () => red(new Error('ws'));
  });
}

test('B10: şemaya uymayan geçerli JSON sunucuyu düşürmez; hata iletisi döner, bağlantı açık kalır (A3-K48)', async () => {
  const s = await sunucu();
  const i = await istemci(s.port);
  const kotuler = ['null', '[]', '"x"', '12', '{"tur":"abone_ol","istek_no":1,"kanallar":[null]}', '{"tur":"abone_ol","istek_no":2,"kanallar":[{"kanal":5}]}', '{"tur":"abonelik_birak","istek_no":3,"kanallar":[null]}', '{"tur":"abone_ol","istek_no":4,"kanallar":[{"kanal":"fizik/durum","hz":"x"}]}'];
  for (const k of kotuler) i.ws.send(k);
  await kosul(() => i.metinler.filter((m) => (m as { tur: string }).tur === 'hata').length >= kotuler.length - 1, 2000, 'hata iletileri');
  i.ws.send(JSON.stringify({ tur: 'abone_ol', istek_no: 9, kanallar: [{ kanal: 'fizik/durum' }] }));
  await kosul(() => i.metinler.some((m) => (m as { tur: string; istek_no: number }).tur === 'abonelik_yaniti' && (m as { istek_no: number }).istek_no === 9), 2000, 'geçerli abonelik');
  assert.equal(i.kapanis.kod, null);
  assert.equal(s.baglantiSayisi(), 1);
});

test('B11: gönderim basıncı altında dizi kuyruğu sınırda kalır, en eskiler düşer ve oncesinde_dusme işaretlenir (A3-K41)', async () => {
  let basinc = false;
  const s = await sunucu({ basinc: () => basinc, dunyaDurdur: true });
  const i = await istemci(s.port);
  i.ws.send(JSON.stringify({ tur: 'abone_ol', istek_no: 1, kanallar: [{ kanal: 'sonar/ping360' }] }));
  await kosul(() => i.metinler.some((m) => (m as { tur: string }).tur === 'abonelik_yaniti'), 2000, 'abonelik');
  basinc = true;
  for (let k = 0; k < 300; k++) s.test.ping360Ekle(99, k);
  await bekle(150);
  basinc = false;
  await bekle(300);
  const cz = new Cozucu();
  const satirlar = i.ikililer.flatMap((m) => cz.ileti(m)).filter((c) => c.kanal.ad === 'sonar/ping360' && (c.yuk as { sensor_id: number }).sensor_id === 99);
  assert.equal(satirlar.length, 256, 'kuyruk sınırı 256');
  assert.equal((satirlar[0]!.yuk as { seq: number }).seq, 44, 'en eski 44 satır düşmeli');
  assert.ok(satirlar[0]!.baslik.bayraklar & BAYRAK.ONCESINDE_DUSME);
});

test('B11: gönderim basıncı altında olay kuyruğu 4096\'yı aşınca bağlantı 4001 ile kapanır (A3-K42)', async () => {
  let basinc = false;
  const s = await sunucu({ basinc: () => basinc, dunyaDurdur: true });
  const i = await istemci(s.port);
  await bekle(100);
  basinc = true;
  for (let k = 0; k < 4200; k++) s.test.olayEkle();
  await bekle(100);
  basinc = false;
  await kosul(() => i.kapanis.kod !== null, 3000, 'kapanış');
  assert.equal(i.kapanis.kod, 4001);
});

// ---------------------------------------------------------------- 13 grafik belleği

test('B13: grafik tamponu çizim olmadan da (dondurma, gizli sekme) sınırlı kalır', () => {
  const t = new Tampon(2);
  for (let i = 0; i < 40_000; i++) t.ekle(i * 0.01, 0, [i, i]); // 400 s, 100 Hz; kirp çağrılmıyor
  // toplu budama payı en çok 1024 örnek (100 Hz'te ~10,2 s)
  assert.ok(t.x[0]! >= 400 - 300 - 11, `en eski ${t.x[0]}`);
  assert.ok(t.x.length <= 30_000 + 1100, `örnek sayısı ${t.x.length}`);
  const u = new Tampon(1);
  for (let i = 0; i < 200_000; i++) u.ekle(i * 0.0001, 0, [i]); // 20 s, 10 kHz: sayı sınırı
  assert.ok(u.x.length <= 60_000, `örnek sayısı ${u.x.length}`);
});
