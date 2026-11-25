// Senaryo panelinin saf yardımcıları: çözümlenmiş senaryonun davranış özeti ve A5 bulgusu
// gösterimi. DOM'a dokunmaz (testler Node'da koşar).

import type { A5Bulgusu } from '../a3/iletiler';

export interface OzetSatiri {
  etiket: string;
  deger: string;
}

function nesne(v: unknown): Record<string, unknown> | null {
  return v !== null && typeof v === 'object' && !Array.isArray(v) ? (v as Record<string, unknown>) : null;
}

function metin(v: unknown): string | null {
  return typeof v === 'string' || typeof v === 'number' || typeof v === 'boolean' ? String(v) : null;
}

/**
 * Çözümlenmiş senaryodan (A5 kanonik JSON) insanın okuyacağı davranış özeti. Yalnız
 * sunucunun gönderdiği alanlar gösterilir; olmayan alan "—" olur, uydurulmaz.
 */
export function davranisOzeti(senaryo: Record<string, unknown>): OzetSatiri[] {
  const meta = nesne(senaryo.meta);
  const ortam = nesne(senaryo.ortam);
  const hedefler = nesne(senaryo.hedefler);
  const gorev = nesne(senaryo.gorev);
  const ogeler = Array.isArray(gorev?.ogeler) ? (gorev!.ogeler as unknown[]) : [];
  const turler = new Map<string, number>();
  for (const o of ogeler) {
    const t = metin(nesne(o)?.item_type) ?? '?';
    turler.set(t, (turler.get(t) ?? 0) + 1);
  }
  const yok = '—';
  return [
    { etiket: 'Ad', deger: metin(meta?.ad) ?? yok },
    { etiket: 'Açıklama', deger: metin(meta?.aciklama) ?? yok },
    { etiket: 'Su derinliği', deger: metin(ortam?.derinlik) === null ? yok : `${metin(ortam?.derinlik)} m` },
    { etiket: 'Hedef sayısı', deger: hedefler === null ? yok : String(Object.keys(hedefler).length) },
    { etiket: 'Görev ögeleri', deger: ogeler.length === 0 ? yok : `${ogeler.length} (${[...turler].map(([t, n]) => `${t} ×${n}`).join(', ')})` },
    { etiket: 'Üst düzey bölümler', deger: Object.keys(senaryo).join(', ') || yok },
  ];
}

/** Bir A5 bulgusunun "yol · satır N" konumu. */
export function bulguKonumu(b: A5Bulgusu): string {
  const parcalar: string[] = [];
  if (b.dosya !== undefined && b.dosya !== '') parcalar.push(b.dosya);
  if (b.yol !== undefined && b.yol !== '') parcalar.push(b.yol);
  const yol = parcalar.join(' · ');
  return b.satir !== undefined && b.satir !== null ? `${yol}${yol === '' ? '' : ' · '}satır ${b.satir}` : yol === '' ? '—' : yol;
}

/** Bulguları satır numarasına göre haritalar: satır vurgusu için (en ağır önem kazanır). */
export function satirOnemleri(bulgular: readonly A5Bulgusu[]): Map<number, 'hata' | 'uyari'> {
  const m = new Map<number, 'hata' | 'uyari'>();
  for (const b of bulgular) {
    if (b.satir === undefined || b.satir === null) continue;
    if (m.get(b.satir) !== 'hata') m.set(b.satir, b.onem);
  }
  return m;
}

export function sayiMetni(v: unknown): string {
  if (v === null || v === undefined) return '—';
  if (typeof v === 'number') return Number.isInteger(v) ? String(v) : String(Number(v.toPrecision(6)));
  return String(v);
}
