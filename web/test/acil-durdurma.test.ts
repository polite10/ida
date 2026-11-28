// Dar web kabulü: sabit basma isteği ve HTTP/sonuç/telemetri ayrımı.
// Ağ ve olaylar test tarafından ilerletilir; süreye göre bekleme ve soket yoktur.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { test, type TestContext } from 'node:test';
import type { A3Baglanti, BaglantiDurumu } from '../src/a3/baglanti';
import { A3Http } from '../src/a3/http';
import { KomutYoneticisi, type KomutAsamasi, type KomutKaydi } from '../src/a3/komut';
import type { OlayKaydi } from '../src/a3/olay';
import { acilDurdurmaDenetimi } from '../src/ortak/acil-durdurma';
import { semaDogrula, semaOku } from '../arac/sahte-a3/sema';

// Bu bileşenin kullandığı DOM yüzeyi; gerçek dinleyiciler ve metin güncellemeleri çalışır.
class Oge {
  className = '';
  textContent = '';
  title = '';
  readonly cocuklar: Oge[] = [];
  readonly ozellikler = new Map<string, string>();
  readonly dinleyiciler = new Map<string, (() => void)[]>();
  readonly classList = {
    toggle: (ad: string, etkin: boolean): void => {
      const siniflar = new Set(this.className.split(' ').filter(Boolean));
      if (etkin) siniflar.add(ad); else siniflar.delete(ad);
      this.className = [...siniflar].join(' ');
    },
  };
  constructor(readonly etiket: string) {}
  setAttribute(ad: string, deger: string): void { this.ozellikler.set(ad, deger); }
  append(...cocuklar: (Oge | string)[]): void {
    for (const c of cocuklar) {
      if (typeof c === 'string') this.textContent += c;
      else this.cocuklar.push(c);
    }
  }
  addEventListener(ad: string, f: () => void): void {
    this.dinleyiciler.set(ad, [...this.dinleyiciler.get(ad) ?? [], f]);
  }
  tikla(): void { for (const f of this.dinleyiciler.get('click') ?? []) f(); }
  bul(sinif: string): Oge {
    if (this.className.split(' ').includes(sinif)) return this;
    for (const c of this.cocuklar) {
      try { return c.bul(sinif); } catch { /* sonraki çocuk */ }
    }
    throw new Error(`Öğe bulunamadı: ${sinif}`);
  }
}

interface Istek { yol: string; baslik: Record<string, string>; govde: Record<string, unknown> | null; sinyal: AbortSignal | null }
const json = (veri: unknown, durum = 200): Response => new Response(JSON.stringify(veri), { status: durum });
const RUN = '20261128-120000-abcdef';
const JETON = 'e'.repeat(64);
const SEMA = semaOku(readFileSync(resolve(process.cwd(), '../docs/sozlesmeler/sema/a3-kanallar.json'), 'utf8'));

function ertelenen<T>(): { soz: Promise<T>; coz: (v: T) => void } {
  let coz!: (v: T) => void;
  const soz = new Promise<T>((r) => { coz = r; });
  return { soz, coz };
}

