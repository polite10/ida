// `olay` kanalının çerçevesini ve `GET /olaylar` öğesini tek biçime getirir.

import { BAYRAK, type Cerceve } from './cerceve';
import { OLAY_TURLERI, YUKLER, type YukOlay } from './katalog';
import type { OlayJson } from './iletiler';

export type OnemAdi = 'bilgi' | 'uyari' | 'hata' | 'emniyet';

export interface OlayKaydi {
  /** Canlı olayda sim süreci boyunca olay sırası; kayıttan gelende MCAP `sequence` (A3-K27, K112). */
  sira: number;
  t_ns: bigint;
  /** Başlıkta `kaynak_kayit` biti: olay izleme kipinde kayıttan üretildi (A3-K28, K112). */
  kaynak_kayit: boolean;
  /** Tür adı; katalogda yoksa `tur_<kod>` (A3-K18: bilinmeyen tür kodu ve ayrıntısıyla gösterilir). */
  tur: string;
  tur_kodu: number | null;
  onem: OnemAdi | string;
  kaynak: string;
  adim_no: bigint | null;
  komut_no: number;
  istemci_id: number;
  ayrinti: Record<string, unknown> | null;
  /** Ayrıntı JSON olarak çözülemediyse ham metin. */
  ayrinti_ham: string | null;
  /** Kayıt, yeniden bağlanmada HTTP geçmişinden doldurulduysa. */
  gecmisten: boolean;
}

const TUR_KOD = new Map<number, string>(OLAY_TURLERI.map((t) => [t.kod, t.ad]));
const TUR_AD = new Map<string, number>(OLAY_TURLERI.map((t) => [t.ad, t.kod]));

function degerAdlari(alan: string): Readonly<Record<number, string>> {
  const a = YUKLER.yuk_olay.find((x) => x.ad === alan);
  return a !== undefined && 'degerler' in a ? (a.degerler as Readonly<Record<number, string>>) : {};
}
const ONEM = degerAdlari('onem');
const KAYNAK = degerAdlari('kaynak');

export function olayTurAdi(kod: number): string {
  return TUR_KOD.get(kod) ?? `tur_${kod}`;
}

export function cercevedenOlay(c: Cerceve<YukOlay>): OlayKaydi {
  const y = c.yuk;
  let ayrinti: Record<string, unknown> | null = null;
  let ham: string | null = null;
  try {
    const v: unknown = JSON.parse(y.ayrinti);
    if (v !== null && typeof v === 'object' && !Array.isArray(v)) ayrinti = v as Record<string, unknown>;
    else ham = y.ayrinti;
  } catch {
    ham = y.ayrinti;
  }
  return {
    sira: c.baslik.sira,
    t_ns: c.baslik.t_ns,
    kaynak_kayit: (c.baslik.bayraklar & BAYRAK.KAYNAK_KAYIT) !== 0,
    tur: olayTurAdi(y.tur),
    tur_kodu: y.tur,
    onem: ONEM[y.onem] ?? `onem_${y.onem}`,
    kaynak: KAYNAK[y.kaynak] ?? `kaynak_${y.kaynak}`,
    adim_no: y.adim_no,
    komut_no: y.komut_no,
    istemci_id: y.istemci_id,
    ayrinti,
    ayrinti_ham: ham,
    gecmisten: false,
  };
}

export function jsondanOlay(o: OlayJson): OlayKaydi {
  // JSON sayıları 2^53 üstünde zaten yuvarlanmış gelir (JSON.parse); ikili yolda kayıp yoktur.
  return {
    sira: o.sira,
    t_ns: BigInt(Math.trunc(o.t_ns)),
    kaynak_kayit: false,
    tur: o.tur,
    tur_kodu: TUR_AD.get(o.tur) ?? null,
    onem: o.onem ?? 'bilgi',
    kaynak: o.kaynak ?? '',
    adim_no: o.adim_no === null || o.adim_no === undefined ? null : BigInt(Math.trunc(o.adim_no)),
    komut_no: o.komut_no ?? 0,
    istemci_id: o.istemci_id ?? 0,
    ayrinti: o.ayrinti,
    ayrinti_ham: null,
    gecmisten: true,
  };
}
