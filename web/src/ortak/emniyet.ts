// Emniyet panelinin gösterim mantığı (DOM'suz; test edilebilir).
//
// Kurallar (A3-K24, K89, K97):
// - Yalnız açıkça 0 ya da 1 olan ve çözücünün geçersiz işaretlemediği değer durum
//   etiketine çevrilir; başka her değer "geçersiz" gösterilir.
// - Kartın değeri bilinmiyorsa (yok değeri 255 → null) "bilinmiyor" gösterilir;
//   A3 §3.4 gereği o girdinin fark biti 0'dır, bu yüzden "eşit" SAYILMAZ.
// - Alan hiç yoksa (eksik) "yok" gösterilir.
// - Özet önceliği: geçersiz > fark > bilinmiyor > eşit. "Eşit" ancak beş girdinin
//   hem yetkili hem kart değeri bilinen ve geçerliyse ve hiçbirinde fark yoksa.

import type { YukAtisDurumTuplerOge, YukEmniyetGirdiler } from '../a3/katalog';

export type GirdiTuru = 'acil' | 'fis' | 'pim';
export type DegerSinifi = 'kisitlayan' | 'gevsek' | 'yok' | 'gecersiz' | 'bilinmiyor';

export interface HucreGosterimi {
  metin: string;
  sinif: DegerSinifi;
}

export type FarkDurumu = 'fark' | 'esit' | 'bilinmiyor' | 'gecersiz';

export interface GirdiSatiri {
  ad: string;
  yetkili: HucreGosterimi;
  kart: HucreGosterimi;
  fark: FarkDurumu;
}

export interface OzetGosterimi {
  durum: FarkDurumu;
  metin: string;
  sinif: 'iyi' | 'kotu' | 'uyari' | 'notr';
}

const SATIRLAR: readonly { ad: string; tur: GirdiTuru; bit: number; alan: string; dizin: number | null }[] = [
  { ad: 'Acil durdurma', tur: 'acil', bit: 0, alan: 'acil_durdurma', dizin: null },
  { ad: 'Silahlandırma fişi · f1', tur: 'fis', bit: 1, alan: 'silahlandirma_fisi', dizin: 0 },
  { ad: 'Silahlandırma fişi · f2', tur: 'fis', bit: 2, alan: 'silahlandirma_fisi', dizin: 1 },
  { ad: 'Emniyet pimi · f1', tur: 'pim', bit: 3, alan: 'emniyet_pimi', dizin: 0 },
  { ad: 'Emniyet pimi · f2', tur: 'pim', bit: 4, alan: 'emniyet_pimi', dizin: 1 },
];

type Okunan = { tur: 'deger'; v: 0 | 1 } | { tur: 'bilinmiyor' } | { tur: 'yok' } | { tur: 'gecersiz' };

function oku(y: Record<string, unknown>, alan: string, dizin: number | null, gecersiz: ReadonlySet<string>): Okunan {
  const yol = dizin === null ? alan : `${alan}[${dizin}]`;
  const ham = y[alan];
  if (ham === undefined || (dizin !== null && (ham === null || !Array.isArray(ham)))) return { tur: 'yok' };
  const v = dizin === null ? ham : (ham as unknown[])[dizin];
  if (v === undefined) return { tur: 'yok' };
  if (gecersiz.has(yol) || gecersiz.has(alan)) return { tur: 'gecersiz' };
  if (v === null) return dizin === null && ham === null && !alan.startsWith('kart_') ? { tur: 'yok' } : { tur: 'bilinmiyor' };
  if (v === 0 || v === 1) return { tur: 'deger', v };
  return { tur: 'gecersiz' };
}

