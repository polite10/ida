// Emniyet girdisi ve yer istasyonu isteklerinin TEK modülü (W5-web-emniyet). DOM yoktur.
//
// Emniyet kararını sunucu verir (A3 §4.11, §4.15); bu modül yalnız istek kurar, sunucunun
// bildirdiğini kaydeder ve hiçbir şeyi kendisi "uygulanmış" saymaz.
//
//  - W5-K51: gösterilen emniyet durumu yalnız `emniyet/girdiler` çerçevesinden gelir; bu modül
//    o durumu değiştirmez, tahmin etmez. Gitmeyen istek `UYGULANMADI`, sonucu bilinmeyen istek
//    `SONUÇ BİLİNMİYOR` diye kayda geçer (bkz. `sonucMetni`).
//  - W5-K52: emniyeti AZALTAN (gevşeten) istek hiçbir koşulda kendiliğinden yinelenmez: jeton
//    tek kullanımlıktır, süresi dolarsa / revizyon değişirse / bağlantı kopar ya da istek
//    reddedilirse jeton düşer ve kullanıcı iki adımı baştan yapar. Emniyeti ARTIRAN istek
//    revizyon bağı taşımaz ve revizyon nedeniyle reddedilmez (A3-K92, K119); bu yüzden
//    "revizyon çakışmasında yeniden gönderme" gerekmez ve yazılmamıştır; hiçbir istek
//    kendiliğinden yinelenmez.
//  - W5-K53: kısayol, toplu eylem ve önceden işaretli onay yoktur; jeton yalnız bellekte durur.
//  - W5-K54: atış tetikleyen istek yoktur; yer istasyonu listesi sunucudan gelir, istemci
//    genişletmez; atış isteği komutları listeden çıkarılır (yalnız daraltma).

import type { A3Baglanti } from './baglanti';
import { hataIletisi } from './hata';
import { A3Hatasi } from './http';
import type { KomutKaydi, KomutYoneticisi } from './komut';

export type GirdiAdi = 'acil_durdurma' | 'silahlandirma_fisi' | 'emniyet_pimi';
export type Tup = 'f1' | 'f2';
export type HedefDurum = 'basili' | 'serbest' | 'takili' | 'cikarilmis';

export interface Secim {
  girdi: GirdiAdi;
  /** Fiş ve pimde zorunlu, acil durdurmada yasak (A3-K89). */
  tup?: Tup;
  hedef: HedefDurum;
}

/** Jetonun sunucu ömründen bu kadar önce istemci tarafında düşer (tahmin: saat ve iletim payı). */
export const JETON_PAYI_MS = 1000;

/**
 * Atış tetikleyen komutlar yer istasyonu formunda gösterilmez (W5-K54). Ad ve A1 eşlemesi
 * (`a1.tur`, ör. `IDAW_FC_OP_FIRE`) birlikte denetlenir; biri atış isteğine benziyorsa gizlenir
 * (en kısıtlayan okuma: yanlış gizlemek yanlış göstermekten güvenlidir).
 */
export function atisKomutuMu(k: { ad: string; a1?: unknown }): boolean {
  return /fire|ates|atis/i.test(k.ad) || /fire/i.test(JSON.stringify(k.a1 ?? null));
}

export function secimHatasi(s: Secim): string | null {
  if (s.girdi === 'acil_durdurma') {
    if (s.tup !== undefined) return 'Acil durdurma tüp almaz.';
    return s.hedef === 'basili' || s.hedef === 'serbest' ? null : 'Acil durdurma hedefi basili ya da serbest olmalı.';
  }
  if (s.tup !== 'f1' && s.tup !== 'f2') return 'Fiş ve pim için tüp (f1 ya da f2) zorunlu.';
  return s.hedef === 'takili' || s.hedef === 'cikarilmis' ? null : 'Fiş ve pim hedefi takili ya da cikarilmis olmalı.';
}

