// W5-web-emniyet: ortak durum şeridindeki "ACİL DURDUR" düğmesi (src/ortak/acil.ts), gerçek bir tarayıcı
// olmadan, küçük bir sahte belge (DOM) üzerinde: emniyet-istek.test.ts veri katmanını, bu dosya ekranda
// yazılan metni sınar. Sahte belge yalnız `el`, `yaz` ve `sinifSec` yardımcılarının dokunduğu işlemleri
// (createElement, setAttribute, append, textContent, classList.toggle, addEventListener) taşır; sayfa
// düzeni, stil ve `emniyet` panelinin kendisi burada sınanmaz.
// Beklemeler olaya göredir (W5-K43): kanal çerçevesi, şerit metni, bağlantı durumu.

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { acilDurdurmaSeridi } from '../src/ortak/acil';
import { kosul } from './yardimci-komut';
import { bellekKur, KALDIR, kapi } from './yardimci-emniyet';

class SahteDugum {
  className = '';
  readonly cocuklar: (SahteDugum | string)[] = [];
  private readonly ozellikler = new Map<string, string>();
  private readonly siniflar = new Set<string>();
  private readonly dinleyiciler = new Map<string, (() => void)[]>();

  constructor(readonly etiket: string) {}

  readonly classList = {
    toggle: (s: string, zorla?: boolean): boolean => {
      const ac = zorla ?? !this.siniflar.has(s);
      if (ac) this.siniflar.add(s);
      else this.siniflar.delete(s);
      return ac;
    },
  };

  setAttribute(ad: string, deger: string): void {
    this.ozellikler.set(ad, deger);
  }

  getAttribute(ad: string): string | null {
    return this.ozellikler.get(ad) ?? null;
  }

  append(...c: (SahteDugum | string)[]): void {
    this.cocuklar.push(...c);
  }

  get textContent(): string {
    return this.cocuklar.map((c) => (typeof c === 'string' ? c : c.textContent)).join('');
  }

  set textContent(v: string) {
    this.cocuklar.length = 0;
    if (v !== '') this.cocuklar.push(v);
  }

  addEventListener(tur: string, f: () => void): void {
    this.dinleyiciler.set(tur, [...(this.dinleyiciler.get(tur) ?? []), f]);
  }

  /** Kullanıcı tıklaması. */
  tikla(): void {
    for (const f of this.dinleyiciler.get('click') ?? []) f();
  }

  sinifVar(s: string): boolean {
    return this.siniflar.has(s) || this.className.split(/\s+/).includes(s);
  }

  bul(sinif: string): SahteDugum {
    const r = this.ara(sinif);
    assert.ok(r !== null, `şeritte "${sinif}" öğesi yok`);
    return r;
  }

  private ara(sinif: string): SahteDugum | null {
    if (this.sinifVar(sinif)) return this;
    for (const c of this.cocuklar) {
      if (typeof c === 'string') continue;
      const r = c.ara(sinif);
      if (r !== null) return r;
    }
    return null;
  }
}

(globalThis as unknown as { document: unknown }).document = { createElement: (etiket: string) => new SahteDugum(etiket) };

async function yeni() {
  const k = await bellekKur();
  const kok = acilDurdurmaSeridi(k.baglanti, k.em, k.komutlar).kok as unknown as SahteDugum;
  return { ...k, dugme: kok.bul('acil-dugme'), gosterge: kok.bul('acil-durum'), sonuc: kok.bul('acil-sonuc') };
}

test('ACİL DURDUR: gösterge sunucu verisi gelmeden değişmez', async (t) => {
  const o = await yeni();
  t.after(o.kapat);
  assert.equal(o.dugme.textContent, 'ACİL DURDUR');
  const ornekler: string[] = [];
  o.komutlar.dinle(() => ornekler.push(o.gosterge.textContent));
  o.dugme.tikla();
  ornekler.push(o.gosterge.textContent);
  await kosul(() => o.sonuc.textContent.startsWith('KUYRUKTA'), 3000, 'şeritte KUYRUKTA');
  o.uygulandi(); // Sonuç olayı da tek başına göstergeyi değiştiremez.
  assert.ok(o.sonuc.textContent.startsWith('UYGULANDI'));
  assert.equal(o.gosterge.textContent, 'sunucu: serbest');
  assert.deepEqual([...new Set(ornekler)], ['sunucu: serbest']);
  o.bildir(1, 2);
  assert.equal(o.gosterge.textContent, 'sunucu: BASILI');
  assert.equal(o.post('/emniyet/degistir'), 1);
  assert.equal(o.post('/emniyet/onay_jetonu'), 0);
});

