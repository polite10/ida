// Sonar panelinin hesapları (DOM'suz; test edilebilir).

export interface Ping360Ayari {
  n_ornek: number;
  ornek_periyodu: number;
  ses_hizi_mps: number;
}

/** Ping360 menzil hesabına giren alanlar; biri geçersizse satır menzile alınmaz. */
export const MENZIL_ALANLARI = ['n_ornek', 'ornek_periyodu', 'ses_hizi_mps', 'ornekler'] as const;

/**
 * A3 §3.4: son kutunun nominal menzili r = c · (2(n − 1) + 1) · periyot · 25 ns / 4 (m).
 * Çözücünün geçersiz işaretlediği, sonlu olmayan ya da pozitif olmayan girdide null:
 * böyle bir satır menzile ve çizime alınmaz (A3-K24).
 */
export function ping360Menzil(y: Ping360Ayari, gecersiz: readonly string[] = []): number | null {
  if (MENZIL_ALANLARI.some((a) => gecersiz.includes(a))) return null;
  const { n_ornek: n, ornek_periyodu: p, ses_hizi_mps: c } = y;
  if (!Number.isInteger(n) || n < 1 || !Number.isInteger(p) || p <= 0) return null;
  if (!Number.isFinite(c) || c <= 0) return null;
  const r = (c * (2 * (n - 1) + 1) * p * 25e-9) / 4;
  return Number.isFinite(r) && r > 0 ? r : null;
}

/** Halka sayısının üst sınırı: veriden bağımsız, çizim işi sınırlı. */
export const AZAMI_HALKA = 12;

/** Menzil halkalarının yarıçapları (m): 1-2-5 adımlı, en çok AZAMI_HALKA. */
export function menzilHalkalari(menzilM: number): number[] {
  if (!Number.isFinite(menzilM) || menzilM <= 0) return [];
  const ham = menzilM / 6;
  const us = Math.pow(10, Math.floor(Math.log10(ham)));
  const k = ham / us;
  const adim = (k < 1.5 ? 1 : k < 3.5 ? 2 : k < 7.5 ? 5 : 10) * us;
  const out: number[] = [];
  for (let i = 1; i <= AZAMI_HALKA && i * adim <= menzilM * (1 + 1e-9); i++) out.push(i * adim);
  return out;
}
