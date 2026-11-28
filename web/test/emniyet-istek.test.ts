// W5-web-emniyet: emniyet girdisi ve yer istasyonu istekleri (src/a3/emniyet-istek.ts).
// Sahte A3 sunucusuna karşı; istek ve yanıt gövdeleri a3-kanallar.json şemalarına karşı
// doğrulanır. Beklemeler olaya göredir (W5-K43): kanal çerçevesi, kayıt aşaması, bağlantı durumu.
//
// Kabul 1'in dört testi (adları aynen):
//   "azaltan yön jetonsuz gönderilemez"
//   "süresi dolmuş jetonla istek yinelenmez"
//   "bağlantı kopukken acil durdurma UYGULANMADI gösterir"
//   "gösterge sunucu verisi gelmeden değişmez"

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { after, test } from 'node:test';
import { EmniyetIstekleri, JETON_PAYI_MS, atisKomutuMu, secimHatasi, sonucMetni, yonu, type Secim } from '../src/a3/emniyet-istek';
import type { KomutKaydi } from '../src/a3/komut';
import { girdiGosterimi } from '../src/ortak/emniyet';
import { kosul, kur, semayaUy, type Kurulum } from './yardimci-komut';

const acik: Kurulum[] = [];
after(async () => {
  for (const k of acik) await k.kapat();
});

interface Ortam extends Kurulum {
  em: EmniyetIstekleri;
  saat: { t: number };
  /** `emniyet/girdiler` çerçevesi sayacı (olaya göre bekleme). */
  cerceve: { n: number };
  acil(): number | undefined;
  rev(): number;
  /** POST isteği sayısı (yola göre). */
  post(yol: string): number;
  ag: { hata: boolean; deneme: number };
}

async function yeni(): Promise<Ortam> {
  const ag = { hata: false, deneme: 0 };
  const k = await kur({
    fetchHatasi: () => {
      if (ag.hata) ag.deneme++;
      return ag.hata;
    },
  });
  acik.push(k);
  const saat = { t: 1_000_000 };
  const em = new EmniyetIstekleri(k.baglanti, k.komutlar, () => saat.t);
  const cerceve = { n: 0 };
  k.baglanti.abone('emniyet/girdiler', () => cerceve.n++, 10);
  const b = await k.komutlar.gonder('baslat');
  assert.equal(b.asama, 'uygulandi', b.ileti);
  await k.durumBekle('kosuyor');
  await kosul(() => k.baglanti.son('emniyet/girdiler') !== undefined, 5000, 'ilk emniyet/girdiler çerçevesi');
  return {
    ...k,
    em,
    saat,
    cerceve,
    ag,
    acil: () => k.baglanti.son('emniyet/girdiler')?.yuk.acil_durdurma,
    rev: () => Number(k.baglanti.son('emniyet/girdiler')?.yuk.emniyet_rev),
    post: (yol) => k.istekler.filter((i) => i.yontem === 'POST' && i.yol === yol).length,
  };
}

/** Birkaç yeni `emniyet/girdiler` çerçevesi gelsin (gecikmiş bir etkinin ortaya çıkması için olay bekler). */
async function cerceveBekle(o: Ortam, n = 3): Promise<void> {
  const hedef = o.cerceve.n + n;
  await kosul(() => o.cerceve.n >= hedef, 5000, `${n} yeni emniyet/girdiler çerçevesi`);
}

const ACIL_BAS: Secim = { girdi: 'acil_durdurma', hedef: 'basili' };
const ACIL_KALDIR: Secim = { girdi: 'acil_durdurma', hedef: 'serbest' };