function kur(t: TestContext,
  yanitla: (i: Istek) => Response | Promise<Response> = () => json({ sonuc: 'kuyrukta', komut_no: 41 }, 202),
  istemciYanitla: (i: Istek) => Response | Promise<Response> = () => json({ sonuc: 'kabul', istemci_id: 7 }),
) {
  const oncekiDom = Object.getOwnPropertyDescriptor(globalThis, 'document');
  Object.defineProperty(globalThis, 'document', { configurable: true, value: { createElement: (ad: string) => new Oge(ad) } });
  t.after(() => {
    if (oncekiDom === undefined) Reflect.deleteProperty(globalThis, 'document');
    else Object.defineProperty(globalThis, 'document', oncekiDom);
  });
  const istekler: Istek[] = [];
  const fetchIslevi: typeof fetch = async (girdi, init) => {
    const i: Istek = {
      yol: new URL(String(girdi)).pathname,
      baslik: init?.headers as Record<string, string>,
      govde: typeof init?.body === 'string' ? JSON.parse(init.body) as Record<string, unknown> : null,
      sinyal: init?.signal ?? null,
    };
    istekler.push(i);
    if (i.yol === '/api/v1/istemciler') return istemciYanitla(i);
    return yanitla(i);
  };
  const bilgi = { durum: 'bagli' as BaglantiDurumu };
  const cerceveler = new Map<string, unknown>();
  const aboneler = new Map<string, Set<(c: unknown) => void>>();
  const durumDinleyenler = new Set<() => void>();
  const olayDinleyenler = new Set<(o: OlayKaydi) => void>();
  const b = {
    bilgi,
    http: new A3Http('http://127.0.0.1:47100', JETON, fetchIslevi),
    son: (ad: string) => cerceveler.get(ad),
    abone: (ad: string, f: (c: unknown) => void) => {
      if (!aboneler.has(ad)) aboneler.set(ad, new Set());
      aboneler.get(ad)!.add(f);
      return () => aboneler.get(ad)!.delete(f);
    },
    durumDinle: (f: () => void) => { durumDinleyenler.add(f); f(); return () => durumDinleyenler.delete(f); },
    olaylar: (f: (o: OlayKaydi) => void) => { olayDinleyenler.add(f); return () => olayDinleyenler.delete(f); },
  } as unknown as A3Baglanti;
  const komutlar = new KomutYoneticisi(b);
  t.after(() => komutlar.kapat());
  const kok = acilDurdurmaDenetimi(b, komutlar).kok as unknown as Oge;
  const cerceve = (ad: string, yuk: Record<string, unknown>, gecersiz: string[] = []): void => {
    const c = { yuk, gecersiz };
    cerceveler.set(ad, c);
    for (const f of aboneler.get(ad) ?? []) f(c);
  };
  return {
    komutlar, istekler, cerceve,
    dugme: kok.bul('acil-durdurma-dugme'), durum: kok.bul('acil-durdurma-durum'),
    sonuc: kok.bul('denetim-son'), neden: kok.bul('denetim-yerel'),
    hazir(run: string | null = RUN): void {
      cerceve('calistirma/durum', { run_id: run ?? '', durum_rev: 12n });
      cerceve('emniyet/girdiler', { acil_durdurma: 0 });
    },
    baglanti(durum: BaglantiDurumu): void {
      bilgi.durum = durum;
      for (const f of durumDinleyenler) f();
    },
    olay(sonuc = 'uygulandi', hata_kodu: string | null = null): void {
      const o = { tur: 'komut_sonucu', ayrinti: { komut_no: 41, sonuc, hata_kodu, uygulama_adimi: 18 } } as unknown as OlayKaydi;
      for (const f of olayDinleyenler) f(o);
    },
  };
}

function asamaBekle(k: KomutYoneticisi, asama: KomutAsamasi): Promise<KomutKaydi> {
  const son = k.kayitlar()[0];
  if (son?.asama === asama) return Promise.resolve(son);
  return new Promise((coz) => {
    const kaldir = k.dinle((liste) => {
      if (liste[0]?.asama === asama) { kaldir(); coz(liste[0]); }
    });
  });
}
const basmaIstekleri = (k: ReturnType<typeof kur>): Istek[] => k.istekler.filter((i) => i.yol === '/api/v1/emniyet/degistir');

