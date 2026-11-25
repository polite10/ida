// A3 §3.9 hata kodlarının Türkçe iletileri (şemadaki kod listesiyle bire bir).

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { A3Hatasi } from '../src/a3/http';
import { hataIletisi, hatadanIleti, ucYok } from '../src/a3/hata';
import { SEMA } from './yardimci-komut';

type RetSemasi = { properties: { hata: { properties: { kod: { enum: string[] } } } } };

test('§3.9 şemasındaki her hata kodunun okunur Türkçe iletisi var', () => {
  const kodlar = (SEMA.$defs.yanit_ret as unknown as RetSemasi).properties.hata.properties.kod.enum;
  assert.ok(kodlar.length >= 30);
  for (const k of kodlar) {
    const m = hataIletisi(k, 'sunucu iletisi');
    assert.ok(m.length > 15, k);
    assert.ok(!m.includes('Bilinmeyen hata kodu'), k);
    assert.ok(!m.includes('sunucu iletisi'), `${k}: bilinen kod istemcinin Türkçe iletisiyle gösterilir`);
    assert.ok(/[a-zçğıöşü]/.test(m) && /[.)]$/.test(m), `${k}: tam cümle`);
  }
});

test('bilinmeyen kod sunucunun iletisiyle gösterilir (A3-K18); ileti yoksa kod yazılır', () => {
  assert.equal(hataIletisi('yeni_kod_1_1', 'Yeni bir durum'), 'Yeni bir durum (yeni_kod_1_1)');
  assert.match(hataIletisi('yeni_kod_1_1'), /yeni_kod_1_1/);
});

test('ayrıntılı iletiler: revizyon ve çalıştırma uyuşmazlığı güncel değeri söyler', () => {
  const m = hataIletisi('beklenen_durum_uyusmuyor', null, { guncel_run_id: '20261125-101010-abcdef', guncel_durum: 'kosuyor' });
  assert.match(m, /güncel durum: kosuyor/);
  assert.match(m, /20261125-101010-abcdef/);
  assert.match(hataIletisi('durum_rev_uyusmuyor', null, { guncel_durum_rev: 42 }), /güncel revizyon: 42/);
});

test('A3Hatasi çevirisi ve "uç yok" ayrımı (W5-K45)', () => {
  assert.match(hatadanIleti(new A3Hatasi('ag_hatasi', 'x', null)), /bilinmiyor/);
  assert.equal(ucYok(new A3Hatasi('bulunamadi', 'x', 404)), true);
  assert.equal(ucYok(new A3Hatasi('yontem_desteklenmiyor', 'x', 405)), true);
  assert.equal(ucYok(new A3Hatasi('gecersiz_durum', 'x', 409)), false);
  assert.equal(ucYok(new A3Hatasi('ag_hatasi', 'x', null)), false);
  assert.equal(ucYok(null), false);
});
