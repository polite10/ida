// Kabul 2 (ret): bozuk ve kısa çerçeve reddedilir; A3-K18, K19, K24, K25.

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Cozucu } from '../src/a3/cerceve';
import type { YukCalistirmaDurum, YukOlay, YukSizinti } from '../src/a3/katalog';
import { Bayt, baslik, birlestir, cerceve } from './yardimci';

function sizinti(prob = 1): Uint8Array {
  return new Bayt().u16(9).u8(0).u32(1).u16(1).i64(0n).i64(-(1n << 63n)).i64(0n).i64(0n).u32(0xffffffff).u32(0).f32(Number.NaN).u16(1).u8(prob).bitir();
}

test('geçerli çerçeve ile bozuk çerçeve aynı iletide: bozuk atılır, sonraki okunur (A3-K25)', () => {
  const iyi = cerceve(1n, 1, 26, sizinti());
  // yuk_uzunlugu 3 ama yük 3 baytta bitemez: okuma uzunluğu aşar
  const bozukYuk = new Bayt().u16(9).u8(0).bitir();
  const bozuk = cerceve(2n, 2, 26, bozukYuk);
  const c = new Cozucu();
  const out = c.ileti(birlestir(bozuk, iyi));
  assert.equal(out.length, 1);
  assert.equal(c.sayaclar.istemci_bozuk_cerceve, 1);
  assert.equal((out[0]!.yuk as YukSizinti).prob_maskesi, 1);
});

test('dizi sayacı yükün dışına taşan çerçeve bozuktur', () => {
  // olay: metin uzunluğu 100 ama 3 bayt var
  const yuk = new Bayt().u16(1).u8(0).u8(0).u64(0n).u32(0).u32(0).u16(100).ham([123, 125, 32]).bitir();
  const c = new Cozucu();
  assert.equal(c.ileti(cerceve(1n, 1, 3, yuk)).length, 0);
  assert.equal(c.sayaclar.istemci_bozuk_cerceve, 1);
});

test('ileti sonunda 24 bayttan kısa kalan atılır', () => {
  const iyi = cerceve(1n, 1, 26, sizinti());
  const c = new Cozucu();
  const out = c.ileti(birlestir(iyi, new Uint8Array(10)));
  assert.equal(out.length, 1);
  assert.equal(c.sayaclar.istemci_kisa_kalan, 1);
});

test('yuk_uzunlugu iletinin dışına taşıyorsa kalan atılır', () => {
  const b = baslik(1n, 1, 500, 26).ham(sizinti()).bitir();
  const c = new Cozucu();
  assert.equal(c.ileti(b).length, 0);
  assert.equal(c.sayaclar.istemci_bozuk_cerceve, 1);
});

test('yuk_uzunlugu 65 536 üstü bozuk sayılır ve atlanır', () => {
  const buyuk = cerceve(1n, 1, 26, new Uint8Array(65537));
  const iyi = cerceve(1n, 2, 26, sizinti(4));
  const c = new Cozucu();
  const out = c.ileti(birlestir(buyuk, iyi));
  assert.equal(out.length, 1);
  assert.equal(c.sayaclar.istemci_bozuk_cerceve, 1);
});

test('bilinmeyen kanal yuk_uzunlugu kadar atlanır, bağlantı sürer (A3-K18)', () => {
  const c = new Cozucu();
  const out = c.ileti(birlestir(cerceve(1n, 1, 999, new Uint8Array([1, 2, 3, 4, 5])), cerceve(1n, 2, 26, sizinti(2))));
  assert.equal(out.length, 1);
  assert.equal(c.sayaclar.istemci_bilinmeyen_kanal, 1);
});

test('aralık/değer listesi dışı ve geçersiz UTF-8 alan "geçersiz" gösterilir, çerçeve atılmaz (A3-K24)', () => {
  const yuk = new Bayt()
    .u8(42) // durum: değer listesinde yok
    .u8(1)
    .u16(2)
    .ham([0xc3, 0x28]) // geçersiz UTF-8
    .u64(0n)
    .f32(250) // hedef_carpan aralık [0, 100] dışı
    .f32(1)
    .u8(1)
    .u8(0)
    .u32(0)
    .u8(0)
    .u8(0)
    .u8(0)
    .u8(0)
    .u8(0)
    .u8(255)
    .u32(0)
    .u64(1n)
    .bitir();
  const c = new Cozucu();
  const out = c.ileti(cerceve(1n, 1, 1, yuk));
  assert.equal(out.length, 1);
  assert.deepEqual([...out[0]!.gecersiz].sort(), ['durum', 'hedef_carpan', 'run_id']);
  assert.equal(c.sayaclar.istemci_gecersiz_alan, 3);
  assert.equal((out[0]!.yuk as YukCalistirmaDurum).durum, 42);
});

test('azami öğe sayısını aşan metin geçersiz gösterilir', () => {
  const uzun = 'x'.repeat(40); // run_id en çok 32
  const yuk = new Bayt().u8(0).u8(0).metin16(uzun).u64(0n).f32(0).f32(0).u8(1).u8(0).u32(0).u8(0).u8(0).u8(0).u8(0).u8(0).u8(255).u32(0).u64(1n).bitir();
  const out = new Cozucu().ileti(cerceve(1n, 1, 1, yuk));
  assert.deepEqual(out[0]!.gecersiz, ['run_id']);
});

test('bildiğinden büyük kanal_surumu: sondaki fazla baytlar yok sayılır (A3-K19)', () => {
  const yuk = birlestir(sizinti(5), new Uint8Array([9, 9, 9, 9]));
  const out = new Cozucu().ileti(cerceve(1n, 1, 26, yuk, 2));
  assert.equal(out.length, 1);
  assert.equal((out[0]!.yuk as YukSizinti).prob_maskesi, 5);
});

test('kanal_surumu 0 (1. sürümden küçük) bozuktur; 1. sürüm alanı eksik nesne panele ulaşmaz (A3-K14, K19)', () => {
  const tam = new Bayt().u16(7).u8(1).u8(0).u64(5n).u32(0).u32(0).bitir(); // ayrinti yok
  const c = new Cozucu();
  assert.equal(c.ileti(cerceve(1n, 1, 3, tam, 0)).length, 0);
  assert.equal(c.sayaclar.istemci_bozuk_cerceve, 1);
  const tamYuk = new Bayt().u16(7).u8(1).u8(0).u64(5n).u32(0).u32(0).metin16('{}').bitir();
  const out = new Cozucu().ileti(cerceve(1n, 1, 3, tamYuk, 0));
  assert.equal(out.length, 0, 'sürüm 0 tam yükle de bozuk');
  assert.equal((new Cozucu().ileti(cerceve(1n, 1, 3, tamYuk, 1))[0]!.yuk as YukOlay).ayrinti, '{}');
});

test('aynı sürümde eksik alan bozuktur', () => {
  const tam = new Bayt().u16(7).u8(1).u8(0).u64(5n).u32(0).u32(0).bitir();
  const c = new Cozucu();
  assert.equal(c.ileti(cerceve(1n, 1, 3, tam, 1)).length, 0);
  assert.equal(c.sayaclar.istemci_bozuk_cerceve, 1);
});
