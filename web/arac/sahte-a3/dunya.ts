// Sahte A3 sunucusunun "dünyası": yazılı bir senaryoyu basit kinematikle
// oynatır ve A3 kanallarının yüklerini üretir. FİZİK DEĞİLDİR; değerler
// yalnız panellerin geliştirilmesi içindir (W3-web dilim 1).
//
// Senaryo (sim zamanı, yaklaşık): araç yüzeyde silahsız başlar → 3 s'de
// silahlanır, 3 m'ye dalar → dört yol noktasını dolaşır → ~72 s'de 3 numaralı
// itici zayıflar (itki çarpanı 0,35), sapma oluşur ve denetleyici toparlar →
// ~100 s ve ~118 s'de senaryodan emniyet girdisi değişimi → ~125–140 s
// fırlatıcı geri okuma arızası (kartın okuması yetkili durumdan farklı) →
// son yol noktasından sonra yüzer, silahsızlanır, çalıştırma biter; birkaç
// saniye sonra sahte sunucu yeni çalıştırma başlatır.

import type * as K from '../../src/a3/katalog';
import type { KanalAdi, KodlamaGirdisi } from '../../src/a3/katalog';

// Sahte dünya kodlayıcı girdisi üretir: 64 bit alanlar güvenli tamsayı (number) olabilir.
type KanalYuku = { [A in keyof K.KanalYuku]: KodlamaGirdisi<K.KanalYuku[A]> };
type YukAtisDurum = KodlamaGirdisi<K.YukAtisDurum>;
type YukCalistirmaDurum = KodlamaGirdisi<K.YukCalistirmaDurum>;
type YukEmniyetGirdiler = KodlamaGirdisi<K.YukEmniyetGirdiler>;
type YukFizikDurum = KodlamaGirdisi<K.YukFizikDurum>;
type YukKartDurum = KodlamaGirdisi<K.YukKartDurum>;
type YukSayacGercekZaman = KodlamaGirdisi<K.YukSayacGercekZaman>;

export const ADIM_NS = 1_250_000; // fizik adımı 1,25 ms (iskelet §4)
export const DUNYA_ADIMI_NS = 10_000_000; // sahte dünyanın kinematik adımı 10 ms
const ADIM_PER = DUNYA_ADIMI_NS / ADIM_NS; // 8

export type Kip = 'sil' | 'hil_1x';

export interface Uretim<K extends KanalAdi = KanalAdi> {
  kanal: K;
  t_ns: number;
  /** Anahtarlı kanallarda `sensor_id:inst`; diğerlerinde boş. */
  anahtar: string;
  yuk: KanalYuku[K];
}

export interface UretilenOlay {
  tur: number;
  onem: number;
  kaynak: number;
  adim_no: number | null;
  ayrinti: Record<string, unknown>;
}

// ---- Rastgele sayı (tohumlu, yeniden üretilebilir) ----