test('seçim ve yön tablosu (A3-K89)', () => {
  assert.equal(yonu(ACIL_BAS), 'artiran');
  assert.equal(yonu(ACIL_KALDIR), 'azaltan');
  assert.equal(yonu({ girdi: 'silahlandirma_fisi', tup: 'f1', hedef: 'cikarilmis' }), 'artiran');
  assert.equal(yonu({ girdi: 'silahlandirma_fisi', tup: 'f1', hedef: 'takili' }), 'azaltan');
  assert.equal(yonu({ girdi: 'emniyet_pimi', tup: 'f2', hedef: 'takili' }), 'artiran');
  assert.equal(yonu({ girdi: 'emniyet_pimi', tup: 'f2', hedef: 'cikarilmis' }), 'azaltan');
  assert.equal(secimHatasi(ACIL_BAS), null);
  assert.notEqual(secimHatasi({ girdi: 'acil_durdurma', tup: 'f1', hedef: 'basili' }), null, 'acil durdurma tüp almaz');
  assert.notEqual(secimHatasi({ girdi: 'emniyet_pimi', hedef: 'takili' }), null, 'pim tüp ister');
  assert.notEqual(secimHatasi({ girdi: 'emniyet_pimi', tup: 'f1', hedef: 'basili' }), null, 'pim hedefi takili/cikarilmis');
});

test('atış isteği komutu ad ya da A1 eşlemesinden ayırt edilir (W5-K54)', () => {
  assert.equal(atisKomutuMu({ ad: 'fc_fire' }), true);
  assert.equal(atisKomutuMu({ ad: 'baska_bir_ad', a1: { mesaj: 'IDA_CMD_FC', tur: 'IDAW_FC_OP_FIRE' } }), true, 'ad değişse de A1 eşlemesi yakalar');
  assert.equal(atisKomutuMu({ ad: 'fc_arm', a1: { mesaj: 'IDA_CMD_FC', tur: 'IDAW_FC_OP_ARM' } }), false);
  assert.equal(atisKomutuMu({ ad: 'disarm', a1: { mesaj: 'IDA_CMD_SIMPLE', tur: 'IDAW_CMD_TYPE_DISARM' } }), false);
});

test('sonuç metni: beş aşama ayrı ve açık yazılır (W5-K51)', () => {
  const k = (a: Partial<KomutKaydi>): KomutKaydi => ({
    yerel: 1, komut: 'emniyet_degistir', etiket: 'x', asama: 'gonderiliyor', komut_no: null, hata_kodu: null, ileti: 'neden', uygulama_adimi: null,
    gonderildi: 0, sonuc_zamani: null, gonderildi_mi: true, ozet: '', ...a,
  });
  assert.equal(sonucMetni(k({})).baslik, 'GÖNDERİLİYOR');
  assert.match(sonucMetni(k({ asama: 'kuyrukta' })).baslik, /^KUYRUKTA/);
  assert.equal(sonucMetni(k({ asama: 'uygulandi', uygulama_adimi: 5 })).baslik, 'UYGULANDI');
  const g = sonucMetni(k({ asama: 'reddedildi', gonderildi_mi: false }));
  assert.equal(g.baslik, 'UYGULANMADI');
  assert.match(g.ayrinti, /gönderilmedi: neden/);
  assert.match(sonucMetni(k({ asama: 'reddedildi' })).ayrinti, /sunucu reddetti: neden/);
  const b = sonucMetni(k({ asama: 'bilinmiyor' }));
  assert.equal(b.baslik, 'SONUÇ BİLİNMİYOR');
  assert.match(b.ayrinti, /uygulanmış olabilir/);
});