/** A3-K89: artıran = acil durdurma basılı, fiş çıkarılmış, pim takılı. */
export function yonu(s: Secim): 'artiran' | 'azaltan' {
  if (s.girdi === 'acil_durdurma') return s.hedef === 'basili' ? 'artiran' : 'azaltan';
  if (s.girdi === 'silahlandirma_fisi') return s.hedef === 'cikarilmis' ? 'artiran' : 'azaltan';
  return s.hedef === 'takili' ? 'artiran' : 'azaltan';
}

const GIRDI_AD: Record<GirdiAdi, string> = { acil_durdurma: 'Acil durdurma', silahlandirma_fisi: 'Silahlandırma fişi', emniyet_pimi: 'Emniyet pimi' };
const HEDEF_AD: Record<HedefDurum, string> = { basili: 'BASILI', serbest: 'serbest', takili: 'takılı', cikarilmis: 'çıkarılmış' };

export function secimOzeti(s: Secim): string {
  return `${GIRDI_AD[s.girdi]}${s.tup !== undefined ? ` · ${s.tup}` : ''} → ${HEDEF_AD[s.hedef]} (${yonu(s) === 'artiran' ? 'emniyeti artıran' : 'emniyeti azaltan'})`;
}

export interface OnayJetonu {
  tur: 'emniyet' | 'yer_istasyonu';
  jeton: string;
  run_id: string | null;
  emniyet_rev: number;
  /** İstemcinin monotonik saatiyle isteğin başladığı an (ms). */
  alindi: number;
  gecerlilik_ms: number;
  ozet: string;
  secim?: Secim;
  komut?: string;
  parametreler?: Record<string, unknown>;
}

export interface KomutTanimi {
  ad: string;
  /** A1 eşlemesi (katalogdan olduğu gibi); yalnız atış isteği komutunu ayırmak için okunur. */
  a1?: unknown;
  sinif: 'daraltici' | 'genisletici' | 'notr';
  onay: boolean;
  parametreler: Record<string, unknown>;
  sonuc: Record<string, unknown> | null;
  bilesik: boolean;
}

export type JetonSonucu = { jeton: OnayJetonu } | { hata: { kod: string; ileti: string } };

/** Ekranda gösterilecek sonuç (W5-K51): üç ayrı durum, hepsi açık yazılı. */
export interface SonucMetni {
  baslik: 'GÖNDERİLİYOR' | 'KUYRUKTA — henüz uygulandığı bildirilmedi' | 'UYGULANDI' | 'UYGULANMADI' | 'SONUÇ BİLİNMİYOR';
  sinif: 'vurgu' | 'uyari' | 'iyi' | 'kotu';
  ayrinti: string;
}

export function sonucMetni(k: KomutKaydi): SonucMetni {
  switch (k.asama) {
    case 'gonderiliyor':
      return { baslik: 'GÖNDERİLİYOR', sinif: 'vurgu', ayrinti: 'yanıt bekleniyor; gösterge değişmedi' };
    case 'kuyrukta':
      return { baslik: 'KUYRUKTA — henüz uygulandığı bildirilmedi', sinif: 'uyari', ayrinti: 'sunucu isteği kabul etti; uygulandığını bildirene kadar gösterge değişmez' };
    case 'uygulandi':
      return { baslik: 'UYGULANDI', sinif: 'iyi', ayrinti: `sunucu bildirdi${k.uygulama_adimi !== null ? ` (adım ${k.uygulama_adimi})` : ''}` };
    case 'reddedildi':
      return { baslik: 'UYGULANMADI', sinif: 'kotu', ayrinti: `${k.gonderildi_mi ? 'sunucu reddetti' : 'istek gönderilmedi'}: ${k.ileti}` };
    default:
      return { baslik: 'SONUÇ BİLİNMİYOR', sinif: 'uyari', ayrinti: `${k.ileti} İstek uygulanmış olabilir; durumu yalnız sunucunun bildirdiği gösterge belirler.` };
  }
}

