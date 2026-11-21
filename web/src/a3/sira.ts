// 32 bit sıra numaralarının seri aritmetikle karşılaştırılması (iskelet §3.1):
// `a`, `b`'den yenidir ancak ve ancak (a − b) mod 2^32 ∈ [1, 2^31 − 1]; fark 0 ise
// yinelemedir; 2^31 ya da büyükse `a` eskidir.

const YARIM = 0x80000000;

/** (a − b) mod 2^32. */
export function siraFarki(a: number, b: number): number {
  return (a - b) >>> 0;
}

export function siraYeni(a: number, b: number): boolean {
  const d = siraFarki(a, b);
  return d >= 1 && d < YARIM;
}

export function siraEski(a: number, b: number): boolean {
  return siraFarki(a, b) >= YARIM;
}

/** `taban`'dan sonraki sıraya göre (seri) sıralama karşılaştırıcısı. */
export function siraSiralayici(taban: number): (a: number, b: number) => number {
  return (a, b) => siraFarki(a, taban) - siraFarki(b, taban);
}

/** b'den n önceki sıra; sarımı hesaba katar. */
export function siraGeri(b: number, n: number): number {
  return (b - n) >>> 0;
}