function mulberry32(tohum: number): () => number {
  let a = tohum >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

class Rastgele {
  private r: () => number;
  private yedek: number | null = null;
  constructor(tohum: number) {
    this.r = mulberry32(tohum);
  }
  duz(): number {
    return this.r();
  }
  normal(): number {
    if (this.yedek !== null) {
      const y = this.yedek;
      this.yedek = null;
      return y;
    }
    let u = 0;
    while (u === 0) u = this.r();
    const v = this.r();
    const m = Math.sqrt(-2 * Math.log(u));
    this.yedek = m * Math.sin(2 * Math.PI * v);
    return m * Math.cos(2 * Math.PI * v);
  }
  /** Rayleigh dağılımlı benek (ortalama ≈ 1). */
  benek(): number {
    let u = 0;
    while (u === 0) u = this.r();
    return Math.sqrt(-2 * Math.log(u)) / 1.2533;
  }
}

// ---- Geometri ----

function sinirla(v: number, a: number, b: number): number {
  return v < a ? a : v > b ? b : v;
}

function aciFarki(a: number, b: number): number {
  let d = a - b;
  while (d > Math.PI) d -= 2 * Math.PI;
  while (d < -Math.PI) d += 2 * Math.PI;
  return d;
}

/** ZYX Euler açılarından gövdeden NED'e Hamilton kuaterniyon (w, x, y, z). */
export function eulerdenKuaterniyon(yatis: number, yunuslama: number, yon: number): [number, number, number, number] {
  const cr = Math.cos(yatis / 2);
  const sr = Math.sin(yatis / 2);
  const cp = Math.cos(yunuslama / 2);
  const sp = Math.sin(yunuslama / 2);
  const cy = Math.cos(yon / 2);
  const sy = Math.sin(yon / 2);
  return [
    cr * cp * cy + sr * sp * sy,
    sr * cp * cy - cr * sp * sy,
    cr * sp * cy + sr * cp * sy,
    cr * cp * sy - sr * sp * cy,
  ];
}

// ---- Senaryo sabitleri (sahte; A5 değildir) ----

export const SU_DERINLIGI_M = 7.0;
export const HAVUZ = { kuzeyMin: -12, kuzeyMax: 28, doguMin: -12, doguMax: 28 };
export const HEDEF = { govde_id: 100, kimlik: 1, n: 6, e: 21, d: 3.2, yaricap: 0.45 };
export const YOL_NOKTALARI: readonly { n: number; e: number; d: number }[] = [
  { n: 15, e: 0, d: 3 },
  { n: 15, e: 15, d: 3 },
  { n: 0, e: 15, d: 3 },
  { n: 0, e: 0, d: 3 },
];
const KAYALAR: readonly { n: number; e: number; r: number }[] = [
  { n: 20, e: -6, r: 0.8 },
  { n: -7, e: 9, r: 0.6 },
  { n: 24, e: 18, r: 1.0 },
  { n: 3, e: 25, r: 0.5 },
  { n: -9, e: -4, r: 0.7 },
];
const ITICI_AZAMI_N = 50; // T200 sınıfı, yaklaşık (tahmin)
const ARIZA_ITICI = 2; // 0 tabanlı dizin: 3 numaralı itici
const ARIZA_CARPAN = 0.35;
export const SES_HIZI = 1481;

export const SENSORLER = {
  imu: 1,
  manyetometre: 2,
  basinc: 3,
  gnss: 4,
  dvl: 5,
  ping1d: 6,
  ping360: 7,
  batarya: 8,
  sizinti: 9,
  firlatici: 10,
} as const;

// Ping360 ayarı: 30 m menzil, 500 örnek (cihaz birimleri; A3 §3.4 formülü)
const P360_N = 500;
const P360_MENZIL = 30;
const P360_PERIYOT = Math.round((2 * P360_MENZIL) / (SES_HIZI * P360_N) / 25e-9); // 25 ns biriminde
const P360_ADIM_GRAD = 2;
const P1D_N = 200;

type CalismaDurumu = 'bos' | 'yuklendi' | 'hazirlaniyor' | 'kosuyor' | 'duraklatildi' | 'bitti' | 'durduruldu' | 'hata';
const DURUM_KOD: Record<CalismaDurumu, number> = {
  bos: 0,
  yuklendi: 1,
  hazirlaniyor: 2,
  kosuyor: 3,
  duraklatildi: 4,
  bitti: 5,
  durduruldu: 6,
  hata: 7,
};

interface Ariza {
  fault_id: string;
  sinif: number;
  hedef: string;
  tur: string;
  kaynak: number;
  baslangic_adimi: number;
  bitis_adimi: number | null;
}

interface Arac {
  n: number;
  e: number;
  d: number;
  yon: number;
  yatis: number;
  yunuslama: number;
  u: number;
  v: number;
  w: number;
  r: number;
  p: number;
  q: number;
  an: [number, number, number];
  // denetleyici iç durumları
  yonIntegral: number;
  derinlikHedef: number;
  hizHedef: number;
  yolNoktasi: number; // -1: dalış, 0..3, 4: yüzeye çıkış, 5: bitti
  silahli: boolean;
  kip: number; // A4 IDA_MODE_*
  soc: number;
  iticiU: number[];
}

function yeniArac(): Arac {
  return {
    n: 0,
    e: 0,
    d: 0.12,
    yon: 0,
    yatis: 0,
    yunuslama: 0,
    u: 0,
    v: 0,
    w: 0,
    r: 0,
    p: 0,
    q: 0,
    an: [0, 0, 0],
    yonIntegral: 0,
    derinlikHedef: 0.12,
    hizHedef: 0,
    yolNoktasi: -1,
    silahli: false,
    kip: 1,
    soc: 0.92,
    iticiU: [0, 0, 0, 0, 0, 0, 0, 0],
  };
}

export type EmniyetKipi = 'olagan' | 'bilinmiyor' | 'gecersiz';

export interface DunyaAyari {
  tohum?: number;
  kip?: Kip;
  /** Yalnız gösterim sınaması: kart geri okuması bilinmiyor ya da değer listesi dışı değerler. */
  emniyetKipi?: EmniyetKipi;
  /**
   * Elle yönetim: dünya kendiliğinden başlamaz ve bitince yeniden başlamaz; çalıştırma
   * yalnız A3 komutlarıyla (başlat, duraklat, …) ilerler (panelden çalıştırma denetimi).
   */
  elle?: boolean;
}

/** Senaryonun bir çalıştırmasını ve aralarındaki sıfırlamaları yürüten sahte dünya. */
export class SahteDunya {
  readonly kip: Kip;
  readonly emniyetKipi: EmniyetKipi;
  readonly elle: boolean;
  /** İstenen hız çarpanı (A3-K66); 0 = olabildiğince hızlı (sahtede 8×). */
  carpan = 1;
  /** Yüklü senaryonun dosya adı. */
  senaryoAdi = 'z1.toml';
  /** `adimla` ile istenen, henüz yürütülmemiş kontrol periyodu sayısı. */
  private adimBekleyen = 0;
  private readonly rnd: Rastgele;
  /** Çalıştırma içindeki sim zamanı (ns). */
  t_ns = 0;
  adim_no = 0;
  durum: CalismaDurumu = 'yuklendi';
  run_id = '';
  durum_rev = 1;
  emniyet_rev = 1;
  /** Durum geçişlerini sürükleyen duvar saati sayacı (ms; dünya dışı). */
  private bekleme_ms = 0;
  private calistirmaNo = 0;
  private a = yeniArac();
  private onceki = yeniArac();
  private seq = new Map<string, number>();
  private ping360Aci = 0;
  private kestirimSapmasi = { n: 0, e: 0, yon: 0 };
  private arizalar: Ariza[] = [];
  private yetkili = { acil: 0, fis: [0, 0], pim: [1, 1], sonAdim: null as number | null, sonKaynak: 0 };
  private kartGoru = { acil: 0, fis: [0, 0], pim: [1, 1] };
  private kartGoruGecmisi: { t: number; acil: number; fis: number[]; pim: number[] }[] = [];
  private sayac = {
    adim_son_tarih_ihlali: 0,
    azami_adim_gecikmesi_ns: 0,
    parti_gonderilen: 0,
  };
  private zamanCizelgesi = new Set<string>();
  private kartAcilis = 7;
  private sonRaporKimligi = '';
  /** Üretilen olaylar; sunucu sıra numarası verip yayınlar. */
  readonly olayKuyrugu: UretilenOlay[] = [];
  private uretim: Uretim[] = [];

  constructor(ayar: DunyaAyari = {}) {
    this.kip = ayar.kip ?? 'sil';
    this.emniyetKipi = ayar.emniyetKipi ?? 'olagan';
    this.elle = ayar.elle ?? false;
    this.rnd = new Rastgele(ayar.tohum ?? 20261121);
    this.durum = 'yuklendi';
    this.bekleme_ms = 600;
  }

  get raporKimligi(): string {
    return this.sonRaporKimligi;
  }

  /** Sahte senaryonun çözümlenmiş biçiminden panelin kullandığı parçalar (A5 alt kümesi). */
  static senaryo(): Record<string, unknown> {
    return {
      meta: { ad: 'sahte-a3', aciklama: 'W3-web sahte sunucu senaryosu; A5 dosyası değildir' },
      ortam: { derinlik: SU_DERINLIGI_M },
      hedefler: {
        h1: {
          kimlik: HEDEF.kimlik,
          sinif: 'sabit_nesne',
          varlik: 'hedef_ornek',
          konum: [HEDEF.n, HEDEF.e, HEDEF.d],
          yonelim: [1, 0, 0, 0],
          hareket: { tur: 'sabit' },
        },
      },
      gorev: {
        yukleme_zamani_ns: 0,
        ogeler: [
          { item_type: 'DIVE_TO', frame: 'NONE', p: [3, 0.25, 0, 0], lat_e7: 0, lon_e7: 0, north_m: 0, east_m: 0, depth: 3 },
          ...YOL_NOKTALARI.map((y) => ({
            item_type: 'WAYPOINT',
            frame: 'LOCAL',
            p: [0.8, 0.5, 0, 0],
            lat_e7: 0,
            lon_e7: 0,
            north_m: y.n,
            east_m: y.e,
            depth: y.d,
          })),
          { item_type: 'SURFACE', frame: 'NONE', p: [0.25, 0, 0, 0], lat_e7: 0, lon_e7: 0, north_m: 0, east_m: 0, depth: 0 },
        ],
      },
    };
  }

  // ---- Dış arayüz ----

  /**
   * Duvar saatinde `ms` kadar ilerler. Koşarken sim zamanı da aynı hızla (1x)
   * ilerler; diğer durumlarda yalnız durum geçişleri sayılır.
   * Üretilen yükleri döndürür.
   */
  ilerle(ms: number): Uretim[] {
    this.ilerleIc(ms);
    // Komut uygulamasının (uygula) ürettikleri de bu çağrının çıktısındadır.
    const u = this.uretim;
    this.uretim = [];
    return u;
  }
  /** Durağan durumlarda (yüklendi, duraklatıldı, …) durum kanalını 10 Hz tazeler. */
  private bosta_ms = 0;

  private ilerleIc(ms: number): Uretim[] {
    if (this.durum !== 'kosuyor' && this.adimBekleyen === 0) {
      this.bosta_ms += ms;
      if (this.bosta_ms >= 100) {
        this.bosta_ms = 0;
        this.calistirmaDurumuUret();
      }
    }
    if (this.adimBekleyen > 0) {
      // `adimla`: n kontrol periyodu = n · 5 ms; sahte dünya 10 ms'lik adımlarla ilerler (yukarı yuvarlanır).
      const adimlar = Math.ceil(this.adimBekleyen / 2);
      this.adimBekleyen = 0;
      for (let i = 0; i < adimlar && this.durum === 'kosuyor'; i++) this.adim();
      if (this.durum === 'kosuyor') this.gecis('duraklatildi', 'adimla_tamamlandi');
      return this.uretim;
    }
    if (this.durum !== 'kosuyor') {
      if (this.durum === 'duraklatildi' || this.durum === 'durduruldu') return this.uretim;
      if (this.elle && (this.durum === 'yuklendi' || this.durum === 'bitti')) return this.uretim;
      this.bekleme_ms -= ms;
      if (this.bekleme_ms <= 0) this.durumIlerlet();
      return this.uretim;
    }
    this.birikim += ms * 1e6 * (this.carpan === 0 ? 8 : this.carpan);
    while (this.birikim >= DUNYA_ADIMI_NS && this.durum === 'kosuyor') {
      this.birikim -= DUNYA_ADIMI_NS;
      this.adim();
    }
    return this.uretim;
  }
  private birikim = 0;

  /** Koşu başlayınca sim zamanını hızla `saniye`'ye getirir (yalnız geliştirme; çıktı atılır). */
  atla(saniye: number): void {
    while (this.durum !== 'kosuyor') this.ilerle(100);
    while (this.t_ns < saniye * 1e9 && this.durum === 'kosuyor') {
      this.uretim = [];
      this.adim();
    }
    this.uretim = [];
    this.olayKuyrugu.length = 0;
  }

  /** Sunucunun `yanit_durum` ve `merhaba` için kullandığı özet. */
  ozet(): {
    durum: CalismaDurumu;
    run_id: string | null;
    t_ns: number;
    adim_no: number;
    durum_rev: number;
    emniyet_rev: number;
  } {
    return {
      durum: this.durum,
      run_id: this.run_id === '' ? null : this.run_id,
      t_ns: this.t_ns,
      adim_no: this.adim_no,
      durum_rev: this.durum_rev,
      emniyet_rev: this.emniyet_rev,
    };
  }

  yetkiliEmniyet(): { acil: number; fis: number[]; pim: number[]; sonAdim: number | null } {
    return this.yetkili;
  }

  etkinArizalar(): readonly Ariza[] {
    return this.arizalar;
  }

  // ---- Durum makinesi ----

  private olay(tur: number, onem: number, kaynak: number, ayrinti: Record<string, unknown>, adim: number | null): void {
    this.olayKuyrugu.push({ tur, onem, kaynak, adim_no: adim, ayrinti });
  }

  private gecis(sonraki: CalismaDurumu, neden: string): void {
    const onceki = this.durum;
    this.durum = sonraki;
    this.durum_rev++;
    this.olay(1, 0, 0, { onceki, sonraki, neden }, this.durum === 'kosuyor' || onceki === 'kosuyor' ? this.adim_no : null);
    this.calistirmaDurumuUret();
  }

  /** `baslat`: yeni çalıştırma kimliği, sıfırlanmış sahte araç; durum `hazirlaniyor` (A3-K62, K63). */
  private calistirmaBaslat(neden: string): void {
    this.calistirmaNo++;
    this.run_id = runIdUret(this.rnd);
    this.t_ns = 0;
    this.adim_no = 0;
    this.a = yeniArac();
    this.onceki = yeniArac();
    this.arizalar = [];
    this.zamanCizelgesi.clear();
    this.kestirimSapmasi = { n: 0, e: 0, yon: 0.01 };
    this.sayac = { adim_son_tarih_ihlali: 0, azami_adim_gecikmesi_ns: 0, parti_gonderilen: 0 };
    this.yetkili = { acil: 0, fis: [0, 0], pim: [1, 1], sonAdim: null, sonKaynak: 0 };
    this.kartGoru = { acil: 0, fis: [0, 0], pim: [1, 1] };
    this.kartGoruGecmisi = [];
    this.birikim = 0;
    this.gecis('hazirlaniyor', neden);
  }

  /**
   * Bir çalıştırma kontrolü komutunu uygular (dünya adım sınırı; A3-K57). Durum ve revizyon
   * denetimleri sunucudadır; burada yalnız geçiş yapılır. Kontrol komutu durum geçişi
   * yapmıyorsa `durum_rev` yine bir artar (A3-K122).
   */
  uygula(komut: string, ek: { carpan?: number; periyot_sayisi?: number }): void {
    switch (komut) {
      case 'baslat':
        this.calistirmaBaslat('baslat');
        this.bekleme_ms = 800;
        return;
      case 'duraklat':
        this.gecis('duraklatildi', 'duraklat');
        return;
      case 'surdur':
        this.birikim = 0;
        this.gecis('kosuyor', 'surdur');
        return;
      case 'adimla':
        this.adimBekleyen = ek.periyot_sayisi ?? 1;
        this.gecis('kosuyor', 'adimla');
        return;
      case 'hiz':
        this.carpan = ek.carpan ?? 1;
        this.durum_rev++;
        this.calistirmaDurumuUret();
        return;
      case 'durdur':
        if (this.durum === 'hazirlaniyor') {
          this.run_id = '';
          this.gecis('yuklendi', 'durdur');
        } else this.gecis('durduruldu', 'durdur');
        return;
      case 'sifirla':
        this.run_id = '';
        this.t_ns = 0;
        this.adim_no = 0;
        this.adimBekleyen = 0;
        this.gecis('yuklendi', 'sifirla');
        return;
    }
  }

  /** `senaryo/yukle` (A3-K77): senaryo değişir, durum `yuklendi` olur. */
  senaryoYukle(ad: string): void {
    this.senaryoAdi = ad;
    this.run_id = '';
    this.t_ns = 0;
    this.adim_no = 0;
    this.gecis('yuklendi', 'senaryo_yukle');
  }

  private durumIlerlet(): void {
    switch (this.durum) {
      case 'yuklendi': {
        // sahte sunucu kendiliğinden baslat verir
        this.calistirmaBaslat('baslat (sahte sunucu)');
        this.bekleme_ms = 1500;
        return;
      }
      case 'hazirlaniyor': {
        this.sonRaporKimligi = hex(this.rnd, 16);
        this.olay(
          14,
          0,
          0,
          {
            rapor_kimligi: this.sonRaporKimligi,
            sonuc: 'gecti',
            kip: this.kip,
            sayilar: { gecti: 4, kaldi: 0, uyari: 0, atlandi: 3 },
            maddeler: ONDENETIM_MADDELERI(this.kip).map((m) => ({ kod: m.kod, durum: m.durum })),
          },
          null,
        );
        this.birikim = 0;
        this.gecis('kosuyor', 'ön denetim geçti');
        return;
      }
      case 'bitti': {
        this.run_id = '';
        this.gecis('yuklendi', 'sifirla (sahte sunucu; otomatik yeniden başlatma)');
        this.bekleme_ms = 1200;
        return;
      }
      default:
        return;
    }
  }

  // ---- Kinematik adım ----

  private adim(): void {
    const dt = DUNYA_ADIMI_NS / 1e9;
    this.t_ns += DUNYA_ADIMI_NS;
    this.adim_no += ADIM_PER;
    const t = this.t_ns / 1e9;
    const a = this.a;
    this.onceki = { ...a, an: [...a.an], iticiU: [...a.iticiU] };

    // Zaman çizelgesi
    if (t >= 3 && !a.silahli && a.yolNoktasi === -1) {
      a.silahli = true;
      a.kip = 4; // DEPTH_HOLD
      a.derinlikHedef = 3;
    }
    this.cizelge(t);

    // Güdüm
    const arizaEtkin = this.arizalar.some((f) => f.fault_id === 'itici_zayiflama');
    if (a.yolNoktasi === -1 && a.silahli && t > 18 && Math.abs(a.d - 3) < 0.12) {
      a.yolNoktasi = 0;
      a.kip = 7; // AUTO
    }
    let yonHedef = a.yon;
    if (a.yolNoktasi >= 0 && a.yolNoktasi < YOL_NOKTALARI.length) {
      const y = YOL_NOKTALARI[a.yolNoktasi]!;
      const dn = y.n - a.n;
      const de = y.e - a.e;
      const mesafe = Math.hypot(dn, de);
      yonHedef = Math.atan2(de, dn);
      a.hizHedef = mesafe < 2 ? 0.25 + 0.15 * mesafe : 0.55;
      a.derinlikHedef = y.d;
      if (mesafe < 0.9) a.yolNoktasi++;
    } else if (a.yolNoktasi === YOL_NOKTALARI.length) {
      a.hizHedef = 0;
      a.derinlikHedef = 0.12;
      a.kip = 4;
      if (a.d < 0.25 && Math.abs(a.u) < 0.05) {
        a.yolNoktasi++;
        this.yuzeydeBitis = t;
      }
    } else if (a.yolNoktasi > YOL_NOKTALARI.length) {
      if (t - this.yuzeydeBitis > 2 && a.silahli) {
        a.silahli = false;
        a.kip = 1;
      }
      if (t - this.yuzeydeBitis > 4) {
        this.gecis('bitti', 'senaryo süresi doldu');
        this.bekleme_ms = this.elle ? 0 : 6000;
        if (this.kip === 'hil_1x') this.emniyet_rev++; // oturum sonu (A3-K117 md.4)
        return;
      }
    }

    // Yön denetimi (PI + sönüm) ve itici arızasının bozucu momenti
    const yonHata = aciFarki(yonHedef, a.yon);
    const bozucu = arizaEtkin ? -0.075 : 0;
    a.yonIntegral = sinirla(a.yonIntegral + yonHata * dt * 0.06 - (arizaEtkin ? 0 : a.yonIntegral * 0.02 * dt), -0.2, 0.2);
    if (arizaEtkin) a.yonIntegral += -bozucu * dt * 0.16;
    const rDot = 0.9 * yonHata + a.yonIntegral - 1.6 * a.r + bozucu + 0.004 * this.rnd.normal();
    a.r = sinirla(a.r + rDot * dt, -0.35, 0.35);
    a.yon = aciFarki(a.yon + a.r * dt, 0);

    // İleri hız ve yanal kayma
    const uDot = 0.6 * (a.hizHedef - a.u) - (arizaEtkin ? 0.03 : 0);
    a.u = sinirla(a.u + uDot * dt, -0.2, 0.9);
    const vDot = (arizaEtkin ? 0.05 - a.yonIntegral * 0.2 : 0) - 0.8 * a.v + 0.005 * this.rnd.normal();
    a.v = a.v + vDot * dt;

    // Derinlik (ikinci dereceden, hız sınırlı)
    const wDot = 0.35 * (a.derinlikHedef - a.d) - 1.1 * a.w + (arizaEtkin ? 0.01 : 0);
    a.w = sinirla(a.w + wDot * dt, -0.35, 0.3);
    a.d = Math.max(0.1, a.d + a.w * dt);

    // Yüzeyde dalga etkisi, derinde küçük salınım
    const dalga = Math.exp(-a.d / 0.6);
    a.yatis = 0.05 * dalga * Math.sin(2 * Math.PI * t / 3.1) + 0.004 * Math.sin(t * 0.9) - 0.15 * a.v;
    a.yunuslama = 0.03 * dalga * Math.sin(2 * Math.PI * t / 4.3 + 1) - 0.25 * a.w;
    a.p = (a.yatis - this.onceki.yatis) / dt;
    a.q = (a.yunuslama - this.onceki.yunuslama) / dt;

    // NED konum
    const c = Math.cos(a.yon);
    const s = Math.sin(a.yon);
    const vn = a.u * c - a.v * s;
    const ve = a.u * s + a.v * c;
    a.n += vn * dt;
    a.e += ve * dt;
    const onc = this.onceki;
    const vnOnc = onc.u * Math.cos(onc.yon) - onc.v * Math.sin(onc.yon);
    const veOnc = onc.u * Math.sin(onc.yon) + onc.v * Math.cos(onc.yon);
    a.an = [(vn - vnOnc) / dt, (ve - veOnc) / dt, (a.w - onc.w) / dt];

    // İtici karışımı (sekiz itici: 0–3 yatay vektörlü, 4–7 dikey); yaklaşık
    const fx = 0.9 * (a.hizHedef - a.u) + 0.35 * a.u;
    const fy = -0.6 * a.v;
    const mz = 1.1 * yonHata + a.yonIntegral * 4 - 0.6 * a.r;
    const fz = 0.8 * (a.derinlikHedef - a.d) - 0.5 * a.w + (a.d > 0.3 ? 0.08 : 0);
    const yatay = [fx - fy - mz, fx + fy + mz, -fx - fy + mz, -fx + fy - mz];
    const dikey = [fz, fz, fz, fz];
    const u = [...yatay, ...dikey].map((x, i) => {
      const k = arizaEtkin && i === ARIZA_ITICI ? 1 / ARIZA_CARPAN : 1;
      return a.silahli ? sinirla(x * 0.9 * k + 0.01 * this.rnd.normal(), -1, 1) : 0;
    });
    a.iticiU = u;

    // Batarya
    let toplamAkim = 0.9; // aviyonik yük (tahmin)
    for (const ui of u) toplamAkim += 17 * Math.pow(Math.abs(ui), 1.4);
    a.soc = Math.max(0, a.soc - (toplamAkim * dt) / (18 * 3600));
    this.paketAkimi = toplamAkim;

    // Kestirim sapması: suda DVL ile yavaş sürüklenme, yüzeyde GNSS ile sıfıra yaklaşır
    if (a.d < 0.3) {
      this.kestirimSapmasi.n *= 1 - 0.3 * dt;
      this.kestirimSapmasi.e *= 1 - 0.3 * dt;
    } else {
      const sur = 0.012 + (arizaEtkin ? 0.01 : 0);
      this.kestirimSapmasi.n += (sur * Math.abs(a.u) * 0.6 + 0.006 * this.rnd.normal()) * dt;
      this.kestirimSapmasi.e += (-sur * Math.abs(a.u) * 0.4 + 0.006 * this.rnd.normal()) * dt;
    }
    this.kestirimSapmasi.yon += (0.0005 * this.rnd.normal() - 0.02 * this.kestirimSapmasi.yon) * dt;
    // Kestirim gürültüsü süzgeç çıktısı gibi ilişkili olsun (AR(1); sahte)
    const ar = (x: number, sigma: number): number => 0.96 * x + sigma * 0.28 * this.rnd.normal();
    const g = this.kGurultu;
    g.n = ar(g.n, 0.04);
    g.e = ar(g.e, 0.04);
    g.d = ar(g.d, 0.008);
    g.vn = ar(g.vn, 0.01);
    g.ve = ar(g.ve, 0.01);

    // Kart geri okuması: yetkili durumu ~60 ms gecikmeyle izler; fırlatıcı arızası pim f2 okumasını bozar
    this.kartGoruGecmisi.push({ t: this.t_ns, acil: this.yetkili.acil, fis: [...this.yetkili.fis], pim: [...this.yetkili.pim] });
    while (this.kartGoruGecmisi.length > 1 && this.kartGoruGecmisi[1]!.t <= this.t_ns - 60_000_000) this.kartGoruGecmisi.shift();
    const gecikmis = this.kartGoruGecmisi[0]!;
    this.kartGoru = { acil: gecikmis.acil, fis: [...gecikmis.fis], pim: [...gecikmis.pim] };
    if (this.arizalar.some((f) => f.fault_id === 'firlatici_okuma')) this.kartGoru.pim[1] = 0;

    // Tanısal sayaçlar (SIL'de tanısal; seyrek gecikme)
    this.sayac.parti_gonderilen += this.kip === 'hil_1x' ? 2 : 0;
    if (this.rnd.duz() < 0.0006) {
      this.sayac.adim_son_tarih_ihlali++;
      this.sayac.azami_adim_gecikmesi_ns = Math.max(this.sayac.azami_adim_gecikmesi_ns, Math.round(200_000 + this.rnd.duz() * 500_000));
    }

    this.uret();
  }
  private yuzeydeBitis = 0;
  private paketAkimi = 0;
  private kGurultu = { n: 0, e: 0, d: 0, vn: 0, ve: 0 };

  private cizelge(t: number): void {
    const bir = (ad: string, an: number): boolean => {
      if (t < an || this.zamanCizelgesi.has(ad)) return false;
      this.zamanCizelgesi.add(ad);
      return true;
    };
    if (bir('itici_zayiflama', 72)) {
      const f: Ariza = {
        fault_id: 'itici_zayiflama',
        sinif: 1,
        hedef: 'itici_3',
        tur: 'itki_carpani',
        kaynak: 0,
        baslangic_adimi: this.adim_no,
        bitis_adimi: null,
      };
      this.arizalar.push(f);
      this.olay(21, 0, 3, { olay_kimligi: 'itici_zayiflama', tur: 'ariza', t_ns: this.t_ns, uygulama_adimi: this.adim_no, sonuc: 'uygulandi' }, this.adim_no);
      this.olay(6, 0, 0, { fault_id: f.fault_id, islem: 'uygulandi', kaynak: 'senaryo', uygulama_adimi: this.adim_no }, this.adim_no);
      this.arizaUret();
    }
    if (bir('fis_f1_tak', 100)) this.emniyetDegis('silahlandirma_fisi', 0, 1, 'fis_f1_tak');
    if (bir('fis_f1_cikar', 118)) this.emniyetDegis('silahlandirma_fisi', 0, 0, 'fis_f1_cikar');
    if (bir('firlatici_okuma', 125)) {
      const f: Ariza = {
        fault_id: 'firlatici_okuma',
        sinif: 0,
        hedef: 'firlatici',
        tur: 'ofset',
        kaynak: 0,
        baslangic_adimi: this.adim_no,
        bitis_adimi: this.adim_no + Math.round(15e9 / ADIM_NS),
      };
      this.arizalar.push(f);
      this.olay(21, 0, 3, { olay_kimligi: 'firlatici_okuma', tur: 'ariza', t_ns: this.t_ns, uygulama_adimi: this.adim_no, sonuc: 'uygulandi' }, this.adim_no);
      this.olay(6, 0, 0, { fault_id: f.fault_id, islem: 'uygulandi', kaynak: 'senaryo', uygulama_adimi: this.adim_no }, this.adim_no);
      this.arizaUret();
    }
    const biten = this.arizalar.filter((f) => f.bitis_adimi !== null && this.adim_no >= f.bitis_adimi);
    for (const f of biten) {
      this.arizalar = this.arizalar.filter((x) => x !== f);
      this.olay(6, 0, 0, { fault_id: f.fault_id, islem: 'bitti', kaynak: 'senaryo', uygulama_adimi: this.adim_no }, this.adim_no);
      this.arizaUret();
    }
  }

  private emniyetDegis(girdi: 'silahlandirma_fisi' | 'emniyet_pimi', tup: number, deger: number, kimlik: string): void {
    const dizi = girdi === 'silahlandirma_fisi' ? this.yetkili.fis : this.yetkili.pim;
    const onceki = dizi[tup]!;
    dizi[tup] = deger;
    // fiş takılması / pim çıkarılması emniyeti azaltır (A3-K89)
    const azaltan = girdi === 'silahlandirma_fisi' ? deger === 1 : deger === 0;
    this.emniyet_rev++;
    this.yetkili.sonAdim = this.adim_no;
    this.yetkili.sonKaynak = 1;
    const ad = (v: number): string => (v === 1 ? 'takili' : 'cikarilmis');
    this.olay(21, 0, 3, { olay_kimligi: kimlik, tur: 'emniyet_girdisi', t_ns: this.t_ns, uygulama_adimi: this.adim_no, sonuc: 'uygulandi' }, this.adim_no);
    this.olay(
      7,
      3,
      3,
      {
        girdi,
        tup: tup === 0 ? 'f1' : 'f2',
        onceki: ad(onceki),
        sonraki: ad(deger),
        kaynak: 'senaryo',
        yon: azaltan ? 'azaltan' : 'artiran',
        uygulama_adimi: this.adim_no,
        emniyet_rev: this.emniyet_rev,
      },
      this.adim_no,
    );
  }

  // ---- Yük üretimi ----

  private sira(anahtar: string): number {
    const v = (this.seq.get(anahtar) ?? 0) + 1;
    this.seq.set(anahtar, v);
    return v;
  }

  private koy<K extends KanalAdi>(kanal: K, yuk: KanalYuku[K], t_ns = this.t_ns, anahtar = ''): void {
    this.uretim.push({ kanal, t_ns, anahtar, yuk } as Uretim);
  }

  private olcumBasligi(sensor: keyof typeof SENSORLER, t: number, ek: Partial<{ window_ns: number; t_tx_ns: number | null; kalite: number | null }> = {}) {
    const id = SENSORLER[sensor];
    const arizaMaskesi = sensor === 'firlatici' && this.arizalar.some((f) => f.fault_id === 'firlatici_okuma') ? 1 : 0;
    return {
      sensor_id: id,
      inst: 0,
      seq: this.sira('s' + id),
      flags: 1,
      window_ns: ek.window_ns ?? 0,
      t_tx_ns: ek.t_tx_ns ?? null,
      t_hazir_ns: t + 500_000,
      t_teslim_ns: t + 1_000_000,
      parti_step_seq: this.kip === 'hil_1x' ? Math.floor(this.adim_no / 4) : null,
      fault_mask: arizaMaskesi,
      quality: ek.kalite ?? null,
      model_version: 1,
    };
  }

  private calistirmaDurumuUret(): void {
    const y: YukCalistirmaDurum = {
      durum: DURUM_KOD[this.durum],
      kip: this.kip === 'sil' ? 1 : 2,
      run_id: this.run_id,
      adim_no: this.adim_no,
      hedef_carpan: this.carpan,
      gerceklesen_carpan: this.durum === 'kosuyor' ? (this.carpan === 0 ? 8 : this.carpan) * (1 + 0.004 * this.rnd.normal()) : null,
      gecerli: 1,
      gercek_zamani_kacirdi: 0,
      gecerlilik_nedenleri: 0,
      tekrar_guvencesi: 2,
      kademe_gorunum: 1,
      kademe_kamera: 0,
      kademe_sonar: 1,
      kademe_kayit: 1,
      a1_oturum: this.kip === 'sil' ? 255 : this.durum === 'kosuyor' ? 3 : 0,
      son_komut_no: 0,
      durum_rev: this.durum_rev,
    };
    this.koy('calistirma/durum', y);
  }

  private arizaUret(): void {
    this.koy('ariza/etkin', {
      arizalar: this.arizalar.map((f) => ({ ...f })),
    });
  }

  private uret(): void {
    const k = Math.round(this.t_ns / DUNYA_ADIMI_NS); // dünya adım sayısı
    const a = this.a;
    const t = this.t_ns;
    const q = eulerdenKuaterniyon(a.yatis, a.yunuslama, a.yon);

    // fizik/durum: her adım (yer gerçeği)
    const fizik: YukFizikDurum = {
      adim_no: this.adim_no,
      govdeler: [
        {
          govde_id: 1,
          tur: 0,
          kutle_durumu: 0,
          p_n: [a.n, a.e, a.d],
          q_nb: q,
          nu: [a.u, a.v, a.w, a.p, a.q, a.r],
          a_n: a.an,
        },
        {
          govde_id: HEDEF.govde_id,
          tur: 2,
          kutle_durumu: 255,
          p_n: [HEDEF.n, HEDEF.e, HEDEF.d],
          q_nb: [1, 0, 0, 0],
          nu: [0, 0, 0, 0, 0, 0],
          a_n: [0, 0, 0],
        },
      ],
      iticiler: a.iticiU.map((u, i) => {
        const carpan = i === ARIZA_ITICI && this.arizalar.some((f) => f.fault_id === 'itici_zayiflama') ? ARIZA_CARPAN : 1;
        const itki = Math.sign(u) * ITICI_AZAMI_N * Math.pow(Math.abs(u), 1.15);
        return { u, itki_n: itki * carpan, akim_a: 17 * Math.pow(Math.abs(u), 1.4) };
      }),
      bara_gerilimi_v: 15.6 - 0.018 * this.paketAkimi - (1 - a.soc) * 2.2,
      paket_akimi_a: this.paketAkimi,
      soc: a.soc,
    };
    this.koy('fizik/durum', fizik);

    // kart/durum: 50 Hz, ölçüm zamanı bir dünya adımı önce, alım şimdi
    if (k % 2 === 0) this.koy('kart/durum', this.kartDurumu(this.onceki, t - DUNYA_ADIMI_NS));
    // yer_istasyonu/durum: 10 Hz, emülatör gecikmesi ~60 ms (sahte)
    if (k % 10 === 5) this.koy('yer_istasyonu/durum', this.kartDurumu(this.onceki, t - 60_000_000));

    if (k % 10 === 0) {
      this.calistirmaDurumuUret();
      this.koy('sayac/gercek_zaman', this.sayaclar());
      this.koy('atis/durum', this.atisDurumu());
      this.koy('emniyet/girdiler', this.emniyetGirdileri());
      this.koy('baglanti/durum', this.baglantiDurumu());
    } else if (this.yetkili.sonAdim === this.adim_no) {
      this.koy('emniyet/girdiler', this.emniyetGirdileri());
    }
    if (k % 50 === 0) {
      this.arizaUret();
      this.koy('kart/cekirdek_sayaclari', this.cekirdekSayaclari());
      if (this.kip === 'hil_1x') this.koy('kart/baglanti_sayaclari', this.baglantiSayaclari(), t, '');
      this.koy('surec/durum', {
        surecler: [
          { surec: 0, durum: 2, yeniden_baslatma: 0, son_kare_t_ns: null },
          { surec: 1, durum: 0, yeniden_baslatma: 0, son_kare_t_ns: null },
        ],
      });
      this.koy('kayit/durum', {
        acik: 1,
        kademe: 1,
        kamera_kareleri: 0,
        yazilan_bayt: Math.round(t / 1e9 * 180_000),
        kayit_dusen: 0,
        tekrar_guvencesi: 2,
        dosya: this.run_id + '.mcap',
      });
    }

    // Sensörler (yalnız panel örneklemesi; gerçek hızlar A5'tedir)
    if (k % 2 === 0) this.imu(t, q);
    if (k % 2 === 1) this.manyetometre(t);
    if (k % 4 === 0) this.basinc(t);
    if (k % 20 === 3) this.gnss(t);
    if (k % 7 === 0) this.dvl(t);
    if (k % 10 === 2) {
      this.koy('sensor/batarya', { ...this.olcumBasligi('batarya', t, { window_ns: 100_000_000 }), gerilim_v: fizik.bara_gerilimi_v + 0.01 * this.rnd.normal(), akim_a: this.paketAkimi + 0.1 * this.rnd.normal() }, t, `${SENSORLER.batarya}:0`);
      this.koy('sensor/sizinti', { ...this.olcumBasligi('sizinti', t), prob_maskesi: 0 }, t, `${SENSORLER.sizinti}:0`);
      this.firlatici(t);
    }
    if (k % 5 === 0) this.ping360(t);
    if (k % 5 === 2) this.ping1d(t);
  }

  private kartDurumu(o: Arac, olcumT: number): YukKartDurum {
    const sap = this.kestirimSapmasi;
    const g = this.kGurultu;
    const gecerli = olcumT > 1e9 ? 1 : 0;
    const yon = o.yon + sap.yon;
    const q = eulerdenKuaterniyon(o.yatis + 0.003, o.yunuslama - 0.002, yon);
    const c = Math.cos(o.yon);
    const s = Math.sin(o.yon);
    return {
      kart_t_ns: olcumT,
      acilis_sayaci: this.kartAcilis,
      kip: o.kip,
      silahli: o.silahli ? 1 : 0,
      saglik: 0b0000_0011,
      kestirim_gecerli: gecerli,
      p_n: gecerli ? [o.n + sap.n + g.n, o.e + sap.e + g.e, o.d + g.d] : [null, null, null],
      v_n: gecerli ? [o.u * c - o.v * s + g.vn, o.u * s + o.v * c + g.ve, o.w] : [null, null, null],
      q_nb: gecerli ? q : [null, null, null, null],
      derinlik_m: o.d + g.d,
      batarya_soc: Math.max(0, o.soc - 0.015),
      adim_suresi_us: Math.round(190 + 25 * this.rnd.duz()),
    };
  }

  private engelMaskesi(): number {
    const a = this.a;
    let m = 0;
    if (!a.silahli) m |= 1 << 0; // NOT_ARMED
    m |= 1 << 1; // MODE: sahte sunucu kipi izin kümesinde saymaz
    if (this.t_ns < 1e9) m |= 1 << 2; // EST
    if (this.kartGoru.acil === 1) m |= 1 << 4;
    for (let i = 0; i < 2; i++) {
      if (this.kartGoru.fis[i] === 0) m |= 1 << (5 + i); // PLUG_Ti: çıkık
      if (this.kartGoru.pim[i] === 1) m |= 1 << (7 + i); // PIN_Ti: takılı
    }
    m |= 1 << 17; // DET_STALE: kamera kapalı
    return m >>> 0;
  }

  private atisDurumu(): YukAtisDurum {
    if (this.emniyetKipi === 'bilinmiyor') {
      return {
        fc_durumu: null,
        engel_maskesi: null,
        kart_son_olay_kimligi: null,
        tupler: [0, 1].map((i) => ({ tup: i, dolu: 1, sureli_izin_gecerli: 0, izin_kalan_ns: null, son_olay_sonucu: 0 })),
      };
    }
    if (this.emniyetKipi === 'gecersiz') {
      return {
        fc_durumu: 9,
        engel_maskesi: this.engelMaskesi(),
        kart_son_olay_kimligi: null,
        tupler: [
          { tup: 0, dolu: 2, sureli_izin_gecerli: 0, izin_kalan_ns: null, son_olay_sonucu: 0 },
          { tup: 1, dolu: 1, sureli_izin_gecerli: 7, izin_kalan_ns: null, son_olay_sonucu: 12 },
        ],
      };
    }
    return {
      fc_durumu: 0,
      engel_maskesi: this.engelMaskesi(),
      kart_son_olay_kimligi: null,
      tupler: [0, 1].map((i) => ({
        tup: i,
        dolu: 1,
        sureli_izin_gecerli: 0,
        izin_kalan_ns: null,
        son_olay_sonucu: 0,
      })),
    };
  }

  private emniyetGirdileri(): YukEmniyetGirdiler {
    const y = this.yetkili;
    const kg = this.kartGoru;
    let fark = 0;
    if (y.acil !== kg.acil) fark |= 1;
    for (let i = 0; i < 2; i++) {
      if (y.fis[i] !== kg.fis[i]) fark |= 1 << (1 + i);
      if (y.pim[i] !== kg.pim[i]) fark |= 1 << (3 + i);
    }
    const olagan: YukEmniyetGirdiler = {
      acil_durdurma: y.acil,
      silahlandirma_fisi: [...y.fis],
      emniyet_pimi: [...y.pim],
      son_degisim_adimi: y.sonAdim,
      son_degisim_kaynagi: y.sonKaynak,
      kart_acil_durdurma: kg.acil,
      kart_silahlandirma_fisi: [...kg.fis],
      kart_emniyet_pimi: [...kg.pim],
      geri_okuma_farki: fark,
      emniyet_rev: this.emniyet_rev,
      emniyet_rev_doydu: 0,
    };
    if (this.emniyetKipi === 'bilinmiyor') {
      // kart IDA_FC_STATUS bildirmedi: kart_* = 255, fark bitleri 0 (A3 §3.4)
      return { ...olagan, kart_acil_durdurma: null, kart_silahlandirma_fisi: [null, null], kart_emniyet_pimi: [null, null], geri_okuma_farki: 0 };
    }
    if (this.emniyetKipi === 'gecersiz') {
      // değer listesi dışı değerler (bozulmuş tel verisi benzetimi; A3-K24)
      return { ...olagan, acil_durdurma: 2, kart_emniyet_pimi: [kg.pim[0] ?? 1, 3], son_degisim_kaynagi: 9 };
    }
    return olagan;
  }

  private sayaclar(): YukSayacGercekZaman {
    const s = this.sayac;
    return {
      parti_gonderilen: s.parti_gonderilen,
      parti_kacirildi: 0,
      parti_kacirildi_birlesik: 0,
      bariyer_bekleme: 0,
      gercek_zaman_kaymasi_ns: 0,
      gercek_zaman_kaymasi_en_buyuk_ns: 0,
      tf_gec_yazim_ns: this.kip === 'hil_1x' ? 180_000 : 0,
      yanit_ret_seq_disi: 0,
      yanit_ret_yinelenen: 0,
      yanit_ret_gec: 0,
      yanit_ret_oturum: 0,
      yanit_iptal_esz: 0,
      esz_guvenli_uygulanan: 0,
      komut_suresi_doldu: 0,
      olay_alinan: 0,
      olay_gec: 0,
      olay_eksik: 0,
      olay_sonuc: [0, 0, 0, 0, 0, 0],
      komut_birlestirildi: 0,
      komut_ertelendi: 0,
      komut_sonucsuz: 0,
      ping360_seyreltildi: 0,
      ping360_gecikme_en_buyuk: this.kip === 'hil_1x' ? 1 : 0,
      pencere_tasma: 0,
      rx_crc_err: 0,
      rx_resync_bytes: 0,
      rx_unknown_msgid: 0,
      rx_bad_len: 0,
      rx_wrong_sys: 0,
      rx_iflag: 0,
      rx_mav1: 0,
      rx_seq_gap: 0,
      oturum_kurulan: this.kip === 'hil_1x' ? this.calistirmaNo : 0,
      oturum_kopma: this.kip === 'hil_1x' ? this.calistirmaNo - 1 : 0,
      beklenmeyen_sifirlama: 0,
      calistirma_gecersiz: 0,
      neden_bitleri: 0,
      son_tarih_kacirildi: 0,
      sim_gecikmesi: 0,
      yanit_ret_eski_uretim: 0,
      olay_rev_ret: 0,
      sonuc_kuyrugu_en_buyuk: 0,
      kritik_erteleme: 0,
      kritik_erteleme_en_buyuk: 0,
      cagri_izi_eksik: 0,
      yapilandirma_ret: 0,
      cagri_cakismasi: 0,
      sonuc_talebi_atildi: 0,
      komut_kovasi_bekleme: 0,
      emniyet_rev_doydu: 0,
      mesaj_retleri: [],
      adim_son_tarih_ihlali: s.adim_son_tarih_ihlali,
      kacirilan_adim: 0,
      azami_adim_gecikmesi_ns: s.azami_adim_gecikmesi_ns,
      sonar_gec: 0,
      sonar_kuyruk_dolu: 0,
      sensor_kuyruk_tasma: 0,
      donma_onceki_cikti_yok: 0,
      atama_celiskisi: 0,
      gevsetme_reddedildi: 0,
      bayat_yetki: 0,
      komut_bayat_yetki: 0,
      a3_halka_tasma: 0,
      kayit_dusen: 0,
    };
  }

  private baglantiDurumu(): KanalYuku['baglanti/durum'] {
    const hil = this.kip === 'hil_1x';
    return {
      a1_oturum: hil ? (this.durum === 'kosuyor' ? 3 : 0) : 255,
      sess: hil ? this.calistirmaNo : 0,
      acilis_sayaci: hil ? this.kartAcilis : 0,
      d_asagi_ort_ns: hil ? 1_400_000 : null,
      emulator: [
        { yon: 0, kuyruk_bayt: 0, dusen: 0, model_tutulamadi: 0 },
        { yon: 1, kuyruk_bayt: Math.round(40 + 30 * this.rnd.duz()), dusen: 0, model_tutulamadi: 0 },
      ],
      // Bağlantıya özgü alanları sunucu gönderirken doldurur.
      bu_baglanti_ezilen_durum: 0,
      bu_baglanti_dusen_dizi: 0,
      bu_baglanti_olay_kuyrugu: 0,
    };
  }

  private cekirdekSayaclari(): KanalYuku['kart/cekirdek_sayaclari'] {
    return {
      acilis_sayaci: this.kartAcilis,
      sample_rejected: 0,
      sample_future: 0,
      sample_conflict: 0,
      sample_dup_dropped: 0,
      narrow_from_rejected: 0,
      bg_bad_input: 0,
      imu_rejected: 0,
      imu_gap: 0,
      bad_input: 0,
      dup_seq: 0,
      old_seq: 0,
      wall_backstep: 0,
      cmd_rejected: 0,
      widen_blocked: 0,
      evres_ignored: 0,
      event_unknown: 0,
      det_rejected: 0,
      ping360_rejected: 0,
      bg_dropped: 0,
      bg_slice_mismatch: 0,
      log_dropped: 0,
      step_miss: 0,
      saglik: 0b0000_0011,
    };
  }

  private baglantiSayaclari(): KanalYuku['kart/baglanti_sayaclari'] {
    const n = Math.floor(this.t_ns / 5_000_000);
    const z = 0;
    return {
      acilis_sayaci: this.kartAcilis,
      rx_bytes: (n * 610) >>> 0,
      rx_frames: (n * 9) >>> 0,
      rx_crc_err: z,
      rx_unknown_msgid: z,
      rx_iflag: z,
      rx_bad_len: z,
      rx_wrong_sys: z,
      rx_mav1: z,
      rx_overflow: z,
      rx_resync_bytes: z,
      rx_seq_gap: z,
      rx_stale_session: z,
      rx_outside_session: z,
      stamp_coarse: z,
      batch_closed: n,
      batch_incomplete: z,
      batch_skipped: z,
      batch_old: z,
      batch_dup: z,
      batch_merged: z,
      member_overflow: z,
      ping360_incomplete: z,
      ping360_bad: z,
      ping360_orphan: z,
      cmd_deferred: z,
      cmd_dropped: z,
      narrow_coalesced: z,
      cmd_bad_field: z,
      grid_missed: z,
      out_fixed: z,
      tx_frames: (n * 3) >>> 0,
      tx_dropped: z,
      tx_crit_stall: z,
      events_sent: z,
      events_resent: z,
      sessions: this.calistirmaNo,
    };
  }

  private imu(t: number, q: [number, number, number, number]): void {
    void q;
    const a = this.a;
    const g = 9.80665;
    this.koy(
      'sensor/imu',
      {
        ...this.olcumBasligi('imu', t, { window_ns: 5_000_000 }),
        ivme_mps2: [
          a.an[0] * Math.cos(a.yon) + a.an[1] * Math.sin(a.yon) + g * Math.sin(a.yunuslama) + 0.02 * this.rnd.normal(),
          -g * Math.sin(a.yatis) + 0.02 * this.rnd.normal(),
          -g * Math.cos(a.yatis) * Math.cos(a.yunuslama) + a.an[2] + 0.02 * this.rnd.normal(),
        ],
        acisal_hiz_radps: [a.p + 0.002 * this.rnd.normal(), a.q + 0.002 * this.rnd.normal(), a.r + 0.002 * this.rnd.normal()],
        sicaklik_c: 24.5,
      },
      t,
      `${SENSORLER.imu}:0`,
    );
  }

  private manyetometre(t: number): void {
    const a = this.a;
    const b = 2.0e-5;
    this.koy(
      'sensor/manyetometre',
      {
        ...this.olcumBasligi('manyetometre', t, { window_ns: 10_000_000 }),
        alan_t: [b * Math.cos(a.yon) + 2e-8 * this.rnd.normal(), -b * Math.sin(a.yon) + 2e-8 * this.rnd.normal(), 4.0e-5],
        sicaklik_c: 24.0,
      },
      t,
      `${SENSORLER.manyetometre}:0`,
    );
  }

  private basinc(t: number): void {
    const d = this.a.d;
    this.koy(
      'sensor/basinc',
      {
        ...this.olcumBasligi('basinc', t, { window_ns: 20_000_000 }),
        basinc_pa: 101325 + 1000 * 9.80665 * (d + 0.004 * this.rnd.normal()),
        sicaklik_c: 20.0,
      },
      t,
      `${SENSORLER.basinc}:0`,
    );
  }

  private gnss(t: number): void {
    const a = this.a;
    const yuzeyde = a.d < 0.3;
    const enlem0 = 0.7155849933176751;
    const boylam0 = 0.5061454830783556;
    const R = 6378137;
    this.koy(
      'sensor/gnss',
      {
        ...this.olcumBasligi('gnss', t),
        flags: yuzeyde ? 1 : 0,
        enlem_rad: enlem0 + (a.n + 0.6 * this.rnd.normal()) / R,
        boylam_rad: boylam0 + (a.e + 0.6 * this.rnd.normal()) / (R * Math.cos(enlem0)),
        yukseklik_m: -a.d,
        hiz_ned_mps: [a.u * Math.cos(a.yon), a.u * Math.sin(a.yon), 0],
        eph_m: yuzeyde ? 1.2 : 99,
        epv_m: yuzeyde ? 2.0 : 99,
        fix_turu: yuzeyde ? 3 : 0,
        uydu_sayisi: yuzeyde ? 11 : 0,
      },
      t,
      `${SENSORLER.gnss}:0`,
    );
  }

  private dvl(t: number): void {
    const a = this.a;
    const irtifa = SU_DERINLIGI_M - a.d;
    const gecerli = irtifa > 0.3 && a.d > 0.4;
    this.koy(
      'sensor/dvl',
      {
        ...this.olcumBasligi('dvl', t, { window_ns: 66_000_000 }),
        flags: gecerli ? 1 : 0,
        hiz_sensor_mps: gecerli ? [a.u + 0.005 * this.rnd.normal(), a.v + 0.005 * this.rnd.normal(), a.w] : [null, null, null],
        irtifa_m: gecerli ? irtifa : null,
        fom_mps: gecerli ? 0.004 : null,
        isin_gecerli: gecerli ? 0x0f : 0,
      },
      t,
      `${SENSORLER.dvl}:0`,
    );
  }

  private firlatici(t: number): void {
    const kg = this.kartGoru;
    // tüp bitleri: bit0 LOADED, bit1 PLUG_IN, bit2 PIN_OUT, bit3 PERMIT_OPEN
    const bit = (i: number): number => 1 | ((kg.fis[i] === 1 ? 1 : 0) << 1) | ((kg.pim[i] === 0 ? 1 : 0) << 2);
    this.koy(
      'sensor/firlatici',
      { ...this.olcumBasligi('firlatici', t), acil_durdurma: kg.acil, tup_bitleri: [bit(0), bit(1)] },
      t,
      `${SENSORLER.firlatici}:0`,
    );
  }

  /** Işının havuz duvarına uzaklığı (yatay düzlem). */
  private duvarMesafesi(n: number, e: number, yon: number): number {
    const dn = Math.cos(yon);
    const de = Math.sin(yon);
    let enAz = Infinity;
    if (dn > 1e-6) enAz = Math.min(enAz, (HAVUZ.kuzeyMax - n) / dn);
    if (dn < -1e-6) enAz = Math.min(enAz, (HAVUZ.kuzeyMin - n) / dn);
    if (de > 1e-6) enAz = Math.min(enAz, (HAVUZ.doguMax - e) / de);
    if (de < -1e-6) enAz = Math.min(enAz, (HAVUZ.doguMin - e) / de);
    return enAz;
  }

  private ping360(t: number): void {
    const a = this.a;
    const aci = this.ping360Aci;
    this.ping360Aci = (this.ping360Aci + P360_ADIM_GRAD) % 400;
    const teta = (2 * Math.PI * aci) / 400; // sensör +x'ten (ileri), sağa doğru
    const yon = a.yon + teta;
    const irtifa = SU_DERINLIGI_M - a.d;
    const isinYariGenislik = (12.5 * Math.PI) / 180;
    const dipBaslangic = irtifa / Math.sin(isinYariGenislik);
    const duvar = this.duvarMesafesi(a.n, a.e, yon);
    const dn = Math.cos(yon);
    const de = Math.sin(yon);
    // hedef
    const hn = HEDEF.n - a.n;
    const he = HEDEF.e - a.e;
    const hedefR = Math.hypot(hn, he);
    const hedefAci = Math.abs(aciFarki(Math.atan2(he, hn), yon));
    const hedefGorunur = hedefAci < (1.6 * Math.PI) / 180 + HEDEF.yaricap / Math.max(hedefR, 0.5) && Math.abs(HEDEF.d - a.d) < 2.5;
    const ornekler = new Uint8Array(P360_N);
    const dr = P360_MENZIL / P360_N;
    for (let i = 0; i < P360_N; i++) {
      const r = (i + 0.5) * dr;
      let v = 6 + 8 * this.rnd.duz();
      if (r < 0.7) v += 190 * Math.exp(-r / 0.25);
      if (r > dipBaslangic * 0.92 && r < duvar) {
        const giris = sinirla((r - dipBaslangic * 0.92) / (dipBaslangic * 0.18), 0, 1);
        let dip = 70 * giris * Math.exp(-(r - dipBaslangic) / 16);
        // kayalar
        const pn = a.n + dn * r;
        const pe = a.e + de * r;
        for (const k of KAYALAR) {
          const m = Math.hypot(pn - k.n, pe - k.e);
          if (m < k.r) dip += 150 * (1 - m / k.r);
        }
        if (hedefGorunur && r > hedefR + HEDEF.yaricap) dip *= 0.25; // akustik gölge
        v += dip * this.rnd.benek();
      }
      if (Math.abs(r - duvar) < 0.18) v += 210 * this.rnd.benek();
      else if (r > duvar && r < duvar * 1.04) v += 40 * this.rnd.benek();
      else if (r > duvar) v *= 0.4;
      if (hedefGorunur && Math.abs(r - hedefR) < HEDEF.yaricap) v += 230 * this.rnd.benek();
      ornekler[i] = sinirla(Math.round(v), 0, 255);
    }
    const tTx = t;
    this.koy(
      'sonar/ping360',
      {
        ...this.olcumBasligi('ping360', tTx, { t_tx_ns: tTx }),
        t_hazir_ns: tTx + Math.round(((2 * P360_MENZIL) / SES_HIZI) * 1e9),
        t_teslim_ns: tTx + Math.round(((2 * P360_MENZIL) / SES_HIZI) * 1e9) + 2_000_000,
        aci_grad: aci,
        n_ornek: P360_N,
        ornek_periyodu: P360_PERIYOT,
        darbe_suresi_us: 80,
        frekans_khz: 750,
        kazanc: 1,
        kademe: 1,
        ses_hizi_mps: SES_HIZI,
        ornekler,
      },
      tTx,
      `${SENSORLER.ping360}:0`,
    );
  }

  private ping1d(t: number): void {
    const a = this.a;
    const irtifa = SU_DERINLIGI_M - a.d + 0.08 * Math.sin(a.n * 0.7) * Math.cos(a.e * 0.5);
    const pencere = Math.max(1, irtifa * 1.5); // oto menzil çarpanı 1,5 (A5 profil varsayılanı)
    const profil = new Uint8Array(P1D_N);
    for (let i = 0; i < P1D_N; i++) {
      const r = ((i + 0.5) / P1D_N) * pencere;
      let v = 4 + 10 * this.rnd.duz();
      if (r < 0.4) v += 160 * Math.exp(-r / 0.12);
      const fark = r - irtifa;
      if (fark > -0.15) v += 235 * Math.exp(-Math.max(0, fark) / 0.35) * Math.exp(-((Math.min(0, fark) / 0.08) ** 2));
      profil[i] = sinirla(Math.round(v * (0.85 + 0.3 * this.rnd.duz())), 0, 255);
    }
    const tTx = t - Math.round((irtifa / SES_HIZI) * 1e9);
    this.koy(
      'sonar/ping1d',
      {
        ...this.olcumBasligi('ping1d', tTx, { t_tx_ns: tTx }),
        mesafe_m: irtifa + 0.01 * this.rnd.normal(),
        guven: a.d > 0.3 ? 0.97 : 0.6,
        kademe: 1,
        profil,
      },
      tTx,
      `${SENSORLER.ping1d}:0`,
    );
  }
}

export function ONDENETIM_MADDELERI(kip: Kip): { kod: string; durum: 'gecti' | 'atlandi'; olculen: number | string | null; sinir: number | string | null; birim?: string; ileti: string; dayanak: string }[] {
  const hil = kip === 'hil_1x';
  return [
    { kod: 'senaryo_gecerli', durum: 'gecti', olculen: null, sinir: null, ileti: 'sahte senaryo (A5 doğrulayıcısı çalışmadı)', dayanak: 'A5' },
    { kod: 'kontrol_hizi', durum: 'gecti', olculen: 200, sinir: 200, birim: 'Hz', ileti: 'kontrol hızı 200 Hz', dayanak: 'iskelet §4' },
    { kod: 'surum_uyumu', durum: hil ? 'gecti' : 'atlandi', olculen: null, sinir: null, ileti: hil ? 'sahte el sıkışma' : 'SIL: A1 el sıkışması yok', dayanak: 'A1' },
    { kod: 'a1_bant_payi', durum: hil ? 'gecti' : 'atlandi', olculen: hil ? 288 : null, sinir: 900, birim: 'B', ileti: 'sahte değer', dayanak: 'iskelet §3' },
    { kod: 'ping360_zamanlama', durum: 'gecti', olculen: null, sinir: null, ileti: 'sahte', dayanak: '05 §3' },
    { kod: 'algilama_bileseni', durum: 'atlandi', olculen: null, sinir: null, ileti: 'kamera kapalı', dayanak: 'A8' },
    { kod: 'kayit_disk_alani', durum: 'gecti', olculen: 'sahte', sinir: 1, birim: 'GiB', ileti: 'sahte değer', dayanak: 'A3 §8 md.6' },
  ];
}

function hex(r: Rastgele, n: number): string {
  let s = '';
  for (let i = 0; i < n; i++) s += Math.floor(r.duz() * 16).toString(16);
  return s;
}

function runIdUret(r: Rastgele): string {
  const d = new Date();
  const p = (v: number, n = 2): string => String(v).padStart(n, '0');
  return (
    `${d.getUTCFullYear()}${p(d.getUTCMonth() + 1)}${p(d.getUTCDate())}-` +
    `${p(d.getUTCHours())}${p(d.getUTCMinutes())}${p(d.getUTCSeconds())}-${hex(r, 6)}`
  );
}