test('ACİL DURDUR: bağlantı kopukken şerit UYGULANMADI yazar, gösterge bayat işaretlenir', async (t) => {
  const o = await yeni();
  t.after(o.kapat);
  o.baglanti.kapat();
  // HTTP taşıması hâlâ yanıt verebilir; kopuk bağlantıda hiç istek başlatılmaz.
  o.dugme.tikla();
  await kosul(() => o.sonuc.textContent.startsWith('UYGULANMADI'), 3000, 'şeritte UYGULANMADI');
  assert.match(o.sonuc.textContent, /gönderil/);
  assert.match(o.gosterge.textContent, /^sunucu: serbest.*BAYAT/);
  assert.equal(o.post('/emniyet/degistir'), 0);
});

test('ACİL DURDUR: bağlıyken yanıt kaybolursa SONUÇ BİLİNMİYOR yazar ve istek yinelenmez', async (t) => {
  const o = await yeni();
  t.after(o.kapat);
  o.ag.yanitKayip = true;
  o.dugme.tikla();
  await kosul(() => o.sonuc.textContent.startsWith('SONUÇ BİLİNMİYOR'), 3000, 'şeritte SONUÇ BİLİNMİYOR');
  assert.equal(o.gosterge.textContent, 'sunucu: serbest');
  o.bildir(0);
  assert.equal(o.post('/emniyet/degistir'), 1);
});

test('bellek: azaltan yön jetonsuz gönderilemez', async (t) => {
  const o = await yeni();
  t.after(o.kapat);
  const r = await o.em.gevset(KALDIR, true);
  assert.equal(r.hata_kodu, 'jeton_yok');
  assert.equal(r.gonderildi_mi, false);
  assert.equal(o.post('/emniyet/degistir'), 0);
});

test('bellek: süresi dolmuş jetonla istek yinelenmez', async (t) => {
  const o = await yeni();
  t.after(o.kapat);
  assert.ok('jeton' in await o.em.jetonIste(KALDIR));
  o.saat.t += 10000;
  const dolmus = await o.em.gevset(KALDIR, true);
  assert.equal(dolmus.gonderildi_mi, false);
  assert.equal(o.post('/emniyet/degistir'), 0);
  // Sunucu ret verirse de otomatik yeni jeton ya da yeniden gönderim yoktur.
  assert.ok('jeton' in await o.em.jetonIste(KALDIR));
  const r = await o.em.gevset(KALDIR, true);
  assert.equal(r.hata_kodu, 'onay_jetonu_gecersiz');
  assert.equal(o.em.jeton(), null);
  o.bildir(0);
  assert.equal((await o.em.gevset(KALDIR, true)).gonderildi_mi, false);
  assert.equal(o.post('/emniyet/degistir'), 1);
  assert.equal(o.post('/emniyet/onay_jetonu'), 2);
});

test('bellek: jeton yanıtını beklerken bağlantı koparsa geç yanıt onayı geri getirmez', async (t) => {
  const o = await yeni();
  t.after(o.kapat);
  const gecikme = kapi<void>();
  o.jeton.beklet = gecikme.soz;
  const bekleyen = o.em.jetonIste(KALDIR);
  await kosul(() => o.post('/emniyet/onay_jetonu') === 1, 3000, 'jeton isteği alındı');
  o.baglanti.kapat();
  gecikme.ac();
  assert.ok('hata' in await bekleyen);
  assert.equal(o.em.jeton(), null);
  assert.equal((await o.em.gevset(KALDIR, true)).gonderildi_mi, false);
});

for (const neden of ['vazgec', 'yeni_istek', 'revizyon', 'sure'] as const) {
  test(`bellek: bekleyen jeton yanıtı ${neden} sonrasında etkinleşmez`, async (t) => {
    const o = await yeni();
    t.after(o.kapat);
    const gecikme = kapi<void>();
    o.jeton.beklet = gecikme.soz;
    const eski = o.em.jetonIste(KALDIR);
    await kosul(() => o.post('/emniyet/onay_jetonu') === 1, 3000, 'ilk jeton isteği alındı');
    if (neden === 'vazgec') o.em.vazgec();
    if (neden === 'revizyon') o.bildir(0, 2);
    if (neden === 'sure') o.saat.t += 10000;
    if (neden === 'yeni_istek') {
      o.jeton.beklet = null;
      o.jeton.kod = 'b'.repeat(32);
      assert.ok('jeton' in await o.em.jetonIste(KALDIR));
    }
    gecikme.ac();
    assert.ok('hata' in await eski);
    assert.equal(o.em.jeton()?.jeton ?? null, neden === 'yeni_istek' ? 'b'.repeat(32) : null);
    assert.equal(o.post('/emniyet/degistir'), 0);
  });
}
