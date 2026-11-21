// Sayı ve zaman biçimleri. Arayüz Türkçedir: ondalık ayırıcı virgül.

export const YOK = '—';

export function sayi(v: number | null | undefined, basamak = 2): string {
  if (v === null || v === undefined || !Number.isFinite(v)) return YOK;
  const s = v.toFixed(basamak);
  return (s === '-0' || /^-0[.,]0*$/.test(s) ? s.slice(1) : s).replace('.', ',');
}

export function isaretli(v: number | null | undefined, basamak = 2): string {
  if (v === null || v === undefined || !Number.isFinite(v)) return YOK;
  const s = sayi(v, basamak);
  return v > 0 && !/^0[,0]*$/.test(s) ? '+' + s : s;
}

export function tamsayi(v: number | bigint | null | undefined): string {
  if (v === null || v === undefined) return YOK;
  if (typeof v === 'bigint') return v.toLocaleString('tr-TR'); // kayıpsız
  if (!Number.isFinite(v)) return YOK;
  return Math.round(v).toLocaleString('tr-TR');
}

/**
 * ns → s. bigint'te önce tam milisaniyeye bölünür, sonra Number'a çevrilir
 * (ölçekleme dönüşümden önce; 2^53 ns ≈ 104 gün sınırına takılmaz).
 */
export function saniye(ns: bigint | number): number {
  if (typeof ns === 'number') return ns / 1e9;
  return Number(ns / 1_000_000n) / 1000 + Number(ns % 1_000_000n) / 1e9;
}

/** Sim zamanı: ss:dd:ss.mmm (saat yoksa dd:ss.mmm); bigint aritmetiğiyle kayıpsız. */
export function simZamani(t_ns: bigint | number | null | undefined): string {
  if (t_ns === null || t_ns === undefined) return YOK;
  if (typeof t_ns === 'number') {
    if (!Number.isFinite(t_ns)) return YOK;
    t_ns = BigInt(Math.trunc(t_ns));
  }
  const isaret = t_ns < 0n ? '−' : '';
  const ms = (t_ns < 0n ? -t_ns : t_ns) / 1_000_000n;
  const s = ms / 1000n;
  const d = s / 60n;
  const sa = d / 60n;
  const p = (x: bigint, n = 2): string => x.toString().padStart(n, '0');
  const govde = `${p(d % 60n)}:${p(s % 60n)}.${p(ms % 1000n, 3)}`;
  return isaret + (sa > 0n ? `${p(sa)}:${govde}` : govde);
}

/** Radyandan dereceye, [0, 360). */
export function yonDerece(rad: number | null | undefined): string {
  if (rad === null || rad === undefined || !Number.isFinite(rad)) return YOK;
  let d = (rad * 180) / Math.PI;
  d = ((d % 360) + 360) % 360;
  return sayi(d, 1);
}

export function derece(rad: number | null | undefined, basamak = 1): string {
  if (rad === null || rad === undefined || !Number.isFinite(rad)) return YOK;
  return isaretli((rad * 180) / Math.PI, basamak);
}

export function sure_ns(girdi: number | bigint | null | undefined): string {
  if (girdi === null || girdi === undefined) return YOK;
  const ns = typeof girdi === 'bigint' ? saniye(girdi) * 1e9 : girdi; // yalnız gösterim
  if (!Number.isFinite(ns)) return YOK;
  if (Math.abs(ns) < 1e3) return `${sayi(ns, 0)} ns`;
  if (Math.abs(ns) < 1e6) return `${sayi(ns / 1e3, 1)} µs`;
  if (Math.abs(ns) < 1e9) return `${sayi(ns / 1e6, 2)} ms`;
  return `${sayi(ns / 1e9, 2)} s`;
}

export function bayt(girdi: number | bigint | null | undefined): string {
  if (girdi === null || girdi === undefined) return YOK;
  const n = Number(girdi); // yalnız gösterim (KiB/MiB ölçeği)
  if (!Number.isFinite(n)) return YOK;
  if (n < 1024) return `${n} B`;
  if (n < 1024 ** 2) return `${sayi(n / 1024, 1)} KiB`;
  if (n < 1024 ** 3) return `${sayi(n / 1024 ** 2, 1)} MiB`;
  return `${sayi(n / 1024 ** 3, 2)} GiB`;
}

/** Kuaterniyondan (w,x,y,z; gövdeden NED'e) ZYX Euler açıları (rad). */
export function euler(q: readonly (number | null)[]): { yatis: number; yunuslama: number; yon: number } | null {
  const [w, x, y, z] = q;
  if (w == null || x == null || y == null || z == null) return null;
  const yatis = Math.atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y));
  const s = Math.max(-1, Math.min(1, 2 * (w * y - z * x)));
  const yunuslama = Math.asin(s);
  const yon = Math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z));
  return { yatis, yunuslama, yon };
}

/** Kuaterniyonla gövde vektörünü NED'e döndürür. */
export function dondur(q: readonly number[], v: readonly number[]): [number, number, number] {
  const [w = 1, x = 0, y = 0, z = 0] = q;
  const [a = 0, b = 0, c = 0] = v;
  // v' = v + 2w (u × v) + 2 u × (u × v), u = (x, y, z)
  const tx = 2 * (y * c - z * b);
  const ty = 2 * (z * a - x * c);
  const tz = 2 * (x * b - y * a);
  return [a + w * tx + (y * tz - z * ty), b + w * ty + (z * tx - x * tz), c + w * tz + (x * ty - y * tx)];
}

/**
 * Aracın NED hızı (m/s): gövde çerçevesindeki doğrusal hız (nu[0..2] = u, v, w)
 * tam yönelimle (q_nb, gövdeden NED'e) döndürülür (A3 §3.4, iskelet §3.1).
 */
export function aracNedHizi(q: readonly number[], nu: readonly number[]): [number, number, number] {
  return dondur(q, [nu[0] ?? 0, nu[1] ?? 0, nu[2] ?? 0]);
}

const DURUM_ADLARI: Record<string, string> = {
  bos: 'Boş',
  yuklendi: 'Yüklendi',
  hazirlaniyor: 'Hazırlanıyor',
  kosuyor: 'Koşuyor',
  duraklatildi: 'Duraklatıldı',
  bitti: 'Bitti',
  durduruldu: 'Durduruldu',
  hata: 'Hata',
  izleme: 'İzleme',
};

export function durumAdi(ad: string): string {
  return DURUM_ADLARI[ad] ?? ad;
}

const KIP_ADLARI: Record<string, string> = {
  yok: '—',
  sil: 'SIL',
  hil_1x: 'HIL 1×',
  hil_bariyer: 'HIL bariyer',
  tekrar_fizik: 'Tekrar · fizik',
  tekrar_girdi_sil: 'Tekrar · girdi',
  tekrar_yeniden_kurma: 'Tekrar · yeniden kurma',
};

export function kipAdi(ad: string): string {
  return KIP_ADLARI[ad] ?? ad;
}

/** A4 IDA_MODE_* (port.h) değer adları. */
export const OTOPILOT_KIPLERI: Readonly<Record<number, string>> = {
  0: 'INIT',
  1: 'DISARMED',
  2: 'MANUAL',
  3: 'STABILIZE',
  4: 'DEPTH_HOLD',
  5: 'STATION_KEEP',
  6: 'GUIDED',
  7: 'AUTO',
};

/** Türkçe büyük harf (i → İ). */
export function buyuk(s: string): string {
  return s.toLocaleUpperCase('tr-TR');
}