function kanonik(v: unknown): string {
  if (Array.isArray(v)) return `[${v.map(kanonik).join(',')}]`;
  if (v !== null && typeof v === 'object') {
    const o = v as Record<string, unknown>;
    return `{${Object.keys(o)
      .sort()
      .map((k) => `${JSON.stringify(k)}:${kanonik(o[k])}`)
      .join(',')}}`;
  }
  return JSON.stringify(v);
}

interface JetonYaniti {
  onay_jetonu?: unknown;
  gecerlilik_ms?: unknown;
  run_id?: unknown;
  emniyet_rev?: unknown;
}

export class EmniyetIstekleri {
  private jeton_: OnayJetonu | null = null;
  private jetonNedeni_ = '';
  /** Bekleyen jeton yanıtları kopma, vazgeçme ve yeni istekten sonra kullanılamaz. */
  private jetonNesli = 0;
  private katalog_: KomutTanimi[] | null = null;
  private readonly dinleyiciler = new Set<() => void>();
  private readonly kaldir: (() => void)[] = [];

  constructor(
    private readonly baglanti: A3Baglanti,
    private readonly komutlar: KomutYoneticisi,
    private readonly simdi: () => number = () => performance.now(),
  ) {
    // Bağlantı koparsa ya da emniyet revizyonu jetonun revizyonunu aşarsa jeton düşer (W5-K52).
    this.kaldir.push(
      baglanti.durumDinle((b) => {
        if (b.durum !== 'bagli') this.jetonuDusur('Bağlantı koptu; onay jetonu düştü.');
      }),
    );
    this.kaldir.push(
      baglanti.abone(
        'emniyet/girdiler',
        (c) => {
          const j = this.jeton_;
          if (j !== null && Number(c.yuk.emniyet_rev) > j.emniyet_rev) this.jetonuDusur('Emniyet revizyonu değişti; onay jetonu düştü.');
        },
        10,
      ),
    );
  }

  kapat(): void {
    this.jetonNesli++;
    for (const k of this.kaldir) k();
    this.kaldir.length = 0;
    this.jeton_ = null;
  }

  dinle(f: () => void): () => void {
    this.dinleyiciler.add(f);
    return () => this.dinleyiciler.delete(f);
  }

  private bildir(): void {
    for (const f of this.dinleyiciler) {
      try {
        f();
      } catch (h) {
        console.error(h);
      }
    }
  }

  private jetonuDusur(neden: string): void {
    this.jetonNesli++;
    const vardi = this.jeton_ !== null;
    this.jeton_ = null;
    this.jetonNedeni_ = neden;
    if (vardi) this.bildir();
  }

  /** Elde tutulan onay jetonu; süresi dolduysa düşürülür ve null döner. */
  jeton(): OnayJetonu | null {
    const j = this.jeton_;
    if (j !== null && this.simdi() - j.alindi >= j.gecerlilik_ms - JETON_PAYI_MS) this.jetonuDusur('Onay jetonunun süresi doldu; iki adımı baştan yapın.');
    return this.jeton_;
  }

  /** Jeton düşünce nedeni (ekranda gösterilir); jeton varken boş. */
  jetonNedeni(): string {
    return this.jeton_ === null ? this.jetonNedeni_ : '';
  }

  /** Jetonun istemci tarafında kalan süresi (ms); jeton yoksa 0. */
  kalanMs(): number {
    const j = this.jeton();
    return j === null ? 0 : Math.max(0, j.gecerlilik_ms - JETON_PAYI_MS - (this.simdi() - j.alindi));
  }

  vazgec(): void {
    this.jetonuDusur('Vazgeçildi; onay jetonu düştü.');
  }

  private runId(): string | null {
    const c = this.baglanti.son('calistirma/durum');
    return c === undefined || c.yuk.run_id === '' ? null : c.yuk.run_id;
  }

  // ---------------------------------------------------------------- emniyet girdisi