test('acil durdurmaya bas: tek adım; gövde onay, jeton ve revizyon bağı taşımaz; sunucu uygulayınca gösterge değişir', async () => {
  const o = await yeni();
  assert.equal(o.acil(), 0);
  const r = await o.em.artir(ACIL_BAS);
  assert.equal(r.asama, 'uygulandi', r.ileti);
  await kosul(() => o.acil() === 1, 3000, 'kanalda acil_durdurma = 1 (sunucudan)');
  const ist = o.istekler.filter((i) => i.yontem === 'POST' && i.yol === '/emniyet/degistir');
  assert.equal(ist.length, 1);
  semayaUy('istek_emniyet_degistir', ist[0]!.govde, 'acil durdurma isteği');
  semayaUy('yanit_kuyrukta', ist[0]!.yanit, 'acil durdurma yanıtı');
  const g = ist[0]!.govde as Record<string, unknown>;
  for (const yasak of ['onay', 'onay_jetonu', 'beklenen_emniyet_rev']) assert.equal(g[yasak], undefined, `artıran istek ${yasak} taşımaz`);
  assert.equal(g.girdi, 'acil_durdurma');
  assert.equal(g.hedef_durum, 'basili');
  assert.equal(g.tup, undefined);
  const e = (await o.baglanti.http.istek('GET', '/emniyet')) as { yetkili: { acil_durdurma: string } };
  assert.equal(e.yetkili.acil_durdurma, 'basili');
  assert.equal(o.post('/emniyet/onay_jetonu'), 0, 'artıran yön jeton istemez');
});

test('artıran yön revizyon çakışmasıyla reddedilmez ve yinelenmez (A3-K92, K119; W5-K52 ikinci cümle)', async () => {
  const o = await yeni();
  o.sunucu.test.emniyetRevArtir(); // başka bir kısıtlama revizyonu artırdı; panel haberdar değil
  const r = await o.em.artir({ girdi: 'silahlandirma_fisi', tup: 'f2', hedef: 'cikarilmis' });
  assert.equal(r.asama, 'uygulandi', r.ileti);
  assert.equal(o.post('/emniyet/degistir'), 1, 'tek istek: yeniden gönderme yok');
});

test('gösterge sunucu verisi gelmeden değişmez', async () => {
  const o = await yeni();
  o.sunucu.test.komutlariTut(true); // sunucu isteği kabul eder ama uygulamaz
  const kayitlar: KomutKaydi[] = [];
  o.komutlar.dinle((l) => {
    if (l[0] !== undefined) kayitlar.push({ ...l[0] });
  });
  // Ekranın kullandığı türetim (`girdiGosterimi`): tıklamadan sunucunun bildirmesine kadar her örnek "serbest" kalmalı.
  const gosterilen = (): string | undefined => {
    const c = o.baglanti.son('emniyet/girdiler');
    return c === undefined ? undefined : girdiGosterimi(c.yuk, c.gecersiz).satirlar[0]?.yetkili.metin;
  };
  const ornekler: (string | undefined)[] = [];
  const ornekle = (): void => void ornekler.push(gosterilen());
  o.baglanti.abone('emniyet/girdiler', ornekle, 10);
  o.komutlar.dinle(ornekle);
  const bekleyen = o.em.artir(ACIL_BAS) as Promise<KomutKaydi>;
  ornekle(); // isteğin gönderilmesinden hemen sonra
  await kosul(() => kayitlar.some((x) => x.komut === 'emniyet_degistir' && x.asama === 'kuyrukta'), 3000, 'kayıt kuyrukta');
  assert.equal(o.acil(), 0, 'kuyruğa alınır alınmaz (kanal çerçevesi gelmeden) gösterge değişmedi');
  await cerceveBekle(o, 3);
  assert.equal(o.acil(), 0, 'sunucu uygulamadan gösterge değişmedi');
  assert.equal(gosterilen(), 'serbest');
  assert.deepEqual([...new Set(ornekler)], ['serbest'], 'sunucu bildirene kadar hiçbir an başka bir gösterge değeri görülmedi');
  const e = (await o.baglanti.http.istek('GET', '/emniyet')) as { yetkili: { acil_durdurma: string } };
  assert.equal(e.yetkili.acil_durdurma, 'serbest', 'sunucunun yetkili durumu da değişmedi');
  const ara = o.komutlar.kayitlar()[0]!;
  assert.match(sonucMetni(ara).baslik, /^KUYRUKTA/);
  o.sunucu.test.komutlariTut(false);
  const son = await bekleyen;
  assert.equal(son.asama, 'uygulandi');
  await kosul(() => o.acil() === 1, 3000, 'sunucu bildirince gösterge değişti');
});

