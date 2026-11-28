// Yer istasyonu komutlarının parametre şemasından (JSON Schema alt kümesi) form alanları üretir
// ve metin girdilerini şemaya göre değere çevirir (DOM'suz; test edilebilir). Sunucu doğrulaması
// asıldır (A3-K100): buradaki denetim yalnız bariz hataları göndermeden yakalar.
//
// Desteklenen: nesne alanları olarak tamsayı, sayı, metin, mantıksal, enum ve bu türlerin
// dizileri (virgülle ayrılmış). Nesne dizisi gibi karmaşık şemalar "desteklenmiyor" diye
// bildirilir; komut listede görünür ama forma dökülmez (listeyi istemci değiştirmez, W5-K54).

export type AlanTuru = 'tamsayi' | 'sayi' | 'metin' | 'mantik' | 'secim' | 'liste';

export interface FormAlani {
  ad: string;
  zorunlu: boolean;
  tur: AlanTuru;
  aciklama: string;
  /** `secim` için seçenekler; `liste` için öğe seçenekleri (varsa). */
  secenekler: readonly string[] | null;
  min: number | null;
  max: number | null;
  /** `liste` için öğe türü ve uzunluk sınırları. */
  ogeTuru: 'tamsayi' | 'sayi' | 'metin' | null;
  minOge: number | null;
  maxOge: number | null;
}

type Sema = Record<string, unknown>;
const nesne = (v: unknown): Sema | null => (v !== null && typeof v === 'object' && !Array.isArray(v) ? (v as Sema) : null);
const sayiMi = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);

function skalerTur(s: Sema): 'tamsayi' | 'sayi' | 'metin' | 'mantik' | 'secim' | null {
  if (Array.isArray(s.enum) && s.enum.every((e) => typeof e === 'string')) return 'secim';
  if (s.type === 'integer') return 'tamsayi';
  if (s.type === 'number') return 'sayi';
  if (s.type === 'string') return 'metin';
  if (s.type === 'boolean') return 'mantik';
  return null;
}

export function formAlanlari(sema: unknown): { alanlar: FormAlani[] } | { desteklenmiyor: string } {
  const s = nesne(sema);
  if (s === null || s.type !== 'object') return { desteklenmiyor: 'parametre şeması bir nesne değil' };
  const ozellikler = nesne(s.properties) ?? {};
  const zorunlu = new Set(Array.isArray(s.required) ? (s.required as unknown[]).filter((x): x is string => typeof x === 'string') : []);
  const alanlar: FormAlani[] = [];
  for (const [ad, ham] of Object.entries(ozellikler)) {
    const a = nesne(ham);
    if (a === null) return { desteklenmiyor: `${ad}: şema okunamadı` };
    const aciklama = typeof a.description === 'string' ? a.description : '';
    const min = sayiMi(a.minimum) ? a.minimum : null;
    const max = sayiMi(a.maximum) ? a.maximum : null;
    const t = skalerTur(a);
    if (t !== null) {
      alanlar.push({ ad, zorunlu: zorunlu.has(ad), tur: t, aciklama, secenekler: t === 'secim' ? (a.enum as string[]) : null, min, max, ogeTuru: null, minOge: null, maxOge: null });
      continue;
    }
    if (a.type === 'array') {
      const oge = nesne(a.items);
      const ot = oge === null ? null : skalerTur(oge);
      if (oge === null || ot === null || ot === 'mantik') return { desteklenmiyor: `${ad}: dizi öğeleri basit türde değil (bu formda desteklenmiyor)` };
      alanlar.push({
        ad,
        zorunlu: zorunlu.has(ad),
        tur: 'liste',
        aciklama,
        secenekler: ot === 'secim' ? (oge.enum as string[]) : null,
        min: sayiMi(oge.minimum) ? oge.minimum : null,
        max: sayiMi(oge.maximum) ? oge.maximum : null,
        ogeTuru: ot === 'secim' ? 'metin' : ot,
        minOge: sayiMi(a.minItems) ? a.minItems : null,
        maxOge: sayiMi(a.maxItems) ? a.maxItems : null,
      });
      continue;
    }
    return { desteklenmiyor: `${ad}: ${String(a.type ?? 'bilinmeyen')} türü bu formda desteklenmiyor` };
  }
  for (const z of zorunlu) if (!alanlar.some((a) => a.ad === z)) return { desteklenmiyor: `zorunlu alan ${z} şemada tanımlı değil` };
  return { alanlar };
}

