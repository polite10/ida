// W5-web-emniyet: yer istasyonu parametre formunun şema mantığı (DOM'suz).

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { test } from 'node:test';
import { formAlanlari, parametreleriOku, type FormAlani } from '../src/ortak/sema-form';

const KOMUTLAR = (JSON.parse(readFileSync(resolve(process.cwd(), '..', 'protocol', 'komutlar.json'), 'utf8')) as { komutlar: { ad: string; parametreler: unknown }[] }).komutlar;
const sema = (ad: string): unknown => KOMUTLAR.find((k) => k.ad === ad)!.parametreler;
const alanlar = (ad: string): FormAlani[] => {
  const f = formAlanlari(sema(ad));
  assert.ok('alanlar' in f, JSON.stringify(f));
  return (f as { alanlar: FormAlani[] }).alanlar;
};

test('parametresiz komut: alan yok, boş nesne', () => {
  assert.deepEqual(parametreleriOku(alanlar('arm'), {}), { parametreler: {} });
});

test('enum, tamsayı ve sınırlar: set_mode_genisletici, fc_arm, param_get', () => {
  const m = alanlar('set_mode_genisletici');
  assert.equal(m[0]!.tur, 'secim');
  assert.deepEqual(parametreleriOku(m, { mode: 'GUIDED' }), { parametreler: { mode: 'GUIDED' } });
  assert.ok('hatalar' in parametreleriOku(m, { mode: 'MANUAL' }), 'listede olmayan seçenek');
  assert.ok('hatalar' in parametreleriOku(m, {}), 'zorunlu alan boş');
  const t = alanlar('fc_arm');
  assert.deepEqual(parametreleriOku(t, { tube: '1' }), { parametreler: { tube: 1 } });
  assert.ok('hatalar' in parametreleriOku(t, { tube: '2' }), 'üst sınır');
  assert.ok('hatalar' in parametreleriOku(t, { tube: '0.5' }), 'tamsayı değil');
  assert.ok('hatalar' in parametreleriOku(alanlar('param_get'), { param_id: '70000' }));
});

test('sayı dizisi: manual (6 eksen, -1…1) virgülle girilir', () => {
  const a = alanlar('manual');
  assert.deepEqual(parametreleriOku(a, { axes: '0, 0,0.5,0,0,-1' }), { parametreler: { axes: [0, 0, 0.5, 0, 0, -1] } });
  assert.ok('hatalar' in parametreleriOku(a, { axes: '0,0,0' }), 'öğe sayısı');
  assert.ok('hatalar' in parametreleriOku(a, { axes: '0,0,0,0,0,2' }), 'aralık dışı');
  assert.ok('hatalar' in parametreleriOku(a, { axes: '0,0,0,0,0,x' }));
});

test('guided_sp: enum dizisi ve isteğe bağlı alanlar atlanır', () => {
  const a = alanlar('guided_sp');
  const r = parametreleriOku(a, { mask: 'DEPTH, YAW', depth: '2,5' });
  assert.deepEqual(r, { parametreler: { mask: ['DEPTH', 'YAW'], depth: 2.5 } });
  assert.ok('hatalar' in parametreleriOku(a, { mask: 'DEPTH,UCMAK' }));
});

test('nesne dizisi içeren komutlar desteklenmiyor diye bildirilir (forma dökülmez)', () => {
  for (const ad of ['mission_upload', 'param_apply', 'param_save']) {
    const f = formAlanlari(sema(ad));
    assert.ok('desteklenmiyor' in f, `${ad}: ${JSON.stringify(f)}`);
  }
});