  /** Emniyeti ARTIRAN değişiklik (tek adım): onay, jeton ve revizyon bağı yoktur (A3-K92, K119). */
  artir(s: Secim): Promise<KomutKaydi> | KomutKaydi {
    const h = secimHatasi(s);
    if (h !== null) return this.komutlar.engelle('emniyet_degistir', 'gecersiz_istek', h, secimOzeti(s));
    if (yonu(s) !== 'artiran') return this.komutlar.engelle('emniyet_degistir', 'yon_yanlis', 'Bu yön emniyeti azaltır; iki adımlı gevşetme gerekir.', secimOzeti(s));
    if (this.baglanti.bilgi.durum !== 'bagli') {
      return this.komutlar.engelle('emniyet_degistir', 'sunucuya_ulasilamadi', 'Bağlantı yok; istek gönderilmedi. Bağlantı kurulunca yeniden basın.', secimOzeti(s));
    }
    const govde: Record<string, unknown> = { beklenen_run_id: this.runId(), girdi: s.girdi, hedef_durum: s.hedef };
    if (s.tup !== undefined) govde.tup = s.tup;
    // Gönderime başlandıktan sonraki her taşıma hatasında sonuç bilinmiyor;
    // WebSocket'in kopuk olması HTTP isteğinin ulaşmadığını kanıtlamaz (W5-K51).
    return this.komutlar.gonderGenel('emniyet_degistir', () => this.baglanti.http.komut<{ komut_no: number }>('/emniyet/degistir', govde), {
      ozet: secimOzeti(s),
    });
  }

  /** Gevşetme adım 1: onay jetonu iste. İstek durumu değiştirmez; hata = bir şey uygulanmadı. */
  async jetonIste(s: Secim): Promise<JetonSonucu> {
    const h = secimHatasi(s);
    if (h !== null) return { hata: { kod: 'gecersiz_istek', ileti: h } };
    if (yonu(s) !== 'azaltan') return { hata: { kod: 'yon_yanlis', ileti: 'Bu yön emniyeti artırır; jeton gerekmez.' } };
    this.jetonuDusur('Yeni jeton istendi; öncekinin yerine geçti.');
    const govde: Record<string, unknown> = { beklenen_run_id: this.runId(), girdi: s.girdi, hedef_durum: s.hedef };
    if (s.tup !== undefined) govde.tup = s.tup;
    return this.jetonAl('emniyet', '/emniyet/onay_jetonu', govde, { secim: s, ozet: secimOzeti(s) });
  }

  /**
   * Gevşetme adım 2. Jeton yoksa, süresi dolduysa, onay verilmediyse ya da seçim jetonunkinden
   * farklıysa HİÇ istek gönderilmez. Jeton tek kullanımlıktır: gönderilmeden önce düşürülür;
   * sonuç ne olursa olsun yeniden gönderilmez ve yeni jeton kendiliğinden istenmez.
   */
  gevset(s: Secim, onay: boolean): Promise<KomutKaydi> | KomutKaydi {
    const ozet = secimOzeti(s);
    const h = secimHatasi(s);
    if (h !== null) return this.komutlar.engelle('emniyet_degistir', 'gecersiz_istek', h, ozet);
    if (yonu(s) !== 'azaltan') return this.komutlar.engelle('emniyet_degistir', 'yon_yanlis', 'Bu yön emniyeti artırır.', ozet);
    const j = this.jeton();
    if (j === null || j.tur !== 'emniyet') {
      return this.komutlar.engelle('emniyet_degistir', 'jeton_yok', `Onay jetonu yok ya da süresi doldu; gevşetme gönderilmedi. İki adımı baştan yapın.`, ozet);
    }
    if (onay !== true) return this.komutlar.engelle('emniyet_degistir', 'onay_yok', 'Onay kutusu işaretlenmedi; gevşetme gönderilmedi.', ozet);
    if (j.secim === undefined || j.secim.girdi !== s.girdi || j.secim.tup !== s.tup || j.secim.hedef !== s.hedef) {
      this.jetonuDusur('Jeton başka bir işlem içindi; düştü.');
      return this.komutlar.engelle('emniyet_degistir', 'jeton_uyusmuyor', 'Onay jetonu bu işlem için alınmamış; gevşetme gönderilmedi.', ozet);
    }
    const govde: Record<string, unknown> = {
      beklenen_run_id: j.run_id,
      girdi: s.girdi,
      hedef_durum: s.hedef,
      onay: true,
      onay_jetonu: j.jeton,
      beklenen_emniyet_rev: j.emniyet_rev,
    };
    if (s.tup !== undefined) govde.tup = s.tup;
    this.jetonuDusur('Onay jetonu kullanıldı (tek kullanımlık).');
    return this.komutlar.gonderGenel('emniyet_degistir', () => this.baglanti.http.komut<{ komut_no: number }>('/emniyet/degistir', govde), {
      cifteEngel: true,
      ozet,
    });
  }