test('bağlantı kopukken acil durdurma UYGULANMADI gösterir', async () => {
  const o = await yeni();
  await o.sunucu.kapat(); // sunucu gitti: WebSocket düşer, HTTP ulaşmaz
  await kosul(() => o.baglanti.bilgi.durum !== 'bagli', 5000, 'bağlantı durumu kopuk');
  const r = await o.em.artir(ACIL_BAS);
  assert.equal(r.asama, 'reddedildi');
  assert.equal(r.gonderildi_mi, false);
  assert.equal(sonucMetni(r).baslik, 'UYGULANMADI');
  assert.equal(o.acil(), 0, 'gösterge değişmedi');
  assert.equal(o.post('/emniyet/degistir'), 0);
});

test('bağlıyken yanıt gelmezse SONUÇ BİLİNMİYOR yazar ve istek yinelenmez', async () => {
  const o = await yeni();
  o.ag.hata = true; // POST'lar ağ hatasıyla düşer; WebSocket açık kalır
  const r = await o.em.artir(ACIL_BAS);
  assert.equal(r.asama, 'bilinmiyor');
  assert.equal(sonucMetni(r).baslik, 'SONUÇ BİLİNMİYOR');
  assert.equal(o.acil(), 0);
  await cerceveBekle(o, 3);
  assert.equal(o.ag.deneme, 1, 'istek kendiliğinden yinelenmedi');
});

test('azaltan yön jetonsuz gönderilemez', async () => {
  const o = await yeni();
  await o.em.artir(ACIL_BAS);
  await kosul(() => o.acil() === 1, 3000, 'acil durdurma basılı');
  // 1. jeton hiç alınmadı: istek gitmez
  const r1 = await o.em.gevset(ACIL_KALDIR, true);
  assert.equal(r1.asama, 'reddedildi');
  assert.equal(r1.hata_kodu, 'jeton_yok');
  assert.equal(r1.gonderildi_mi, false);
  // 2. jeton var ama onay kutusu işaretsiz: istek gitmez, jeton korunur
  const j = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j, JSON.stringify(j));
  const r2 = await o.em.gevset(ACIL_KALDIR, false);
  assert.equal(r2.hata_kodu, 'onay_yok');
  assert.equal(r2.gonderildi_mi, false);
  assert.ok(o.em.jeton() !== null, 'onaysız deneme jetonu tüketmez');
  // 3. jeton başka bir işlem içindi: istek gitmez, jeton düşer
  const r3 = await o.em.gevset({ girdi: 'silahlandirma_fisi', tup: 'f1', hedef: 'takili' }, true);
  assert.equal(r3.hata_kodu, 'jeton_uyusmuyor');
  assert.equal(o.em.jeton(), null);
  assert.equal(o.post('/emniyet/degistir'), 1, 'yalnız ilk (artıran) istek gitti; hiçbir gevşetme gönderilmedi');
  assert.equal(o.acil(), 1);
  // Sunucu da jetonsuz gevşetmeyi reddeder (sahte sunucunun sözleşme davranışı)
  await assert.rejects(
    o.baglanti.http.komut('/emniyet/degistir', { beklenen_run_id: (await o.durum()).run_id, girdi: 'acil_durdurma', hedef_durum: 'serbest' }),
    (h: { kod?: string }) => h.kod === 'onay_gerekli',
  );
  // artıran yön için gevset, azaltan yön için artir reddedilir
  assert.equal((await o.em.artir(ACIL_KALDIR)).hata_kodu, 'yon_yanlis');
});

