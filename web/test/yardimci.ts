// Testlerin bağımsız bayt yazıcısı: kitaplığın kodlayıcısını kullanmaz.

export class Bayt {
  private b: number[] = [];
  private yaz(n: number, f: (dv: DataView) => void): this {
    const ab = new ArrayBuffer(n);
    f(new DataView(ab));
    this.b.push(...new Uint8Array(ab));
    return this;
  }
  u8(v: number): this {
    return this.yaz(1, (d) => d.setUint8(0, v));
  }
  u16(v: number): this {
    return this.yaz(2, (d) => d.setUint16(0, v, true));
  }
  u32(v: number): this {
    return this.yaz(4, (d) => d.setUint32(0, v, true));
  }
  i32(v: number): this {
    return this.yaz(4, (d) => d.setInt32(0, v, true));
  }
  u64(v: bigint): this {
    return this.yaz(8, (d) => d.setBigUint64(0, v, true));
  }
  i64(v: bigint): this {
    return this.yaz(8, (d) => d.setBigInt64(0, v, true));
  }
  f32(v: number): this {
    return this.yaz(4, (d) => d.setFloat32(0, v, true));
  }
  f64(v: number): this {
    return this.yaz(8, (d) => d.setFloat64(0, v, true));
  }
  ham(b: ArrayLike<number>): this {
    for (let i = 0; i < b.length; i++) this.b.push(b[i]!);
    return this;
  }
  metin16(s: string): this {
    const b = new TextEncoder().encode(s);
    return this.u16(b.length).ham(b);
  }
  get uzunluk(): number {
    return this.b.length;
  }
  bitir(): Uint8Array {
    return Uint8Array.from(this.b);
  }
}

/** A3 §3.2 başlığı, bayt konumları 0/8/12/16/18/20/22. */
export function baslik(t_ns: bigint, sira: number, yukUzunlugu: number, kanalId: number, surum = 1, bayrak = 0): Bayt {
  return new Bayt().i64(t_ns).u32(sira).u32(yukUzunlugu).u16(kanalId).u16(surum).u16(bayrak).u16(0);
}

export function cerceve(t_ns: bigint, sira: number, kanalId: number, yuk: Uint8Array, surum = 1, bayrak = 0): Uint8Array {
  return baslik(t_ns, sira, yuk.length, kanalId, surum, bayrak).ham(yuk).bitir();
}

export function birlestir(...p: Uint8Array[]): Uint8Array {
  const n = p.reduce((s, x) => s + x.length, 0);
  const o = new Uint8Array(n);
  let k = 0;
  for (const x of p) {
    o.set(x, k);
    k += x.length;
  }
  return o;
}

export const f32 = (v: number): number => Math.fround(v);
