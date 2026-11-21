// A3 ikili çerçeve kodlayıcısı. Paneller kullanmaz; sahte A3 sunucusu ve
// testler içindir. Düzen çözücüyle aynı katalog tanımından gelir (A3 §3.1–3.4).

import { BASLIK_BAYT, YUKLER, type AlanTanimi, type KanalTanimi, type KodlamaGirdisi, type YukAdi, type YukTipleri } from './katalog';
import { YOK_TAMSAYI } from './cerceve';

const UTF8 = new TextEncoder();

/** Büyüyen bayt yazıcısı; bütün çok baytlı değerler küçük uçlu. */
export class Yazici {
  private tampon = new Uint8Array(256);
  private dv = new DataView(this.tampon.buffer);
  konum = 0;

  private yer(n: number): number {
    if (this.konum + n > this.tampon.length) {
      let yeni = this.tampon.length * 2;
      while (yeni < this.konum + n) yeni *= 2;
      const t = new Uint8Array(yeni);
      t.set(this.tampon);
      this.tampon = t;
      this.dv = new DataView(t.buffer);
    }
    const k = this.konum;
    this.konum += n;
    return k;
  }
  // Her yazımda önce `yer` çağrılır: tampon büyürse `dv` yenilenir; `this.dv.set…(this.yer(n))`
  // biçimi eski görünüme yazardı (argümandan önce nesne değerlendirilir).
  u8(v: number): this {
    const k = this.yer(1);
    this.dv.setUint8(k, v);
    return this;
  }
  u16(v: number): this {
    const k = this.yer(2);
    this.dv.setUint16(k, v, true);
    return this;
  }
  u32(v: number): this {
    const k = this.yer(4);
    this.dv.setUint32(k, v, true);
    return this;
  }
  i32(v: number): this {
    const k = this.yer(4);
    this.dv.setInt32(k, v, true);
    return this;
  }
  u64(v: bigint | number): this {
    const b = kayipsiz(v);
    if (b < 0n || b > (1n << 64n) - 1n) throw new RangeError(`u64 aralık dışı: ${b}`);
    const k = this.yer(8);
    this.dv.setBigUint64(k, b, true);
    return this;
  }
  i64(v: bigint | number): this {
    const b = kayipsiz(v);
    if (b < -(1n << 63n) || b > (1n << 63n) - 1n) throw new RangeError(`i64 aralık dışı: ${b}`);
    const k = this.yer(8);
    this.dv.setBigInt64(k, b, true);
    return this;
  }
  f32(v: number): this {
    const k = this.yer(4);
    this.dv.setFloat32(k, v, true);
    return this;
  }
  f64(v: number): this {
    const k = this.yer(8);
    this.dv.setFloat64(k, v, true);
    return this;
  }
  baytlar(b: Uint8Array): this {
    const k = this.yer(b.length); // önce yer aç: tampon büyüyebilir
    this.tampon.set(b, k);
    return this;
  }
  /** `metin(u16, m)`: u16 uzunluk + UTF-8. */
  metin(s: string, sayac: 'u8' | 'u16' | 'u32' = 'u16'): this {
    const b = UTF8.encode(s);
    this.sayac(sayac, b.length);
    return this.baytlar(b);
  }
  sayac(tip: 'u8' | 'u16' | 'u32', n: number): this {
    if (tip === 'u8') return this.u8(n);
    if (tip === 'u16') return this.u16(n);
    return this.u32(n);
  }
  bitir(): Uint8Array {
    return this.tampon.slice(0, this.konum);
  }
}

/** 64 bit değer: bigint olduğu gibi; number yalnız güvenli tamsayıysa (sessiz yuvarlama yok). */
function kayipsiz(v: bigint | number): bigint {
  if (typeof v === 'bigint') return v;
  if (!Number.isSafeInteger(v)) throw new RangeError(`64 bit alana güvenli olmayan sayı: ${v}; bigint verin`);
  return BigInt(v);
}

function sayiyaCevir(v: number | bigint): number {
  return typeof v === 'bigint' ? Number(v) : v;
}