for (const run of [null, RUN]) {
  test(`sabit basma isteği (${run === null ? 'null run_id' : 'run_id'}): 202 ve sonuç göstergeyi değiştirmez`, async (t) => {
    const k = kur(t);
    k.hazir(run);
    assert.equal(k.dugme.textContent, 'SANAL ACİL DURDUR');
    k.dugme.tikla();
    await asamaBekle(k.komutlar, 'kuyrukta');
    assert.match(k.sonuc.textContent, /KUYRUKTA · Uygulama sonucu bekleniyor/);
    assert.equal(k.durum.textContent, 'Sanal girdi: serbest');
    const [i] = basmaIstekleri(k);
    assert.ok(i);
    assert.deepEqual(Object.keys(i.govde!).sort(), ['beklenen_run_id', 'girdi', 'hedef_durum', 'istek_kimligi']);
    assert.equal(i.govde!.beklenen_run_id, run);
    assert.equal(i.govde!.girdi, 'acil_durdurma');
    assert.equal(i.govde!.hedef_durum, 'basili');
    assert.match(String(i.govde!.istek_kimligi), /^[0-9a-f]{24}$/);
    assert.equal(i.baslik['Ida-Istemci'], '7');
    assert.equal(i.baslik.Authorization, `Bearer ${JETON}`);
    assert.equal(i.baslik['Content-Type'], 'application/json');
    assert.deepEqual(semaDogrula(SEMA, SEMA.$defs.istek_emniyet_degistir!, i.govde), []);
    k.olay();
    await asamaBekle(k.komutlar, 'uygulandi');
    assert.match(k.sonuc.textContent, /uygulandı \(adım 18\)/);
    assert.equal(k.durum.textContent, 'Sanal girdi: serbest', 'sonuç olayı telemetri değildir');
    k.cerceve('emniyet/girdiler', { acil_durdurma: 1 });
    assert.equal(k.durum.textContent, 'Sanal girdi: BASILI');
    assert.equal(basmaIstekleri(k).length, 1);
    assert.deepEqual(k.istekler.map((i) => i.yol), ['/api/v1/istemciler', '/api/v1/emniyet/degistir']);
    assert.deepEqual([...k.dugme.dinleyiciler.keys()], ['click']);
  });
}

test('bağlıyken ilk çalıştırma çerçevesi beklenmez: tek null run_id POST ve nihai sonuç', async (t) => {
  const k = kur(t);
  assert.equal(k.dugme.ozellikler.get('aria-disabled'), 'false');
  assert.equal(k.neden.textContent, '');
  k.dugme.tikla();
  await asamaBekle(k.komutlar, 'kuyrukta');
  assert.match(k.sonuc.textContent, /KUYRUKTA · Uygulama sonucu bekleniyor/);
  assert.equal(basmaIstekleri(k).length, 1);
  assert.equal(basmaIstekleri(k)[0]!.govde!.beklenen_run_id, null);
  assert.equal(basmaIstekleri(k)[0]!.govde!.girdi, 'acil_durdurma');
  assert.equal(basmaIstekleri(k)[0]!.govde!.hedef_durum, 'basili');
  assert.equal(k.durum.textContent, 'Sanal girdi: telemetri bekleniyor');
  k.olay();
  const sonuc = await asamaBekle(k.komutlar, 'uygulandi');
  assert.equal(sonuc.gonderildi_mi, true);
  assert.equal(sonuc.uygulama_adimi, 18);
  assert.match(k.sonuc.textContent, /uygulandı \(adım 18\)/);
  assert.equal(k.durum.textContent, 'Sanal girdi: telemetri bekleniyor');
  assert.equal(basmaIstekleri(k).length, 1);
});

test('kopuk bağlantıda ilk çerçeve veya önbellekte durum olsa da sıfır POST ve UYGULANMADI', async (t) => {
  const k = kur(t);
  k.baglanti('kopuk');
  assert.equal(k.dugme.ozellikler.get('aria-disabled'), 'true');
  assert.match(k.neden.textContent, /bağlantı yok/);
  k.dugme.tikla();
  assert.equal(k.komutlar.kayitlar()[0]!.gonderildi_mi, false);
  assert.match(k.sonuc.textContent, /UYGULANMADI/);
  assert.match(k.sonuc.textContent, /Gönderilmedi/);
  k.baglanti('bagli');
  k.hazir();
  k.baglanti('kopuk');
  assert.match(k.neden.textContent, /bağlantı yok/);
  assert.match(k.durum.textContent, /serbest · bayat/);
  k.dugme.tikla();
  assert.equal((await asamaBekle(k.komutlar, 'reddedildi')).gonderildi_mi, false);
  assert.match(k.sonuc.textContent, /UYGULANMADI/);
  assert.equal(k.istekler.length, 0);
});

