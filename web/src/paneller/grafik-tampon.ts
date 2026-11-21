// Canlı grafik tamponu (DOM'suz; test edilebilir).
//
// Budama veri alımında yapılır, çizimden bağımsızdır: çizim dondurulsa ya da
// sekme gizli olsa da tampon en çok AZAMI_SANIYE ve AZAMI_ORNEK ile sınırlıdır.

export const AZAMI_SANIYE = 300;
/** Ölçü başına örnek sınırı (bellek üst sınırı; 300 s × ~100 satır/s ile uyumlu, tahmin). */
export const AZAMI_ORNEK = 40_000;
/** Budamayı toplu yapmak için pay: her eklemede dizi kaydırmamak için. */
const PAY = 1024;

export class Tampon {
  x: number[] = [];
  y: (number | null)[][];
  constructor(
    n: number,
    private readonly azamiSaniye = AZAMI_SANIYE,
    private readonly azamiOrnek = AZAMI_ORNEK,
  ) {
    this.y = Array.from({ length: n }, () => []);
  }
  ekle(t: number, sutun: number, v: readonly (number | null)[]): void {
    // Çoğu zaman sona eklenir; geç gelen örnek araya sokulur.
    let i = this.x.length;
    while (i > 0 && this.x[i - 1]! > t) i--;
    const sonaMi = i === this.x.length;
    if (sonaMi) {
      this.x.push(t);
      this.y.forEach((s, j) => s.push(j >= sutun && j < sutun + v.length ? (v[j - sutun] ?? null) : null));
    } else {
      this.x.splice(i, 0, t);
      this.y.forEach((s, j) => s.splice(i, 0, j >= sutun && j < sutun + v.length ? (v[j - sutun] ?? null) : null));
    }
    this.buda();
  }
  /** Saklama süresini ve örnek sınırını uygular (toplu: en çok PAY kadar aşılır). */
  private buda(): void {
    const son = this.x[this.x.length - 1];
    if (son === undefined) return;
    const enKucuk = son - this.azamiSaniye;
    let n = 0;
    if (this.x.length > this.azamiOrnek + PAY) n = this.x.length - this.azamiOrnek;
    else if ((this.x[PAY] ?? Infinity) < enKucuk) {
      while (n < this.x.length && this.x[n]! < enKucuk) n++;
    }
    if (n > 0) {
      this.x.splice(0, n);
      this.y.forEach((s) => s.splice(0, n));
    }
  }
  /** Geriye uyumluluk: çizim tarafı da çağırabilir; budama zaten alımda yapılır. */
  kirp(enKucuk: number): void {
    let n = 0;
    while (n < this.x.length && this.x[n]! < enKucuk) n++;
    if (n > 0) {
      this.x.splice(0, n);
      this.y.forEach((s) => s.splice(0, n));
    }
  }
  temizle(): void {
    this.x = [];
    this.y = this.y.map(() => []);
  }
}