test('iki adımlı gevşetme: jeton → onay → sunucu uygular; jeton tek kullanımlıktır', async () => {
  const o = await yeni();
  await o.em.artir(ACIL_BAS);
  await kosul(() => o.acil() === 1, 3000, 'basılı');
  const j = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j);
  if (!('jeton' in j)) return;
  const jetonIstegi = o.istekler.find((i) => i.yol === '/emniyet/onay_jetonu')!;
  semayaUy('istek_onay_jetonu_emniyet', jetonIstegi.govde, 'jeton isteği');
  semayaUy('yanit_onay_jetonu', jetonIstegi.yanit, 'jeton yanıtı');
  assert.equal(o.acil(), 1, 'jeton almak durumu değiştirmez');
  const r = await o.em.gevset(ACIL_KALDIR, true);
  assert.equal(r.asama, 'uygulandi', r.ileti);
  await kosul(() => o.acil() === 0, 3000, 'sunucu bildirince serbest');
  const istek = o.istekler.filter((i) => i.yol === '/emniyet/degistir').at(-1)!;
  semayaUy('istek_emniyet_degistir', istek.govde, 'gevşetme isteği');
  const g = istek.govde as Record<string, unknown>;
  assert.equal(g.onay, true);
  assert.equal(g.onay_jetonu, j.jeton.jeton);
  assert.equal(g.beklenen_emniyet_rev, j.jeton.emniyet_rev);
  assert.equal(o.em.jeton(), null, 'jeton tüketildi');
  const tekrar = await o.em.gevset(ACIL_KALDIR, true);
  assert.equal(tekrar.hata_kodu, 'jeton_yok');
  assert.equal(o.post('/emniyet/degistir'), 2, 'tekrar gönderim yok');
  assert.equal(o.post('/emniyet/onay_jetonu'), 1, 'yeni jeton kendiliğinden istenmedi');
});

test('süresi dolmuş jetonla istek yinelenmez', async () => {
  const o = await yeni();
  await o.em.artir(ACIL_BAS);
  await kosul(() => o.acil() === 1, 3000, 'basılı');
  // (a) istemci saati: jeton ömründen payı düşüldükten sonra düşer; istek gitmez
  const j = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j);
  o.saat.t += 10_000 - JETON_PAYI_MS;
  assert.equal(o.em.jeton(), null);
  assert.match(o.em.jetonNedeni(), /süresi doldu/);
  const r = await o.em.gevset(ACIL_KALDIR, true);
  assert.equal(r.hata_kodu, 'jeton_yok');
  assert.equal(r.gonderildi_mi, false);
  assert.equal(o.post('/emniyet/degistir'), 1);
  assert.equal(o.post('/emniyet/onay_jetonu'), 1, 'süre dolunca yeni jeton kendiliğinden istenmedi');
  // (b) sunucu tarafı: istemci jetonu geçerli sanır, sunucu süresi dolmuş sayar → ret, yineleme yok
  const j2 = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j2);
  o.sunucu.test.jetonlariSonlandir();
  const r2 = await o.em.gevset(ACIL_KALDIR, true);
  assert.equal(r2.asama, 'reddedildi');
  assert.equal(r2.hata_kodu, 'onay_jetonu_gecersiz');
  assert.equal(r2.gonderildi_mi, true);
  assert.equal(sonucMetni(r2).baslik, 'UYGULANMADI');
  await cerceveBekle(o, 3);
  assert.equal(o.post('/emniyet/degistir'), 2, 'ikinci istek yalnız bir kez gitti');
  assert.equal(o.post('/emniyet/onay_jetonu'), 2, 'ret sonrası yeni jeton istenmedi');
  assert.equal(o.em.jeton(), null);
  assert.equal(o.acil(), 1, 'acil durdurma basılı kaldı');
});