test('sunucunun ret nedeni açıkça gösterilir', async (t) => {
  const k = kur(t, () => json({ sonuc: 'ret', komut_no: 41, hata: { kod: 'tekrar_kipinde_yasak', ileti: 'Tekrar kipinde yasak' } }, 409));
  k.hazir();
  k.dugme.tikla();
  assert.equal((await asamaBekle(k.komutlar, 'reddedildi')).hata_kodu, 'tekrar_kipinde_yasak');
  assert.match(k.sonuc.textContent, /UYGULANMADI/);
  assert.match(k.sonuc.textContent, /tekrar kipinde/i);
  assert.equal(k.durum.textContent, 'Sanal girdi: serbest');
  assert.equal(basmaIstekleri(k).length, 1);
});

test('ağ hatası sonucu bilinmiyor; bağlantı yenilenince POST yinelenmez', async (t) => {
  const k = kur(t, () => { throw new TypeError('fetch failed'); });
  k.hazir();
  k.dugme.tikla();
  assert.equal((await asamaBekle(k.komutlar, 'bilinmiyor')).hata_kodu, 'ag_hatasi');
  assert.match(k.sonuc.textContent, /SONUÇ BİLİNMİYOR/);
  k.baglanti('kopuk');
  k.baglanti('bagli');
  assert.equal(basmaIstekleri(k).length, 1);
  assert.equal(k.durum.textContent, 'Sanal girdi: serbest · bayat / yeni telemetri bekleniyor');
  k.cerceve('emniyet/girdiler', { acil_durdurma: 0 });
  assert.equal(k.durum.textContent, 'Sanal girdi: serbest');
});

test('kuyruktayken kopma belirsizdir; bağlantı sonrası yalnız komut sonucu sorgulanır', async (t) => {
  const k = kur(t, (i) => i.yol.startsWith('/api/v1/komutlar/')
    ? json({ sonuc: 'kabul', komut_no: 41, durum: 'uygulandi', uygulama_adimi: 18 })
    : json({ sonuc: 'kuyrukta', komut_no: 41 }, 202));
  k.hazir();
  k.dugme.tikla();
  await asamaBekle(k.komutlar, 'kuyrukta');
  k.baglanti('kopuk');
  assert.equal(k.komutlar.kayitlar()[0]!.asama, 'bilinmiyor');
  assert.match(k.sonuc.textContent, /SONUÇ BİLİNMİYOR/);
  k.baglanti('bagli');
  await asamaBekle(k.komutlar, 'uygulandi');
  assert.equal(basmaIstekleri(k).length, 1);
  assert.equal(k.istekler.filter((i) => i.yol === '/api/v1/komutlar/41').length, 1);
  assert.equal(k.durum.textContent, 'Sanal girdi: serbest · bayat / yeni telemetri bekleniyor');
  k.cerceve('emniyet/girdiler', { acil_durdurma: 1 });
  assert.equal(k.durum.textContent, 'Sanal girdi: BASILI');
});

test('sonuç HTTP kabulünden önce gelebilir; çift tıklama ikinci basma isteği üretmez', async (t) => {
  const yanit = ertelenen<Response>();
  const gitti = ertelenen<void>();
  const k = kur(t, () => { gitti.coz(); return yanit.soz; });
  k.hazir();
  k.dugme.tikla();
  await gitti.soz;
  assert.equal(k.dugme.ozellikler.get('aria-disabled'), 'true');
  k.dugme.tikla();
  assert.equal(basmaIstekleri(k).length, 1);
  k.olay();
  yanit.coz(json({ sonuc: 'kuyrukta', komut_no: 41 }, 202));
  await asamaBekle(k.komutlar, 'uygulandi');
  assert.equal(k.komutlar.kayitlar().length, 1);
  assert.match(k.sonuc.textContent, /uygulandı/);
  assert.equal(k.durum.textContent, 'Sanal girdi: serbest');
  assert.equal(basmaIstekleri(k).length, 1);
});

test('HTTP yanıtı beklenirken bağlantı koparsa 202 sonucu bilinmiyor olur', async (t) => {
  const yanit = ertelenen<Response>();
  const gitti = ertelenen<void>();
  const k = kur(t, () => { gitti.coz(); return yanit.soz; });
  k.hazir();
  k.dugme.tikla();
  await gitti.soz;
  k.baglanti('kopuk');
  yanit.coz(json({ sonuc: 'kuyrukta', komut_no: 41 }, 202));
  assert.equal((await asamaBekle(k.komutlar, 'bilinmiyor')).hata_kodu, 'baglanti_koptu');
  assert.match(k.sonuc.textContent, /SONUÇ BİLİNMİYOR/);
  assert.equal(basmaIstekleri(k).length, 1);
});

