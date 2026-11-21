// Kabul 2 (elle): sözleşmedeki alan düzeninden elle kurulmuş bayt dizileri.
// Değerler A3 §3.2 ve §3.4 tablolarından satır satır yazıldı.

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { BAYRAK, Cozucu, type Cerceve } from '../src/a3/cerceve';
import type { YukCalistirmaDurum, YukEmniyetGirdiler, YukFizikDurum, YukOlay, YukPing360, YukAtisDurum } from '../src/a3/katalog';
import { Bayt, cerceve, f32 } from './yardimci';

function tek<T>(veri: Uint8Array): Cerceve<T> {
  const c = new Cozucu();
  const out = c.ileti(veri);
  assert.equal(out.length, 1);
  assert.equal(c.sayaclar.istemci_bozuk_cerceve, 0);
  return out[0] as Cerceve<T>;
}

test('başlık: 24 bayt, alan konumları ve bayraklar (A3 §3.2)', () => {
  const yuk = new Bayt().u8(3).u8(1).metin16('').u64(0n).f32(1).f32(Number.NaN).u8(1).u8(0).u32(0).u8(0).u8(0).u8(0).u8(0).u8(0).u8(255).u32(0).u64(1n).bitir();
  const v = cerceve(123_456_789_012n, 42, 1, yuk, 1, BAYRAK.OZET | BAYRAK.KAYNAK_KAYIT);
  assert.equal(v.length, 24 + yuk.length);
  // elle: yuk_uzunlugu 12. baytta, kanal_id 16'da
  assert.equal(new DataView(v.buffer).getUint32(12, true), yuk.length);
  assert.equal(new DataView(v.buffer).getUint16(16, true), 1);
  const c = tek<YukCalistirmaDurum>(v);
  assert.equal(c.baslik.t_ns, 123_456_789_012n);
  assert.equal(c.baslik.sira, 42);
  assert.equal(c.baslik.kanal_id, 1);
  assert.equal(c.baslik.kanal_surumu, 1);
  assert.equal(c.baslik.bayraklar, 3);
  assert.equal(c.kanal.ad, 'calistirma/durum');
});

test('yuk_calistirma_durum: 17 alan sırayla', () => {
  const yuk = new Bayt()
    .u8(3) // durum = kosuyor
    .u8(2) // kip = hil_1x
    .metin16('20261121-101500-a1b2c3') // run_id
    .u64(80_000n) // adim_no
    .f32(1) // hedef_carpan
    .f32(0.98) // gerceklesen_carpan
    .u8(0) // gecerli = gecersiz
    .u8(1) // gercek_zamani_kacirdi
    .u32((1 << 0) | (1 << 16)) // gecerlilik_nedenleri
    .u8(2) // tekrar_guvencesi
    .u8(1)
    .u8(0)
    .u8(1)
    .u8(1) // kademeler
    .u8(3) // a1_oturum = kosuyor
    .u32(17) // son_komut_no
    .u64(9n) // durum_rev
    .bitir();
  const c = tek<YukCalistirmaDurum>(cerceve(100_000_000n, 1, 1, yuk));
  assert.deepEqual(c.gecersiz, []);
  assert.deepEqual(c.yuk, {
    durum: 3,
    kip: 2,
    run_id: '20261121-101500-a1b2c3',
    adim_no: 80000n,
    hedef_carpan: 1,
    gerceklesen_carpan: f32(0.98),
    gecerli: 0,
    gercek_zamani_kacirdi: 1,
    gecerlilik_nedenleri: 0x10001,
    tekrar_guvencesi: 2,
    kademe_gorunum: 1,
    kademe_kamera: 0,
    kademe_sonar: 1,
    kademe_kayit: 1,
    a1_oturum: 3,
    son_komut_no: 17,
    durum_rev: 9n,
  });
});

test('yuk_olay: yok değeri 2^64-1 → null, ayrıntı UTF-8', () => {
  const ayrinti = '{"girdi":"emniyet_pimi","tup":"f2","neden":"çıkarıldı"}';
  const yuk = new Bayt().u16(7).u8(3).u8(2).u64(0xffffffffffffffffn).u32(5).u32(2).metin16(ayrinti).bitir();
  const c = tek<YukOlay>(cerceve(5n, 77, 3, yuk));
  assert.deepEqual(c.yuk, { tur: 7, onem: 3, kaynak: 2, adim_no: null, komut_no: 5, istemci_id: 2, ayrinti });
  assert.equal(c.baslik.sira, 77);
});

