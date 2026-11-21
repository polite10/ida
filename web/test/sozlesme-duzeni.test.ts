// Kabul 2 (her yük türü): A3 sözleşmesinin METNİNDEKİ (docs/sozlesmeler/A3-web.md
// §3.4) yük tablolarından alan düzeni okunur, her yük türü için bu düzenle bayt
// dizisi kurulur ve istemci çözücüsüyle (makinece okunur katalogdan üretilmiş
// düzen) çözülür. Böylece katalog ile sözleşme metni de birbirine karşı denenir.

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { test } from 'node:test';
import { Cozucu } from '../src/a3/cerceve';
import { KANALLAR, YUKLER } from '../src/a3/katalog';
import { Bayt, cerceve } from './yardimci';

interface MdAlan {
  ad: string;
  tip: string;
  aralik: string;
  alt: MdAlan[];
}

const metin = readFileSync(resolve(process.cwd(), '..', 'docs', 'sozlesmeler', 'A3-web.md'), 'utf8');

function tabloSatirlari(bolum: string): MdAlan[] {
  const ust: MdAlan[] = [];
  for (const satir of bolum.split('\n')) {
    if (!satir.startsWith('| `')) continue;
    const h = satir.split('|').map((x) => x.trim());
    const ad = h[1]!.replace(/`/g, '');
    const alan: MdAlan = { ad, tip: h[2]!, aralik: h[4]!, alt: [] };
    const m = /^(.+)\[\]\.(.+)$/.exec(ad);
    if (m) {
      const ebeveyn = ust.find((x) => x.ad === m[1]);
      assert.ok(ebeveyn, `dizi alanı bulunamadı: ${ad}`);
      ebeveyn.alt.push({ ...alan, ad: m[2]! });
    } else ust.push(alan);
  }
  return ust;
}

function bolumler(): Map<string, MdAlan[]> {
  const sonuc = new Map<string, MdAlan[]>();
  const parca = metin.split(/\n(?=#### )/);
  let olcumBasligi: MdAlan[] = [];
  for (const p of parca) {
    const bas = p.split('\n')[0]!;
    if (bas.startsWith('#### Ölçüm başlığı')) olcumBasligi = tabloSatirlari(p.split('\n### ')[0]!);
    const m = /^#### `(yuk_[a-z0-9_]+)`/.exec(bas);
    if (!m) continue;
    const govde = p.split('\n### ')[0]!;
    let alanlar = tabloSatirlari(govde);
    if (govde.includes('Ölçüm başlığının 12 alanı')) alanlar = [...olcumBasligi, ...alanlar];
    sonuc.set(m[1]!, alanlar);
  }
  assert.equal(olcumBasligi.length, 12, 'ölçüm başlığı 12 alan olmalı');
  return sonuc;
}

const TAMSAYI = /^(u8|u16|u32|u64|i32|i64)$/;

/** Aralık sütunundan geçerli bir değer seçer; yoksa ayırt edici bir değer. */
function deger(tip: string, aralik: string, no: number): number {
  const kodlar = /^(\d+)=/.exec(aralik);
  if (kodlar) return Number(kodlar[1]);
  const ar = /^\[(-?[\d.e+]+), (-?[\d.e+]+)\]/.exec(aralik);
  if (ar) return Number(ar[1]);
  if (TAMSAYI.test(tip)) return 3 + ((no * 7) % 200);
  return 0.5 + (no % 50);
}

function kur(alanlar: MdAlan[], b: Bayt, sayac: { no: number }): Record<string, unknown> {
  const nesne: Record<string, unknown> = {};
  for (const a of alanlar) {
    const no = sayac.no++;
    const skaler = (tip: string): number | bigint => {
      const v = deger(tip, a.aralik, no);
      switch (tip) {
        case 'u8':
          b.u8(v);
          break;
        case 'u16':
          b.u16(v);
          break;
        case 'u32':
          b.u32(v);
          break;
        case 'i32':
          b.i32(v);
          break;
        case 'u64':
          b.u64(BigInt(v));
          return BigInt(v); // 64 bit alanlar kayıpsız bigint çözülür
        case 'i64':
          b.i64(BigInt(v));
          return BigInt(v);
        case 'f32':
          b.f32(v);
          return Math.fround(v);
        case 'f64':
          b.f64(v);
          break;
        default:
          assert.fail(`tanınmayan tip ${tip} (${a.ad})`);
      }
      return v;
    };
    let m: RegExpExecArray | null;
    if (TAMSAYI.test(a.tip) || a.tip === 'f32' || a.tip === 'f64') nesne[a.ad] = skaler(a.tip);
    else if ((m = /^(u8|u16|u32|u64|i32|i64|f32|f64)\[(\d+)\]$/.exec(a.tip))) {
      nesne[a.ad] = Array.from({ length: Number(m[2]) }, () => skaler(m![1]!));
    } else if ((m = /^metin\((u16|u32), en çok (\d+)\)$/.exec(a.tip))) {
      const s = `ğ${a.ad}`.slice(0, Math.min(Number(m[2]), 20) - 1);
      const u = new TextEncoder().encode(s);
      if (m[1] === 'u16') b.u16(u.length);
      else b.u32(u.length);
      b.ham(u);
      nesne[a.ad] = s;
    } else if ((m = /^bayt_dizisi\((u16|u32), en çok (\d+)\)$/.exec(a.tip))) {
      const u = [no % 256, 1, 2, 255];
      if (m[1] === 'u16') b.u16(u.length);
      else b.u32(u.length);
      b.ham(u);
      nesne[a.ad] = u;
    } else if ((m = /^dizi\((u8|u16), en çok (\d+)\)$/.exec(a.tip))) {
      const n = Math.min(2, Number(m[2]));
      if (m[1] === 'u8') b.u8(n);
      else b.u16(n);
      const ogeler: Record<string, unknown>[] = [];
      for (let i = 0; i < n; i++) ogeler.push(kur(a.alt, b, sayac));
      nesne[a.ad] = ogeler;
    } else assert.fail(`tanınmayan tip ${a.tip} (${a.ad})`);
  }
  return nesne;
}

function duz(v: unknown): unknown {
  if (v instanceof Uint8Array) return Array.from(v);
  if (Array.isArray(v)) return v.map(duz);
  if (v !== null && typeof v === 'object') return Object.fromEntries(Object.entries(v).map(([k, x]) => [k, duz(x)]));
  return v;
}

function anahtarlar(v: unknown, yol = ''): string[] {
  if (Array.isArray(v)) return v.flatMap((x, i) => anahtarlar(x, `${yol}[${i}]`));
  if (v !== null && typeof v === 'object') return Object.entries(v).flatMap(([k, x]) => [`${yol}.${k}`, ...anahtarlar(x, `${yol}.${k}`)]);
  return [];
}

const md = bolumler();

test('sözleşme metninde kataloğun bütün yük türleri var', () => {
  assert.deepEqual([...md.keys()].sort(), Object.keys(YUKLER).sort());
});

for (const [yukAdi, alanlar] of md) {
  test(`${yukAdi}: sözleşme metnindeki düzenle kurulan bayt dizisi doğru çözülür`, () => {
    const kanal = KANALLAR.find((k) => k.yuk === yukAdi);
    assert.ok(kanal, `${yukAdi} için kanal yok`);
    const b = new Bayt();
    const beklenen = kur(alanlar, b, { no: 0 });
    const c = new Cozucu();
    const out = c.ileti(cerceve(1_000_000n, 1, kanal.kanal_id, b.bitir()));
    assert.equal(c.sayaclar.istemci_bozuk_cerceve, 0, 'bozuk sayıldı: düzen uyuşmuyor');
    assert.equal(out.length, 1);
    const cozulen = duz(out[0]!.yuk);
    // alan adları ve SIRASI aynı olmalı
    assert.deepEqual(anahtarlar(cozulen), anahtarlar(beklenen));
    assert.deepEqual(cozulen, beklenen);
    assert.deepEqual(out[0]!.gecersiz, [], 'sözleşmenin aralığından seçilen değer geçersiz sayıldı');
  });
}