test('takılan istemci kaydı, POST veya yanıt gövdesi 5 s bütçesinde iptal edilir; sonraki tek tıklama çalışır', async (t) => {
  t.mock.timers.enable({ apis: ['setTimeout'] });
  let faz: 'istemci' | 'post' | 'govde' | 'tamam' = 'istemci';
  let gitti = ertelenen<void>();
  const takilan = (i: Istek): Promise<Response> => {
    const sinyal = i.sinyal;
    assert.ok(sinyal, 'stop bütçesi istemci kaydına ve POST isteğine taşınmalı');
    return new Promise((_coz, ret) => {
      sinyal.addEventListener('abort', () => ret(sinyal.reason), { once: true });
      gitti.coz();
    });
  };
  const k = kur(t, (i) => {
    if (faz === 'post') return takilan(i);
    if (faz === 'govde') {
      const sinyal = i.sinyal;
      assert.ok(sinyal);
      const yanit = new Response(new ReadableStream({
        start(denetleyici) {
          sinyal.addEventListener('abort', () => denetleyici.error(sinyal.reason), { once: true });
        },
      }), { status: 202 });
      const oku = yanit.text.bind(yanit);
      yanit.text = () => { gitti.coz(); return oku(); };
      return yanit;
    }
    return json({ sonuc: 'kuyrukta', komut_no: 41 }, 202);
  }, (i) => faz === 'istemci' ? takilan(i) : json({ sonuc: 'kabul', istemci_id: 7 }));
  k.hazir();
  for (const f of ['istemci', 'post', 'govde'] as const) {
    faz = f;
    gitti = ertelenen<void>();
    k.dugme.tikla();
    await gitti.soz;
    assert.equal(k.komutlar.kayitlar()[0]!.asama, 'gonderiliyor');
    t.mock.timers.tick(4_999);
    assert.equal(k.komutlar.kayitlar()[0]!.asama, 'gonderiliyor');
    const istekSayisi = k.istekler.length;
    t.mock.timers.tick(1);
    const sonuc = await asamaBekle(k.komutlar, 'bilinmiyor');
    assert.equal(sonuc.hata_kodu, 'zaman_asimi');
    assert.match(k.sonuc.textContent, /SONUÇ BİLİNMİYOR/);
    assert.equal(k.istekler.at(-1)!.sinyal!.aborted, true);
    assert.equal(k.dugme.ozellikler.get('aria-disabled'), 'false');
    assert.equal(k.istekler.length, istekSayisi, 'abort otomatik tekrar göndermez');
    assert.equal(k.durum.textContent, 'Sanal girdi: serbest');
  }
  faz = 'tamam';
  k.dugme.tikla();
  await asamaBekle(k.komutlar, 'kuyrukta');
  k.olay();
  await asamaBekle(k.komutlar, 'uygulandi');
  assert.equal(k.komutlar.kayitlar().length, 4, 'her elle tıklama bir yeni kayıt oluşturur');
  assert.equal(basmaIstekleri(k).length, 3, 'istemci kaydı zaman aşımında POST gönderilmez');
  assert.equal(k.istekler.length, 5, 'iki istemci kaydı ve üç elle tetiklenen stop isteği');
});

test('geçersiz acil durdurma telemetrisi basılı diye gösterilmez', (t) => {
  const k = kur(t);
  k.hazir();
  k.cerceve('emniyet/girdiler', { acil_durdurma: 1 }, ['acil_durdurma']);
  assert.equal(k.durum.textContent, 'Sanal girdi: GEÇERSİZ');
  k.cerceve('emniyet/girdiler', { acil_durdurma: 7 });
  assert.equal(k.durum.textContent, 'Sanal girdi: GEÇERSİZ');
  assert.equal(k.istekler.length, 0);
});