test('yuk_fizik_durum: iç içe diziler (dizi(u16), dizi(u8)), f64[n]', () => {
  const b = new Bayt().u64(400n).u16(1);
  b.u16(1).u8(0).u8(1); // govde_id, tur=arac, kutle_durumu=bir_torpido
  [1.5, -2.25, 3].forEach((x) => b.f64(x));
  [1, 0, 0, 0].forEach((x) => b.f64(x));
  [0.5, 0, 0.1, 0, 0, 0.02].forEach((x) => b.f64(x));
  [0, 0, 0].forEach((x) => b.f64(x));
  b.u8(2); // iticiler
  b.f32(0.25).f32(10).f32(2.5);
  b.f32(-1).f32(-40).f32(17);
  b.f32(15.5).f32(12).f32(0.75);
  const c = tek<YukFizikDurum>(cerceve(500_000_000n, 3, 10, b.bitir(), 1, BAYRAK.YER_GERCEGI));
  assert.equal(c.yuk.adim_no, 400n);
  assert.equal(c.yuk.govdeler.length, 1);
  assert.deepEqual(c.yuk.govdeler[0], { govde_id: 1, tur: 0, kutle_durumu: 1, p_n: [1.5, -2.25, 3], q_nb: [1, 0, 0, 0], nu: [0.5, 0, 0.1, 0, 0, 0.02], a_n: [0, 0, 0] });
  assert.deepEqual(c.yuk.iticiler, [
    { u: 0.25, itki_n: 10, akim_a: 2.5 },
    { u: -1, itki_n: -40, akim_a: 17 },
  ]);
  assert.equal(c.yuk.bara_gerilimi_v, 15.5);
  assert.equal(c.yuk.soc, 0.75);
  assert.equal(c.baslik.bayraklar & BAYRAK.YER_GERCEGI, BAYRAK.YER_GERCEGI);
});

test('yuk_emniyet_girdiler: tüp başına u8[2], 255 = bilinmiyor → null', () => {
  const yuk = new Bayt()
    .u8(0) // acil_durdurma serbest
    .u8(1)
    .u8(0) // silahlandirma_fisi f1 takili, f2 cikarilmis
    .u8(1)
    .u8(1) // emniyet_pimi
    .u64(0xffffffffffffffffn) // son_degisim_adimi yok
    .u8(2) // operator
    .u8(255) // kart_acil_durdurma bilinmiyor
    .u8(1)
    .u8(255)
    .u8(0)
    .u8(1)
    .u8(0b00110) // geri_okuma_farki
    .u32(12)
    .u8(0)
    .bitir();
  const c = tek<YukEmniyetGirdiler>(cerceve(1n, 1, 14, yuk));
  assert.deepEqual(c.gecersiz, []);
  assert.deepEqual(c.yuk, {
    acil_durdurma: 0,
    silahlandirma_fisi: [1, 0],
    emniyet_pimi: [1, 1],
    son_degisim_adimi: null,
    son_degisim_kaynagi: 2,
    kart_acil_durdurma: null,
    kart_silahlandirma_fisi: [1, null],
    kart_emniyet_pimi: [0, 1],
    geri_okuma_farki: 6,
    emniyet_rev: 12,
    emniyet_rev_doydu: 0,
  });
});

test('yuk_atis_durum: yok değerleri (255, 0xFFFFFFFF, 0, -1)', () => {
  const yuk = new Bayt().u8(255).u32(0xffffffff).u64(0n).u8(2);
  yuk.u8(0).u8(1).u8(0).i64(-1n).u8(0);
  yuk.u8(1).u8(1).u8(1).i64(2_500_000_000n).u8(3);
  const c = tek<YukAtisDurum>(cerceve(1n, 1, 13, yuk.bitir()));
  assert.equal(c.yuk.fc_durumu, null);
  assert.equal(c.yuk.engel_maskesi, null);
  assert.equal(c.yuk.kart_son_olay_kimligi, null);
  assert.deepEqual(c.yuk.tupler, [
    { tup: 0, dolu: 1, sureli_izin_gecerli: 0, izin_kalan_ns: null, son_olay_sonucu: 0 },
    { tup: 1, dolu: 1, sureli_izin_gecerli: 1, izin_kalan_ns: 2_500_000_000n, son_olay_sonucu: 3 },
  ]);
});

test('yuk_ping360: ölçüm başlığı (12 alan) + cihaz birimleri + bayt_dizisi', () => {
  const ornek = Uint8Array.from({ length: 300 }, (_, i) => i % 256);
  const yuk = new Bayt()
    .u16(7) // sensor_id
    .u8(0) // inst
    .u32(1001) // seq
    .u16(1) // flags
    .i64(0n) // window_ns
    .i64(9_000_000_000n) // t_tx_ns
    .i64(9_040_000_000n) // t_hazir_ns
    .i64(9_042_000_000n) // t_teslim_ns
    .u32(0xffffffff) // parti_step_seq yok
    .u32(0) // fault_mask
    .f32(Number.NaN) // quality yok
    .u16(3) // model_version
    .u16(399) // aci_grad
    .u16(300) // n_ornek
    .u16(80) // ornek_periyodu
    .u16(80) // darbe_suresi_us
    .u16(750) // frekans_khz
    .u8(1) // kazanc
    .u8(2) // kademe
    .f32(1481) // ses_hizi_mps
    .u16(300)
    .ham(ornek)
    .bitir();
  const c = tek<YukPing360>(cerceve(9_000_000_000n, 5, 30, yuk));
  assert.deepEqual(c.gecersiz, []);
  const y = c.yuk;
  assert.equal(y.sensor_id, 7);
  assert.equal(y.seq, 1001);
  assert.equal(y.t_tx_ns, 9_000_000_000n);
  assert.equal(y.parti_step_seq, null);
  assert.equal(y.quality, null);
  assert.equal(y.aci_grad, 399);
  assert.equal(y.ses_hizi_mps, 1481);
  assert.deepEqual(Array.from(y.ornekler), Array.from(ornek));
});
