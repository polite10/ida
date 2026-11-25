// A3 WebSocket metin iletileri ve HTTP yanıtlarının tipleri
// (a3-kanallar.json `$defs`; A3 §3.6–3.9). Yalnız istemcinin kullandığı alanlar
// tiplenir; bilinmeyen alanlar yok sayılır (A3-K18).

import type { HataKodu } from './katalog';

export type Hz = 50 | 25 | 10 | 5 | 2 | 1;
export const GECERLI_HZ: readonly Hz[] = [50, 25, 10, 5, 2, 1];

// ---- WebSocket, sunucudan ----

export interface WsMerhaba {
  tur: 'merhaba';
  a3_surumu: string;
  katalog_ozeti: string;
  baglanti_id: number;
  run_id: string | null;
  son_olay_sirasi: number;
  yayin_tavani_hz: 50 | 25 | 10;
}

export interface WsAbonelikYanitiKanal {
  kanal: string;
  kanal_id?: number;
  kanal_surumu?: number;
  etkin_hz?: number | null;
  sonuc: 'kabul' | 'ret';
  neden?: 'bilinmeyen_kanal' | 'zorunlu_kanal' | 'kademe_kapali' | 'gecersiz_hz';
}

export interface WsAbonelikYaniti {
  tur: 'abonelik_yaniti';
  istek_no: number;
  kanallar: WsAbonelikYanitiKanal[];
}

export interface WsHata {
  tur: 'hata';
  istek_no?: number | null;
  kod: 'gecersiz_ileti' | 'bilinmeyen_tur';
  ileti: string;
}

export type WsSunucuIletisi = WsMerhaba | WsAbonelikYaniti | WsHata;

// ---- WebSocket, istemciden ----

export interface WsAboneOl {
  tur: 'abone_ol';
  istek_no: number;
  kanallar: { kanal: string; hz?: Hz }[];
}

export interface WsAbonelikBirak {
  tur: 'abonelik_birak';
  istek_no: number;
  kanallar: string[];
}

// ---- HTTP ----

export interface YanitSurum {
  sonuc: 'kabul';
  a3_surumu: string;
  katalog_ozeti: string;
  ws_alt_protokolu: string;
  a6_surumu: string;
  sim_surumu: string;
  platform: 'macos-arm64' | 'windows-x64';
}

export interface YanitIstemci {
  sonuc: 'kabul';
  istemci_id: number;
}

export type CalistirmaDurumAdi =
  | 'bos'
  | 'yuklendi'
  | 'hazirlaniyor'
  | 'kosuyor'
  | 'duraklatildi'
  | 'bitti'
  | 'durduruldu'
  | 'hata'
  | 'izleme';

export type KipAdi = 'yok' | 'sil' | 'hil_1x' | 'hil_bariyer' | 'tekrar_fizik' | 'tekrar_girdi_sil' | 'tekrar_yeniden_kurma';

export interface YanitDurum {
  sonuc: 'kabul';
  durum: CalistirmaDurumAdi;
  kip: KipAdi;
  run_id: string | null;
  sim_t_ns: number;
  adim_no: number;
  gecerli: boolean;
  gercek_zamani_kacirdi: boolean;
  gecerlilik_nedenleri: string[];
  sayaclar: Record<string, number>;
  kademeler: { gorunum: string; kamera: string; sonar: string; kayit: string };
  yayin_tavani_hz: 50 | 25 | 10;
  senaryo_ozeti?: string | null;
  emniyet_rev: number;
  a1_run_id?: number | null;
  durum_rev: number;
  son_komut_no: number;
}

/** `GET /olaylar` yanıtındaki olay: `yuk_olay`'ın JSON karşılığı (A3-K45). */
export interface OlayJson {
  sira: number;
  t_ns: number;
  tur: string;
  onem?: string;
  kaynak?: string;
  adim_no?: number | null;
  komut_no?: number;
  istemci_id?: number;
  ayrinti: Record<string, unknown>;
}

export interface YanitOlaylar {
  sonuc: 'kabul';
  olaylar: OlayJson[];
  tamam: boolean;
}

export interface OnDenetimMaddesi {
  kod: string;
  durum: 'gecti' | 'kaldi' | 'uyari' | 'atlandi';
  olculen?: unknown;
  sinir?: unknown;
  birim?: string | null;
  ileti?: string;
  dayanak?: string;
}

export interface OnDenetimRaporu {
  rapor_surumu: string;
  rapor_kimligi: string;
  sonuc: 'gecti' | 'kaldi';
  kip: string;
  olusturma_utc: string;
  maddeler: OnDenetimMaddesi[];
}

export interface YanitKuyrukta {
  sonuc: 'kuyrukta';
  komut_no: number;
  yineleme?: boolean;
}

export interface YanitRet {
  sonuc: 'ret';
  yineleme?: boolean;
  komut_no?: number | null;
  hata: { kod: HataKodu | string; ileti: string; ayrinti?: Record<string, unknown> };
}

/** `GET /komutlar/{komut_no}` (A3-K60). */
export interface YanitKomut {
  sonuc: 'kabul';
  komut_no: number;
  durum: 'kuyrukta' | 'uygulandi' | 'reddedildi';
  uygulama_adimi?: number;
  hata?: { kod?: string; ileti?: string; ayrinti?: Record<string, unknown> } & Record<string, unknown>;
  sonuc_ayrinti?: Record<string, unknown>;
}

export interface SenaryoListeOgesi {
  ad: string;
  boyut_bayt?: number;
  degisim_utc?: string;
}

export interface YanitSenaryoListesi {
  sonuc: 'kabul';
  senaryolar: SenaryoListeOgesi[];
}

export interface YanitSenaryoDosyasi {
  sonuc: 'kabul';
  ad: string;
  icerik: string;
}

/** A5 doğrulama raporu (A5 §3.14). */
export interface A5Bulgusu {
  kural: string;
  onem: 'hata' | 'uyari';
  asama?: number;
  dosya?: string;
  yol?: string;
  satir?: number | null;
  ileti: string;
}

export interface A5Raporu {
  gecerli: boolean;
  davranis_ozeti?: string | null;
  kaynak_ozetleri?: Record<string, string>;
  bulgular: A5Bulgusu[];
}

export interface YanitSenaryoDogrula {
  sonuc: 'kabul';
  yineleme?: boolean;
  gecerli: boolean;
  senaryo_ozeti?: string | null;
  rapor: A5Raporu;
}

export interface YanitSenaryoYukle {
  sonuc: 'kabul';
  komut_no: number;
  senaryo_ozeti: string;
  rapor: A5Raporu;
  yineleme?: boolean;
}