  private async jetonAl(
    tur: OnayJetonu['tur'],
    yol: string,
    govde: Record<string, unknown>,
    ek: Pick<OnayJetonu, 'ozet'> & Partial<OnayJetonu>,
  ): Promise<JetonSonucu> {
    if (this.baglanti.bilgi.durum !== 'bagli') {
      return { hata: { kod: 'sunucuya_ulasilamadi', ileti: 'Bağlantı yok; onay jetonu istenmedi.' } };
    }
    const nesil = this.jetonNesli;
    const alindi = this.simdi(); // Ağda geçen süre de jetonun ömründen düşer.
    let y: JetonYaniti;
    try {
      y = await this.baglanti.http.kimlikliIstek<JetonYaniti>(yol, govde);
    } catch (h) {
      const kod = h instanceof A3Hatasi ? h.kod : 'ag_hatasi';
      return { hata: { kod, ileti: hataIletisi(kod, h instanceof Error ? h.message : null, h instanceof A3Hatasi ? h.ayrinti : null) } };
    }
    if (typeof y.onay_jetonu !== 'string' || !/^[0-9a-f]{32}$/.test(y.onay_jetonu) || typeof y.gecerlilik_ms !== 'number' || typeof y.emniyet_rev !== 'number') {
      return { hata: { kod: 'yanit_cozulemedi', ileti: hataIletisi('yanit_cozulemedi') } };
    }
    const rev = this.baglanti.son('emniyet/girdiler')?.yuk.emniyet_rev;
    if (nesil !== this.jetonNesli || this.baglanti.bilgi.durum !== 'bagli' ||
        (rev !== undefined && rev !== null && Number(rev) > y.emniyet_rev) ||
        this.simdi() - alindi >= y.gecerlilik_ms - JETON_PAYI_MS) {
      return { hata: { kod: 'onay_jetonu_gecersiz', ileti: 'Bekleyen onay geçersizleşti; iki adımı baştan yapın.' } };
    }
    const j: OnayJetonu = {
      tur,
      jeton: y.onay_jetonu,
      run_id: typeof y.run_id === 'string' ? y.run_id : null,
      emniyet_rev: y.emniyet_rev,
      alindi,
      gecerlilik_ms: y.gecerlilik_ms,
      ...ek,
    };
    this.jeton_ = j;
    this.jetonNedeni_ = '';
    this.bildir();
    return { jeton: j };
  }

  // ---------------------------------------------------------------- yer istasyonu

  /** `GET /yer_istasyonu/komutlar`: sunucunun listesi; atış isteği komutları çıkarılır (W5-K54). */
  async yerKomutlari(): Promise<{ komutlar: KomutTanimi[]; gizlenen: string[] } | { hata: { kod: string; ileti: string } }> {
    let y: { komutlar?: unknown };
    try {
      y = await this.baglanti.http.istek<{ komutlar?: unknown }>('GET', '/yer_istasyonu/komutlar');
    } catch (h) {
      const kod = h instanceof A3Hatasi ? h.kod : 'ag_hatasi';
      return { hata: { kod, ileti: hataIletisi(kod, h instanceof Error ? h.message : null) } };
    }
    if (!Array.isArray(y.komutlar)) return { hata: { kod: 'yanit_cozulemedi', ileti: hataIletisi('yanit_cozulemedi') } };
    const hepsi = y.komutlar as KomutTanimi[];
    const gizlenen = hepsi.filter((k) => atisKomutuMu(k)).map((k) => k.ad);
    this.katalog_ = hepsi.filter((k) => !atisKomutuMu(k));
    return { komutlar: this.katalog_, gizlenen };
  }

