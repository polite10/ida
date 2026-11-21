// A3 ikili telemetri çerçevelerinin çözücüsü (A3 §3.1–3.4, A3-K18…K28).
//
// Bir WebSocket ikili iletisi bir ya da daha çok çerçeve taşır; her çerçeve
// 24 baytlık başlık ve `yuk_uzunlugu` bayt yükten oluşur. Yük alanları
// katalogdaki sırayla, doldurmasız ve küçük uçlu okunur.

import {
  BASLIK_BAYT,
  KANALLAR,
  YUKLER,
  type AlanTanimi,
  type KanalTanimi,
  type YokEtiketi,
  type YukAdi,
  type YukTipleri,
} from './katalog';

/** Başlık bayrak bitleri (A3-K28). */
export const BAYRAK = {
  KAYNAK_KAYIT: 1 << 0,
  OZET: 1 << 1,
  ONCESINDE_DUSME: 1 << 2,
  YER_GERCEGI: 1 << 3,
} as const;

/** `yuk_uzunlugu` üst sınırı (A3-K21). */
export const YUK_AZAMI_BAYT = 65536;

export interface CerceveBasligi {
  /** Kanalın zaman anlamına göre sim zamanı [ns] (A3-K26); kayıpsız. */
  readonly t_ns: bigint;
  readonly sira: number;
  readonly yuk_uzunlugu: number;
  readonly kanal_id: number;
  readonly kanal_surumu: number;
  readonly bayraklar: number;
}

export interface Cerceve<T = unknown> {
  readonly baslik: CerceveBasligi;
  readonly kanal: KanalTanimi;
  readonly yuk: T;
  /** A3-K24: değeri aralık ya da değer listesi dışında kalan, geçersiz UTF-8 taşıyan ya da `azami`'yi aşan alanların yolları. */
  readonly gecersiz: readonly string[];
  /** A3-K19: istemcinin bildiğinden küçük `kanal_surumu`'lu çerçevede yükte bulunmayan, "yok" sayılan alanlar. */
  readonly eksik: readonly string[];
}

/** İstemci tarafı sayaçlar (A3-K18, K24, K25). */
export interface CozucuSayaclari {
  ileti: number;
  cerceve: number;
  /** A3-K25: yük okuması `yuk_uzunlugu`'nu aşan ya da başlığı tutarsız çerçeve. */
  istemci_bozuk_cerceve: number;
  /** A3-K24: geçersiz gösterilen alan sayısı. */
  istemci_gecersiz_alan: number;
  /** A3-K18: bilinmeyen `kanal_id`, atlandı. */
  istemci_bilinmeyen_kanal: number;
  /** A3-K25: ileti sonunda 24 bayttan kısa kalan, atıldı. */
  istemci_kisa_kalan: number;
}

export function yeniSayaclar(): CozucuSayaclari {
  return {
    ileti: 0,
    cerceve: 0,
    istemci_bozuk_cerceve: 0,
    istemci_gecersiz_alan: 0,
    istemci_bilinmeyen_kanal: 0,
    istemci_kisa_kalan: 0,
  };
}

const KANAL_ID: ReadonlyMap<number, KanalTanimi> = new Map(KANALLAR.map((k) => [k.kanal_id, k as KanalTanimi]));
const KANAL_AD: ReadonlyMap<string, KanalTanimi> = new Map(KANALLAR.map((k) => [k.ad, k as KanalTanimi]));

export function kanalBul(adYaDaId: string | number): KanalTanimi | undefined {
  return typeof adYaDaId === 'number' ? KANAL_ID.get(adYaDaId) : KANAL_AD.get(adYaDaId);
}

/** "Yok" değerlerinin tamsayı karşılığı; NaN kayan noktalı alanlar içindir. */
export const YOK_TAMSAYI: Readonly<Record<Exclude<YokEtiketi, 'NaN'>, bigint>> = {
  INT64_MIN: -(1n << 63n),
  '2^64-1': (1n << 64n) - 1n,
  '0xFFFFFFFF': 0xffffffffn,
  '255': 255n,
  '-1': -1n,
  '0': 0n,
};

/** Okuma `son`'u aşınca atılan işaret (A3-K25). */
class TasmaHatasi {
  readonly tasma = true;
}
const TASMA = new TasmaHatasi();

const UTF8_KATI = new TextDecoder('utf-8', { fatal: true });
const UTF8_GEVSEK = new TextDecoder('utf-8');