function etiket(tur: GirdiTuru, o: Okunan): HucreGosterimi {
  switch (o.tur) {
    case 'yok':
      return { metin: 'yok', sinif: 'yok' };
    case 'bilinmiyor':
      return { metin: 'bilinmiyor', sinif: 'bilinmiyor' };
    case 'gecersiz':
      return { metin: 'geçersiz', sinif: 'gecersiz' };
    default: {
      const v = o.v;
      if (tur === 'acil') return { metin: v === 1 ? 'BASILI' : 'serbest', sinif: v === 1 ? 'kisitlayan' : 'gevsek' };
      const metin = v === 1 ? 'takılı' : 'çıkarılmış';
      // A3-K89: fişin çıkarılması ve pimin takılması emniyeti artıran (kısıtlayan) yöndür.
      const kisit = tur === 'fis' ? v === 0 : v === 1;
      return { metin, sinif: kisit ? 'kisitlayan' : 'gevsek' };
    }
  }
}

export function girdiGosterimi(y: YukEmniyetGirdiler, gecersiz: readonly string[] = []): { satirlar: GirdiSatiri[]; ozet: OzetGosterimi } {
  const g = new Set(gecersiz);
  const kayit = y as unknown as Record<string, unknown>;
  const farkBitleri = typeof kayit['geri_okuma_farki'] === 'number' && !g.has('geri_okuma_farki') ? (kayit['geri_okuma_farki'] as number) : null;
  const satirlar = SATIRLAR.map((s): GirdiSatiri => {
    const yo = oku(kayit, s.alan, s.dizin, g);
    const ko = oku(kayit, 'kart_' + s.alan, s.dizin, g);
    let fark: FarkDurumu;
    if (yo.tur === 'gecersiz' || ko.tur === 'gecersiz') fark = 'gecersiz';
    else if (yo.tur !== 'deger' || ko.tur !== 'deger' || farkBitleri === null) fark = 'bilinmiyor';
    else if (((farkBitleri >> s.bit) & 1) === 1 || yo.v !== ko.v) fark = 'fark';
    else fark = 'esit';
    return { ad: s.ad, yetkili: etiket(s.tur, yo), kart: etiket(s.tur, ko), fark };
  });
  const say = (d: FarkDurumu): number => satirlar.filter((s) => s.fark === d).length;
  let ozet: OzetGosterimi;
  if (say('gecersiz') > 0) ozet = { durum: 'gecersiz', metin: `${say('gecersiz')} geçersiz değer`, sinif: 'kotu' };
  else if (say('fark') > 0) ozet = { durum: 'fark', metin: `${say('fark')} fark`, sinif: 'kotu' };
  else if (say('bilinmiyor') > 0) ozet = { durum: 'bilinmiyor', metin: `${say('bilinmiyor')} geri okuma bilinmiyor`, sinif: 'uyari' };
  else ozet = { durum: 'esit', metin: 'Geri okuma eşit', sinif: 'iyi' };
  return { satirlar, ozet };
}

export interface TupGosterimi {
  dolu: HucreGosterimi & { cip: 'vurgu' | 'notr' | 'kotu' };
  izin: string;
}

/** `gecersizMi(alan)`: çözücünün bu tüp öğesindeki alanı geçersiz işaretleyip işaretlemediği. */
export function tupGosterimi(t: YukAtisDurumTuplerOge, gecersizMi: (alan: string) => boolean = () => false): TupGosterimi {
  const ikili = (alan: 'dolu' | 'sureli_izin_gecerli'): 0 | 1 | null => {
    const v = (t as unknown as Record<string, unknown>)[alan];
    return !gecersizMi(alan) && (v === 0 || v === 1) ? v : null;
  };
  const d = ikili('dolu');
  const i = ikili('sureli_izin_gecerli');
  return {
    dolu: d === null ? { metin: 'GEÇERSİZ', sinif: 'gecersiz', cip: 'kotu' } : d === 1 ? { metin: 'DOLU', sinif: 'kisitlayan', cip: 'vurgu' } : { metin: 'BOŞ', sinif: 'kisitlayan', cip: 'notr' },
    izin: i === null ? 'geçersiz' : i === 1 ? 'geçerli' : 'yok',
  };
}