  private tanim(ad: string): KomutTanimi | null {
    return this.katalog_?.find((k) => k.ad === ad) ?? null;
  }

  /** Komut iki adımlı mı: onay isteyen ya da genişletici (en kısıtlayan okuma). */
  static ikiAdimli(t: KomutTanimi): boolean {
    return t.onay || t.sinif === 'genisletici';
  }

  async yerJetonIste(ad: string, parametreler: Record<string, unknown>): Promise<JetonSonucu> {
    const t = this.tanim(ad);
    if (t === null) return { hata: { kod: 'komut_listede_yok', ileti: hataIletisi('komut_listede_yok') } };
    if (!EmniyetIstekleri.ikiAdimli(t)) return { hata: { kod: 'jeton_gerekmez', ileti: 'Bu komut tek adımlıdır.' } };
    this.jetonuDusur('Yeni jeton istendi; öncekinin yerine geçti.');
    return this.jetonAl('yer_istasyonu', '/yer_istasyonu/onay_jetonu', { beklenen_run_id: this.runId(), komut: ad, parametreler }, { komut: ad, parametreler, ozet: `${ad} ${kanonik(parametreler)}` });
  }

  /** Yer istasyonu komutu: listede yoksa gönderilmez; iki adımlı komutta gevşetme kuralları geçerli. */
  yerGonder(ad: string, parametreler: Record<string, unknown>, onay = false): Promise<KomutKaydi> | KomutKaydi {
    const ozet = `${ad} ${kanonik(parametreler)}`;
    const t = this.tanim(ad);
    if (t === null) return this.komutlar.engelle('yer_istasyonu_komut', 'komut_listede_yok', hataIletisi('komut_listede_yok'), ozet);
    if (this.baglanti.bilgi.durum !== 'bagli') {
      return this.komutlar.engelle('yer_istasyonu_komut', 'sunucuya_ulasilamadi', 'Bağlantı yok; komut gönderilmedi.', ozet);
    }
    const gonder = (govde: Record<string, unknown>, cifte: boolean): Promise<KomutKaydi> =>
      this.komutlar.gonderGenel('yer_istasyonu_komut', () => this.baglanti.http.komut<{ komut_no: number }>('/yer_istasyonu/komut', govde), { cifteEngel: cifte, ozet });
    if (!EmniyetIstekleri.ikiAdimli(t)) {
      return gonder({ beklenen_run_id: this.runId(), komut: ad, parametreler }, false);
    }
    const j = this.jeton();
    if (j === null || j.tur !== 'yer_istasyonu') {
      return this.komutlar.engelle('yer_istasyonu_komut', 'jeton_yok', 'Onay jetonu yok ya da süresi doldu; komut gönderilmedi. İki adımı baştan yapın.', ozet);
    }
    if (onay !== true) return this.komutlar.engelle('yer_istasyonu_komut', 'onay_yok', 'Onay kutusu işaretlenmedi; komut gönderilmedi.', ozet);
    if (j.komut !== ad || kanonik(j.parametreler ?? {}) !== kanonik(parametreler)) {
      this.jetonuDusur('Jeton başka bir komut ya da parametre içindi; düştü.');
      return this.komutlar.engelle('yer_istasyonu_komut', 'jeton_uyusmuyor', 'Onay jetonu bu komut ve parametreler için alınmamış; gönderilmedi.', ozet);
    }
    const govde = { beklenen_run_id: j.run_id, komut: ad, parametreler: j.parametreler ?? {}, onay: true, onay_jetonu: j.jeton, beklenen_emniyet_rev: j.emniyet_rev };
    this.jetonuDusur('Onay jetonu kullanıldı (tek kullanımlık).');
    return gonder(govde, true);
  }
}