class Okuyucu {
  konum: number;
  constructor(
    private readonly dv: DataView,
    baslangic: number,
    readonly son: number,
  ) {
    this.konum = baslangic;
  }
  private yer(n: number): number {
    const k = this.konum;
    if (k + n > this.son) throw TASMA;
    this.konum = k + n;
    return k;
  }
  kalan(): number {
    return this.son - this.konum;
  }
  /** Tamsayıyı ham (bigint) ve sayı olarak okur. */
  tamsayi(tip: string): { ham: bigint; sayi: number } {
    switch (tip) {
      case 'u8': {
        const v = this.dv.getUint8(this.yer(1));
        return { ham: BigInt(v), sayi: v };
      }
      case 'u16': {
        const v = this.dv.getUint16(this.yer(2), true);
        return { ham: BigInt(v), sayi: v };
      }
      case 'u32': {
        const v = this.dv.getUint32(this.yer(4), true);
        return { ham: BigInt(v), sayi: v };
      }
      case 'i32': {
        const v = this.dv.getInt32(this.yer(4), true);
        return { ham: BigInt(v), sayi: v };
      }
      case 'u64': {
        const v = this.dv.getBigUint64(this.yer(8), true);
        return { ham: v, sayi: Number.NaN }; // 64 bit: yalnız `ham` kullanılır
      }
      case 'i64': {
        const v = this.dv.getBigInt64(this.yer(8), true);
        return { ham: v, sayi: Number.NaN };
      }
      default:
        throw new Error('tamsayı değil: ' + tip);
    }
  }
  kayan(tip: string): number {
    return tip === 'f32' ? this.dv.getFloat32(this.yer(4), true) : this.dv.getFloat64(this.yer(8), true);
  }
  sayac(tip: 'u8' | 'u16' | 'u32'): number {
    return this.tamsayi(tip).sayi;
  }
  baytlar(n: number): Uint8Array {
    const k = this.yer(n);
    // Kopya: büyük iletinin arabelleğini tutmamak için.
    return new Uint8Array(this.dv.buffer.slice(this.dv.byteOffset + k, this.dv.byteOffset + k + n));
  }
}

interface OkumaDurumu {
  gecersiz: string[];
}

function tamsayiTipi(tip: string): boolean {
  return tip === 'u8' || tip === 'u16' || tip === 'u32' || tip === 'u64' || tip === 'i32' || tip === 'i64';
}

function skalerOku(o: Okuyucu, alan: AlanTanimi, yol: string, d: OkumaDurumu): number | bigint | null {
  if (tamsayiTipi(alan.tip)) {
    const { ham, sayi } = o.tamsayi(alan.tip);
    if (alan.yok !== undefined && alan.yok !== 'NaN' && ham === YOK_TAMSAYI[alan.yok]) return null;
    const altmisDort = alan.tip === 'u64' || alan.tip === 'i64';
    if (alan.degerler !== undefined && (altmisDort || !Object.prototype.hasOwnProperty.call(alan.degerler, sayi))) d.gecersiz.push(yol);
    else if (alan.aralik !== undefined && (ham < BigInt(alan.aralik[0]) || ham > BigInt(alan.aralik[1]))) d.gecersiz.push(yol);
    // 64 bit alanlar kayıpsız bigint olarak döner; Number'a çevrilmez.
    return altmisDort ? ham : sayi;
  }
  const v = o.kayan(alan.tip);
  if (Number.isNaN(v) && alan.yok === 'NaN') return null;
  if (!Number.isFinite(v)) d.gecersiz.push(yol);
  else if (alan.aralik !== undefined && (v < alan.aralik[0] || v > alan.aralik[1])) d.gecersiz.push(yol);
  return v;
}

function alanOku(o: Okuyucu, alan: AlanTanimi, yol: string, d: OkumaDurumu): unknown {
  switch (alan.tip) {
    case 'metin': {
      const n = o.sayac(alan.sayac ?? 'u16');
      const b = o.baytlar(n);
      if (alan.azami !== undefined && n > alan.azami) d.gecersiz.push(yol);
      try {
        return UTF8_KATI.decode(b);
      } catch {
        d.gecersiz.push(yol);
        return UTF8_GEVSEK.decode(b);
      }
    }
    case 'bayt_dizisi': {
      const n = o.sayac(alan.sayac ?? 'u16');
      if (alan.azami !== undefined && n > alan.azami) d.gecersiz.push(yol);
      return o.baytlar(n);
    }
    case 'dizi': {
      const n = o.sayac(alan.sayac ?? 'u16');
      if (alan.azami !== undefined && n > alan.azami) d.gecersiz.push(yol);
      const ogeler: Record<string, unknown>[] = [];
      for (let i = 0; i < n; i++) {
        const nesne: Record<string, unknown> = {};
        for (const alt of alan.oge ?? []) nesne[alt.ad] = alanOku(o, alt, `${yol}[${i}].${alt.ad}`, d);
        ogeler.push(nesne);
      }
      return ogeler;
    }
    default: {
      if (alan.adet !== undefined) {
        const dizi: (number | bigint | null)[] = [];
        for (let i = 0; i < alan.adet; i++) dizi.push(skalerOku(o, alan, `${yol}[${i}]`, d));
        return dizi;
      }
      return skalerOku(o, alan, yol, d);
    }
  }
}