test('revizyon çakışması: jeton düşer; sunucu reddederse istek yinelenmez', async () => {
  const o = await yeni();
  await o.em.artir(ACIL_BAS);
  await kosul(() => o.acil() === 1, 3000, 'basılı');
  // (a) kanal revizyonu artınca istemci jetonu kendisi düşürür
  const j = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j);
  o.sunucu.test.emniyetRevArtir();
  await kosul(() => o.em.jeton() === null, 3000, 'jeton revizyon artışıyla düştü');
  assert.match(o.em.jetonNedeni(), /revizyonu değişti/);
  // (b) haber gelmeden gönderilen istek: sunucu girişte reddeder
  const j2 = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j2);
  o.sunucu.test.emniyetRevArtir();
  const r = await o.em.gevset(ACIL_KALDIR, true);
  assert.equal(r.hata_kodu, 'emniyet_rev_uyusmuyor');
  assert.equal(o.post('/emniyet/degistir'), 2);
  // (c) sunucu kuyruğa aldıktan sonra uygulama sınırında reddeder
  const j3 = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j3);
  o.sunucu.test.komutlariTut(true);
  const bekleyen = o.em.gevset(ACIL_KALDIR, true) as Promise<KomutKaydi>;
  await kosul(() => o.komutlar.kayitlar()[0]?.asama === 'kuyrukta', 3000, 'gevşetme kuyrukta');
  o.sunucu.test.emniyetRevArtir();
  o.sunucu.test.komutlariTut(false);
  const r3 = await bekleyen;
  assert.equal(r3.asama, 'reddedildi');
  assert.equal(r3.hata_kodu, 'emniyet_rev_uyusmuyor');
  await cerceveBekle(o, 3);
  assert.equal(o.post('/emniyet/degistir'), 3, 'yineleme yok');
  assert.equal(o.post('/emniyet/onay_jetonu'), 3, 'yeni jeton kendiliğinden istenmedi');
  assert.equal(o.acil(), 1);
});

test('bağlantı kopunca jeton düşer; toparlanınca geri gelmez', async () => {
  const o = await yeni();
  await o.em.artir(ACIL_BAS);
  await kosul(() => o.acil() === 1, 3000, 'basılı');
  const j = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j);
  o.sunucu.baglantilariKopar();
  await kosul(() => o.em.jeton() === null, 5000, 'jeton bağlantı kopunca düştü');
  assert.match(o.em.jetonNedeni(), /Bağlantı koptu/);
  await kosul(() => o.baglanti.bilgi.durum === 'bagli', 8000, 'yeniden bağlandı');
  assert.equal(o.em.jeton(), null);
  const r = await o.em.gevset(ACIL_KALDIR, true);
  assert.equal(r.hata_kodu, 'jeton_yok');
  assert.equal(o.post('/emniyet/degistir'), 1);
});

test('fiş ve pim: tüp zorunlu; artıran tek adım, azaltan iki adım', async () => {
  const o = await yeni();
  const fisCikar = await o.em.artir({ girdi: 'silahlandirma_fisi', tup: 'f1', hedef: 'cikarilmis' });
  assert.equal(fisCikar.asama, 'uygulandi', fisCikar.ileti);
  const pimTak = await o.em.artir({ girdi: 'emniyet_pimi', tup: 'f2', hedef: 'takili' });
  assert.equal(pimTak.asama, 'uygulandi', pimTak.ileti);
  const eksik = await o.em.artir({ girdi: 'emniyet_pimi', hedef: 'takili' });
  assert.equal(eksik.hata_kodu, 'gecersiz_istek');
  assert.equal(eksik.gonderildi_mi, false);
  const pimCikar: Secim = { girdi: 'emniyet_pimi', tup: 'f2', hedef: 'cikarilmis' };
  const j = await o.em.jetonIste(pimCikar);
  assert.ok('jeton' in j);
  const r = await o.em.gevset(pimCikar, true);
  assert.equal(r.asama, 'uygulandi', r.ileti);
  await kosul(() => o.baglanti.son('emniyet/girdiler')?.yuk.emniyet_pimi[1] === 0, 3000, 'pim f2 çıkarılmış (sunucudan)');
  const g = o.istekler.filter((i) => i.yol === '/emniyet/degistir').at(-1)!.govde as Record<string, unknown>;
  assert.equal(g.tup, 'f2');
  semayaUy('istek_emniyet_degistir', g, 'pim gevşetme isteği');
});