type Okunan = { deger: unknown } | { atla: true } | { hata: string };

function skaler(tur: 'tamsayi' | 'sayi' | 'metin', metin: string, min: number | null, max: number | null, secenekler: readonly string[] | null): Okunan {
  if (tur === 'metin') {
    if (secenekler !== null && !secenekler.includes(metin)) return { hata: `şunlardan biri olmalı: ${secenekler.join(', ')}` };
    return { deger: metin };
  }
  const t = metin.trim().replace(',', '.');
  if (tur === 'tamsayi' ? !/^-?\d+$/.test(t) : !/^-?\d+(\.\d+)?([eE][-+]?\d+)?$/.test(t)) return { hata: tur === 'tamsayi' ? 'tamsayı olmalı' : 'sayı olmalı' };
  const n = Number(t);
  if (!Number.isFinite(n)) return { hata: 'sonlu bir sayı olmalı' };
  if (min !== null && n < min) return { hata: `en az ${min} olmalı` };
  if (max !== null && n > max) return { hata: `en çok ${max} olmalı` };
  return { deger: n };
}

export function degerOku(a: FormAlani, metin: string): Okunan {
  const bos = metin.trim() === '';
  if (a.tur === 'mantik') return { deger: metin === 'true' };
  if (bos) return a.zorunlu ? { hata: 'zorunlu' } : { atla: true };
  if (a.tur === 'tamsayi' || a.tur === 'sayi') return skaler(a.tur, metin, a.min, a.max, null);
  if (a.tur === 'metin') return { deger: metin };
  if (a.tur === 'secim') return a.secenekler !== null && a.secenekler.includes(metin) ? { deger: metin } : { hata: `şunlardan biri olmalı: ${(a.secenekler ?? []).join(', ')}` };
  const parcalar = metin.split(',').map((p) => p.trim());
  if (parcalar.some((p) => p === '')) return { hata: 'virgülle ayrılmış değerlerde boş öğe var' };
  if (a.minOge !== null && parcalar.length < a.minOge) return { hata: `en az ${a.minOge} öğe olmalı` };
  if (a.maxOge !== null && parcalar.length > a.maxOge) return { hata: `en çok ${a.maxOge} öğe olmalı` };
  const ogeler: unknown[] = [];
  for (const p of parcalar) {
    const o = skaler(a.ogeTuru ?? 'metin', p, a.min, a.max, a.secenekler);
    if ('hata' in o) return { hata: `"${p}": ${o.hata}` };
    if ('deger' in o) ogeler.push(o.deger);
  }
  return { deger: ogeler };
}

/** Bütün alanları okur; hatalıysa alan adına göre hata iletileri döner (gönderilmez). */
export function parametreleriOku(alanlar: readonly FormAlani[], metinler: Readonly<Record<string, string>>): { parametreler: Record<string, unknown> } | { hatalar: Record<string, string> } {
  const p: Record<string, unknown> = {};
  const hatalar: Record<string, string> = {};
  for (const a of alanlar) {
    const o = degerOku(a, metinler[a.ad] ?? (a.tur === 'mantik' ? 'false' : ''));
    if ('hata' in o) hatalar[a.ad] = o.hata;
    else if ('deger' in o) p[a.ad] = o.deger;
  }
  return Object.keys(hatalar).length > 0 ? { hatalar } : { parametreler: p };
}