function skalerYaz(y: Yazici, alan: AlanTanimi, v: number | bigint | null): void {
  if (v === null) {
    if (alan.yok === undefined) throw new Error(`${alan.ad}: "yok" değeri tanımsız alana null yazılamaz`);
    if (alan.yok === 'NaN') {
      if (alan.tip === 'f32') y.f32(Number.NaN);
      else y.f64(Number.NaN);
      return;
    }
    const ham = YOK_TAMSAYI[alan.yok];
    switch (alan.tip) {
      case 'u8':
        y.u8(Number(ham));
        return;
      case 'u16':
        y.u16(Number(ham));
        return;
      case 'u32':
        y.u32(Number(ham));
        return;
      case 'i32':
        y.i32(Number(ham));
        return;
      case 'u64':
        y.u64(ham);
        return;
      case 'i64':
        y.i64(ham);
        return;
      default:
        throw new Error(alan.tip);
    }
  }
  switch (alan.tip) {
    case 'u8':
      y.u8(sayiyaCevir(v));
      return;
    case 'u16':
      y.u16(sayiyaCevir(v));
      return;
    case 'u32':
      y.u32(sayiyaCevir(v));
      return;
    case 'i32':
      y.i32(sayiyaCevir(v));
      return;
    case 'u64':
      y.u64(v);
      return;
    case 'i64':
      y.i64(v);
      return;
    case 'f32':
      y.f32(sayiyaCevir(v));
      return;
    case 'f64':
      y.f64(sayiyaCevir(v));
      return;
    default:
      throw new Error(alan.tip);
  }
}

function alanYaz(y: Yazici, alan: AlanTanimi, deger: unknown): void {
  switch (alan.tip) {
    case 'metin':
      y.metin(String(deger), alan.sayac ?? 'u16');
      return;
    case 'bayt_dizisi': {
      const b = deger as Uint8Array;
      y.sayac(alan.sayac ?? 'u16', b.length).baytlar(b);
      return;
    }
    case 'dizi': {
      const ogeler = deger as Record<string, unknown>[];
      y.sayac(alan.sayac ?? 'u16', ogeler.length);
      for (const oge of ogeler) for (const alt of alan.oge ?? []) alanYaz(y, alt, oge[alt.ad]);
      return;
    }
    default:
      if (alan.adet !== undefined) {
        const dizi = deger as (number | bigint | null)[];
        if (dizi.length !== alan.adet) throw new Error(`${alan.ad}: ${alan.adet} öğe beklenirdi`);
        for (const v of dizi) skalerYaz(y, alan, v);
        return;
      }
      skalerYaz(y, alan, deger as number | bigint | null);
  }
}

/** Yükü katalog düzeniyle kodlar. 64 bit alanlar bigint ya da güvenli tamsayı olabilir. */
export function yukKodla<Y extends YukAdi>(yukAdi: Y, deger: KodlamaGirdisi<YukTipleri[Y]>): Uint8Array {
  const y = new Yazici();
  const nesne = deger as unknown as Record<string, unknown>;
  for (const alan of YUKLER[yukAdi] as readonly AlanTanimi[]) alanYaz(y, alan, nesne[alan.ad]);
  return y.bitir();
}

export interface BaslikGirdisi {
  t_ns: number | bigint;
  sira: number;
  bayraklar?: number;
}

/** Başlık + yük = bir çerçeve (A3 §3.2). */
export function cerceveKodla(kanal: KanalTanimi, baslik: BaslikGirdisi, yuk: Uint8Array): Uint8Array {
  const y = new Yazici();
  y.i64(baslik.t_ns)
    .u32(baslik.sira >>> 0)
    .u32(yuk.length)
    .u16(kanal.kanal_id)
    .u16(kanal.kanal_surumu)
    .u16(baslik.bayraklar ?? 0)
    .u16(0)
    .baytlar(yuk);
  const c = y.bitir();
  if (c.length !== BASLIK_BAYT + yuk.length) throw new Error('başlık boyu');
  return c;
}

/** Çerçeveleri tek ikili iletide art arda birleştirir (A3-K20). */
export function iletiBirlestir(cerceveler: readonly Uint8Array[]): Uint8Array {
  let n = 0;
  for (const c of cerceveler) n += c.length;
  const out = new Uint8Array(n);
  let k = 0;
  for (const c of cerceveler) {
    out.set(c, k);
    k += c.length;
  }
  return out;
}
