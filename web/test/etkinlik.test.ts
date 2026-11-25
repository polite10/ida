// Düğme etkinliği: A3 Tablo 4.7a (durum × komut) ve Tablo 4.7b (kipe göre komutlar).
// Beklenen tablo sözleşmeden elle yazılmıştır (kodu yansıtmaz): satır = durum, sütun = komut.

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { dugmeler, type DugmeAdi } from '../src/a3/etkinlik';
import type { CalistirmaDurumAdi } from '../src/a3/iletiler';
import { carpanOku, komutSatiri, periyotSayisiOku } from '../src/ortak/denetim';
import type { KomutKaydi } from '../src/a3/komut';

const SUTUNLAR: DugmeAdi[] = ['yukle', 'baslat', 'duraklat', 'surdur', 'adimla', 'hiz', 'durdur', 'sifirla'];
// 1 = Tablo 4.7a'da "—" olmayan hücre
const TABLO: Record<CalistirmaDurumAdi, string> = {
  //                 yukle baslat duraklat surdur adimla hiz durdur sifirla
  bos: /*          */ '1     0      0        0      0     0    0      0',
  yuklendi: /*     */ '1     1      0        0      0     0    0      1',
  hazirlaniyor: /* */ '0     0      0        0      0     0    1      1',
  kosuyor: /*      */ '0     0      1        0      0     1    1      1',
  duraklatildi: /* */ '0     0      0        1      1     1    1      1',
  bitti: /*        */ '1     0      0        0      0     0    0      1',
  durduruldu: /*   */ '1     0      0        0      0     0    0      1',
  hata: /*         */ '1     0      0        0      0     0    0      1',
  izleme: /*       */ '0     0      0        0      0     0    0      0',
};

for (const [durum, satir] of Object.entries(TABLO) as [CalistirmaDurumAdi, string][]) {
  test(`Tablo 4.7a: ${durum} durumunda (SIL) hangi düğme etkin`, () => {
    const beklenen = satir.trim().split(/\s+/).map((x) => x === '1');
    const g = dugmeler(durum, 'sil');
    SUTUNLAR.forEach((s, i) => {
      assert.equal(g[s].etkin, beklenen[i], `${durum} · ${s}`);
      if (!g[s].etkin) assert.ok(g[s].neden.length > 10, `${durum} · ${s}: etkin olmayan düğme nedenini söyler`);
      if (!g[s].etkin) assert.ok(!/^\s*$/.test(g[s].neden));
    });
    assert.equal(g.duraklat.onay, null, 'SIL kipinde duraklat onay istemez');
  });
}

test('Tablo 4.7a: nedenler durumu ve gerekeni söyler (örnekler)', () => {
  assert.match(dugmeler('kosuyor', 'sil').surdur.neden, /koşuyor.*duraklatılmışken/);
  assert.match(dugmeler('yuklendi', 'sil').duraklat.neden, /yüklendi/);
  assert.match(dugmeler('izleme', 'sil').baslat.neden, /izleme/);
  assert.match(dugmeler('bitti', 'sil').baslat.neden, /yüklenmiş bir senaryo/);
});

for (const kip of ['hil_1x', 'hil_bariyer'] as const) {
  test(`Tablo 4.7b: ${kip} kipinde adımlama ve hız yok, duraklatma onay ister`, () => {
    const dur = dugmeler('duraklatildi', kip);
    assert.equal(dur.adimla.etkin, false);
    assert.match(dur.adimla.neden, /HIL/);
    assert.equal(dur.hiz.etkin, false);
    assert.equal(dur.surdur.etkin, true, 'sürdür HIL\'de geçerli ve onay istemez');
    assert.equal(dur.surdur.onay, null);
    const kos = dugmeler('kosuyor', kip);
    assert.equal(kos.hiz.etkin, false);
    assert.equal(kos.duraklat.etkin, true);
    assert.equal(kos.duraklat.onay, 'gecersizlik');
    assert.equal(kos.durdur.etkin, true);
    assert.equal(kos.sifirla.etkin, true);
  });
}

for (const kip of ['tekrar_fizik', 'tekrar_girdi_sil', 'tekrar_yeniden_kurma'] as const) {
  test(`Tablo 4.7b: ${kip} SIL kuralını izler (adımla ve hız geçerli, onay yok)`, () => {
    const g = dugmeler('duraklatildi', kip);
    assert.equal(g.adimla.etkin, true);
    assert.equal(g.hiz.etkin, true);
    assert.equal(dugmeler('kosuyor', kip).duraklat.onay, null);
  });
}

test('sunucudan durum gelmediyse hiçbir düğme etkin değil (iyimser varsayım yok; W5-K44)', () => {
  for (const g of [dugmeler(null, null), dugmeler('kosuyor', null), dugmeler(null, 'sil')]) {
    for (const s of SUTUNLAR) {
      assert.equal(g[s].etkin, false);
      assert.match(g[s].neden, /sunucudan henüz durum gelmedi/);
    }
  }
});

test('adım sayısı ve hız çarpanı girdi denetimi (A3-K65, K66)', () => {
  assert.deepEqual(periyotSayisiOku('1'), { deger: 1 });
  assert.deepEqual(periyotSayisiOku(' 10000 '), { deger: 10000 });
  for (const kotu of ['0', '10001', '-3', '1.5', 'abc', '', '1e3']) assert.ok('hata' in periyotSayisiOku(kotu), kotu);
  assert.deepEqual(carpanOku('0'), { deger: 0 });
  assert.deepEqual(carpanOku('0,5'), { deger: 0.5 });
  assert.deepEqual(carpanOku('100'), { deger: 100 });
  assert.deepEqual(carpanOku('0.01'), { deger: 0.01 });
  for (const kotu of ['0.005', '100.5', '-1', 'x', '']) assert.ok('hata' in carpanOku(kotu), kotu);
});

test('komut satırı: gönderildi / kuyrukta / uygulandı / reddedildi ayrı yazılır', () => {
  const k = (a: Partial<KomutKaydi>): KomutKaydi => ({
    yerel: 1, komut: 'duraklat', etiket: 'Duraklat', asama: 'gonderiliyor', komut_no: null, hata_kodu: null, ileti: '', uygulama_adimi: null,
    gonderildi: 0, sonuc_zamani: null, gonderildi_mi: true, ozet: '', ...a,
  });
  assert.match(komutSatiri(k({})), /gönderiliyor/);
  assert.match(komutSatiri(k({ asama: 'kuyrukta', komut_no: 7 })), /#7 — kuyrukta · uygulanmadı/);
  assert.match(komutSatiri(k({ asama: 'uygulandi', komut_no: 7, uygulama_adimi: 120 })), /uygulandı \(adım 120\)/);
  assert.match(komutSatiri(k({ asama: 'reddedildi', komut_no: 7, ileti: 'neden' })), /reddedildi: neden/);
  assert.match(komutSatiri(k({ asama: 'bilinmiyor', ileti: 'Bağlantı koptu' })), /sonuç bilinmiyor: Bağlantı koptu/);
});