export interface YukSonucu<T> {
  yuk: T;
  gecersiz: string[];
  eksik: string[];
}

/**
 * Bir yükü çözer. Yük `uzunluk`'u aşacaksa ya da `surum` < 1 ise `null` döner (bozuk; A3-K25).
 * `surum` > `bilinen` ise bilinen alanlardan sonraki baytlar yok sayılır (A3-K19).
 * Katalogda 1. sürümden sonra eklenmiş alan olmadığından `eksik` şimdilik hep boştur.
 */
export function yukCoz<Y extends YukAdi>(
  yukAdi: Y,
  dv: DataView,
  baslangic: number,
  uzunluk: number,
  surum = 1,
  bilinen = 1,
): YukSonucu<YukTipleri[Y]> | null {
  // Kanal sürümleri 1'den başlar ve küçük sürüm yalnız yükün SONUNA alan ekler (A3-K14);
  // dolayısıyla her sürüm 1. sürümün bütün alanlarını taşır. Katalogdaki bütün alanlar
  // 1. sürümündür: 1'den küçük sürümlü ya da 1. sürüm alanı eksik çerçeve bozuktur ve
  // panele eksik (null) alanlı nesne ulaşmaz. Yükün sonunda "yok" sayılabilecek alanlar
  // ancak katalog alan başına giriş sürümü taşıdığında (gelecek sürüm) mümkündür.
  if (surum < 1) return null;
  const o = new Okuyucu(dv, baslangic, baslangic + uzunluk);
  const d: OkumaDurumu = { gecersiz: [] };
  const eksik: string[] = [];
  const nesne: Record<string, unknown> = {};
  try {
    void bilinen;
    for (const alan of YUKLER[yukAdi] as readonly AlanTanimi[]) nesne[alan.ad] = alanOku(o, alan, alan.ad, d);
  } catch (h) {
    if (h === TASMA) return null;
    throw h;
  }
  return { yuk: nesne as unknown as YukTipleri[Y], gecersiz: d.gecersiz, eksik };
}

export function baslikOku(dv: DataView, k: number): CerceveBasligi {
  return {
    t_ns: dv.getBigInt64(k, true),
    sira: dv.getUint32(k + 8, true),
    yuk_uzunlugu: dv.getUint32(k + 12, true),
    kanal_id: dv.getUint16(k + 16, true),
    kanal_surumu: dv.getUint16(k + 18, true),
    bayraklar: dv.getUint16(k + 20, true),
  };
}

/**
 * Durumlu çözücü: iletileri çerçevelere ayırır, yükleri çözer ve
 * istemci sayaçlarını tutar.
 */
export class Cozucu {
  readonly sayaclar: CozucuSayaclari = yeniSayaclar();

  ileti(veri: ArrayBuffer | Uint8Array): Cerceve[] {
    const u8 = veri instanceof Uint8Array ? veri : new Uint8Array(veri);
    const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength);
    const s = this.sayaclar;
    const cikti: Cerceve[] = [];
    s.ileti++;
    let p = 0;
    const uzunluk = u8.byteLength;
    while (p < uzunluk) {
      if (uzunluk - p < BASLIK_BAYT) {
        // A3-K25: ileti sonunda 24 bayttan kısa kalan atılır.
        s.istemci_kisa_kalan++;
        break;
      }
      const baslik = baslikOku(dv, p);
      const yukBas = p + BASLIK_BAYT;
      const yukSon = yukBas + baslik.yuk_uzunlugu;
      if (yukSon > uzunluk) {
        // Uzunluk iletinin dışına taşıyor: sonraki çerçevenin yeri bilinemez.
        s.istemci_bozuk_cerceve++;
        break;
      }
      p = yukSon;
      if (baslik.yuk_uzunlugu > YUK_AZAMI_BAYT) {
        s.istemci_bozuk_cerceve++;
        continue;
      }
      const kanal = KANAL_ID.get(baslik.kanal_id);
      if (kanal === undefined) {
        // A3-K18: bilinmeyen kanal `yuk_uzunlugu` kadar atlanır.
        s.istemci_bilinmeyen_kanal++;
        continue;
      }
      const sonuc = yukCoz(kanal.yuk, dv, yukBas, baslik.yuk_uzunlugu, baslik.kanal_surumu, kanal.kanal_surumu);
      if (sonuc === null) {
        s.istemci_bozuk_cerceve++;
        continue;
      }
      if (baslik.t_ns < 0n) sonuc.gecersiz.push('baslik.t_ns');
      s.istemci_gecersiz_alan += sonuc.gecersiz.length;
      s.cerceve++;
      cikti.push({ baslik, kanal, yuk: sonuc.yuk, gecersiz: sonuc.gecersiz, eksik: sonuc.eksik });
    }
    return cikti;
  }
}