test('yer istasyonu: liste sunucudan gelir, atış isteği gizlenir; genişletici iki adımlı, daraltıcı tek adımlı', async () => {
  const o = await yeni();
  const l = await o.em.yerKomutlari();
  assert.ok('komutlar' in l, JSON.stringify(l));
  if (!('komutlar' in l)) return;
  semayaUy('a1_komut_listesi', { surum: '1.0', komutlar: l.komutlar }, 'komut listesi');
  assert.ok(l.komutlar.some((k) => k.ad === 'arm'));
  assert.ok(!l.komutlar.some((k) => /fire/.test(k.ad)), 'atış isteği komutu gösterilmez');
  assert.ok(l.gizlenen.includes('fc_fire'));
  // listede olmayan (ya da gizlenen) komut gönderilmez
  const f = await o.em.yerGonder('fc_fire', { tube: 0 }, true);
  assert.equal(f.hata_kodu, 'komut_listede_yok');
  assert.equal(f.gonderildi_mi, false);
  const y = await o.em.yerGonder('olmayan_komut', {});
  assert.equal(y.hata_kodu, 'komut_listede_yok');
  // genişletici: jetonsuz gönderilmez
  const jetonsuz = await o.em.yerGonder('arm', {}, true);
  assert.equal(jetonsuz.hata_kodu, 'jeton_yok');
  assert.equal(o.post('/yer_istasyonu/komut'), 0);
  const j = await o.em.yerJetonIste('arm', {});
  assert.ok('jeton' in j, JSON.stringify(j));
  const ji = o.istekler.find((i) => i.yol === '/yer_istasyonu/onay_jetonu')!;
  semayaUy('istek_onay_jetonu_yer_istasyonu', ji.govde, 'yer istasyonu jeton isteği');
  assert.equal((await o.em.yerGonder('arm', {}, false)).hata_kodu, 'onay_yok');
  // jeton başka parametre içinse düşer ve gönderilmez
  assert.equal((await o.em.yerGonder('set_mode_genisletici', { mode: 'GUIDED' }, true)).hata_kodu, 'jeton_uyusmuyor');
  assert.equal(o.post('/yer_istasyonu/komut'), 0);
  const j2 = await o.em.yerJetonIste('arm', {});
  assert.ok('jeton' in j2);
  const olaylar: string[] = [];
  o.baglanti.olaylar((e) => {
    if (e.tur === 'yer_istasyonu_komutu') olaylar.push(String(e.ayrinti?.durum));
  });
  const r = await o.em.yerGonder('arm', {}, true);
  assert.equal(r.asama, 'uygulandi', r.ileti);
  const gk = o.istekler.find((i) => i.yol === '/yer_istasyonu/komut')!;
  semayaUy('istek_yer_istasyonu_komut', gk.govde, 'yer istasyonu komutu');
  assert.equal((gk.govde as Record<string, unknown>).onay, true);
  await kosul(() => olaylar.includes('karta_iletildi'), 3000, 'yer_istasyonu_komutu olayı');
  // daraltıcı: tek adım, onay ve revizyon bağı yok; sunucu revizyonu artırır
  const rev0 = o.rev();
  const d = await o.em.yerGonder('fc_disarm', {});
  assert.equal(d.asama, 'uygulandi', d.ileti);
  const gd = o.istekler.filter((i) => i.yol === '/yer_istasyonu/komut').at(-1)!.govde as Record<string, unknown>;
  assert.equal(gd.onay, undefined);
  assert.equal(gd.onay_jetonu, undefined);
  assert.equal(gd.beklenen_emniyet_rev, undefined);
  await kosul(() => o.rev() > rev0, 3000, 'daraltıcı komut revizyonu artırdı (sunucudan)');
});

test('yer istasyonu: yanıtsız istek SONUÇ BİLİNMİYOR olur ve yinelenmez', async () => {
  const o = await yeni();
  const l = await o.em.yerKomutlari();
  assert.ok('komutlar' in l);
  const j = await o.em.yerJetonIste('arm', {});
  assert.ok('jeton' in j);
  o.ag.hata = true;
  const r = await o.em.yerGonder('arm', {}, true);
  assert.equal(r.asama, 'bilinmiyor');
  assert.equal(o.em.jeton(), null, 'jeton tüketildi');
  await cerceveBekle(o, 3);
  assert.equal(o.ag.deneme, 1);
});

test('sahte sunucu: aynı sınırda sonradan gelen kısıtlama önce uygulanır, gevşetme reddedilir', async () => {
  const o = await yeni();
  const j = await o.em.jetonIste(ACIL_KALDIR);
  assert.ok('jeton' in j);
  o.sunucu.test.komutlariTut(true);
  const gevsek = o.em.gevset(ACIL_KALDIR, true);
  await kosul(() => o.komutlar.kayitlar()[0]?.asama === 'kuyrukta', 3000, 'gevşetme kuyrukta');
  const kisit = o.em.artir(ACIL_BAS);
  await kosul(() => o.komutlar.kayitlar()[0]?.asama === 'kuyrukta', 3000, 'kısıtlama kuyrukta');
  o.sunucu.test.komutlariTut(false);
  assert.equal((await gevsek).hata_kodu, 'kisitlama_hakim');
  assert.equal((await kisit).asama, 'uygulandi');
  await kosul(() => o.acil() === 1, 3000, 'kısıtlayan değer kanalda');
});

test('sahte sunucu: nötr komut revizyonu artırmaz ve bayat run_id ile kabul edilmez', async () => {
  const o = await yeni();
  const l = await o.em.yerKomutlari();
  assert.ok('komutlar' in l);
  const once = await o.baglanti.http.istek<{ emniyet_rev: number }>('GET', '/emniyet');
  const r = await o.em.yerGonder('param_get', { param_id: 1 });
  assert.equal(r.asama, 'uygulandi', r.ileti);
  const sonra = await o.baglanti.http.istek<{ emniyet_rev: number }>('GET', '/emniyet');
  assert.equal(sonra.emniyet_rev, once.emniyet_rev);
  await assert.rejects(o.baglanti.http.komut('/yer_istasyonu/komut', {
    beklenen_run_id: '19990101-000000-000000', komut: 'param_get', parametreler: { param_id: 1 },
  }), (h: { kod?: string }) => h.kod === 'beklenen_durum_uyusmuyor');
});

test('W5-K53: kısayol, toplu eylem ve depolama yok; jeton yalnız bellekte (kaynak denetimi)', () => {
  const dosyalar = ['src/a3/emniyet-istek.ts', 'src/ortak/acil.ts', 'src/paneller/emniyet.ts'];
  for (const d of dosyalar) {
    const kaynak = readFileSync(d, 'utf8')
      .split('\n')
      .filter((satir) => !/^\s*(\/\/|\*|\/\*)/.test(satir)) // yorum satırlarını at
      .join('\n');
    assert.doesNotMatch(kaynak, /keydown|keyup|keypress|accesskey|addEventListener\(\s*'key/i, `${d}: klavye kısayolu yok`);
    assert.doesNotMatch(kaynak, /localStorage|sessionStorage|indexedDB|document\.cookie|history\.(push|replace)State|location\.(hash|search|href)\s*=/, `${d}: jeton depolamaya ya da adrese yazılmaz`);
    assert.doesNotMatch(kaynak, /\.checked\s*=\s*true|checked:\s*true|setAttribute\(\s*'checked'/, `${d}: onay kutusu önceden işaretli gelmez`);
    assert.doesNotMatch(kaynak, /\b(hepsini|tümünü|toplu)\b/i, `${d}: toplu eylem düğmesi yok`);
    assert.doesNotMatch(kaynak, /\b(confirm|alert|prompt)\s*\(/, `${d}: tarayıcı onay kutusu yok`);
  }
});
