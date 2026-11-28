// Komut katmanı: panellerin durum değiştiren isteklerinin tek yolu (W5-web-komut).
//
//  - `Ida-Istemci` başlığı ve zorunlu `beklenen_run_id` / `beklenen_durum_rev` alanları
//    (A3-K50, K54, K123) `A3Http.calistirma` üzerinden gider; revizyonlar ekranın
//    gösterdiği, yani sunucunun son bildirdiği `calistirma/durum` çerçevesinden alınır.
//  - 202 yanıtı (`kuyrukta`) komutun uygulandığı anlamına gelmez; nihai sonuç
//    `komut_sonucu` olayıyla gelir (A3-K56, K57). Olay 202 yanıtından önce de gelebilir
//    (arabelleğe alınır); olay kaçarsa `GET /komutlar/{komut_no}` ile sorulur (A3-K60).
//  - Süre sınırı ya da bağlantı kopması: sonuç "bilinmiyor" diye gösterilir (komut uygulanmış
//    olabilir); sonraki olay ya da sorgu gerçek sonucu getirirse kayıt güncellenir.
//  - İyimser gösterim yoktur (W5-K44): bu sınıf yalnız komutların kaydını tutar; çalıştırma
//    durumunu hiçbir zaman kendisi değiştirmez ya da tahmin etmez.

import type { A3Baglanti } from './baglanti';
import { hataIletisi } from './hata';
import { A3Hatasi, type CalistirmaKomutu } from './http';
import type { YanitKomut, YanitSenaryoYukle } from './iletiler';
import type { OlayKaydi } from './olay';

export type KomutAsamasi = 'gonderiliyor' | 'kuyrukta' | 'uygulandi' | 'reddedildi' | 'bilinmiyor';

export type KomutAdi = CalistirmaKomutu | 'senaryo_yukle' | 'emniyet_degistir' | 'yer_istasyonu_komut';

export interface KomutKaydi {
  /** Bu istemci içinde artan yerel numara. */
  yerel: number;
  komut: KomutAdi;
  etiket: string;
  asama: KomutAsamasi;
  /** Sunucunun verdiği A3 komut numarası (202 / ret yanıtından). */
  komut_no: number | null;
  /** Ret ya da bilinmeyen sonucun nedeni: A3 §3.9 kodu ya da istemci içi kod. */
  hata_kodu: string | null;
  /** Kullanıcıya gösterilecek Türkçe ileti (sonuç satırı). */
  ileti: string;
  uygulama_adimi: number | null;
  /** Komutun sunucuya gittiği duvar saati anı (ms). */
  gonderildi: number;
  /** Nihai ya da "bilinmiyor" sonucun anı (ms). */
  sonuc_zamani: number | null;
  /** Komut gerçekten sunucuya gönderildi mi (yerel engel değilse true). */
  gonderildi_mi: boolean;
  /** Komutun neyi değiştirdiğini söyleyen kısa metin (emniyet girdisi ve yer istasyonu komutları). */
  ozet: string;
}

export interface KomutAyari {
  /** 202'den nihai sonuca kadar süre sınırı (ms); varsayılan 15 000. */
  sureMs?: number;
  /** `baslat` için süre sınırı (ön denetim, HIL'de A1 el sıkışması dahil); varsayılan 60 000. */
  baslatSureMs?: number;
  /** Olay gelmezse `GET /komutlar` sorgusunun başladığı bekleme (ms); varsayılan 1 000. */
  olayBeklemeMs?: number;
  /** Sorgular arası süre (ms); varsayılan 500. */
  yoklamaMs?: number;
  /** Tutulan en çok kayıt; varsayılan 50. */
  enCokKayit?: number;
}

export const KOMUT_ETIKETLERI: Readonly<Record<KomutAdi, string>> = {
  baslat: 'Başlat',
  duraklat: 'Duraklat',
  surdur: 'Sürdür',
  adimla: 'Adımla',
  hiz: 'Hız',
  durdur: 'Durdur',
  sifirla: 'Sıfırla',
  senaryo_yukle: 'Senaryo yükle',
  emniyet_degistir: 'Emniyet girdisi',
  yer_istasyonu_komut: 'Yer istasyonu komutu',
};

const SONUC_ARABELLEGI = 256;

interface Bekleyen {
  son: number;
  zamanlayicilar: ReturnType<typeof setTimeout>[];
  coz: (k: KomutKaydi) => void;
}

export class KomutYoneticisi {
  private readonly kayitlar_: KomutKaydi[] = [];
  private readonly komutNoyaGore = new Map<number, KomutKaydi>();
  private readonly bekleyenler = new Map<number, Bekleyen>();
  /** `komut_sonucu` olaylarının komut numarasına göre arabelleği (olay 202'den önce gelebilir). */
  private readonly sonucArabellegi = new Map<number, Record<string, unknown>>();
  private readonly dinleyiciler = new Set<(k: readonly KomutKaydi[], son: KomutKaydi | null) => void>();
  private yerelNo = 0;
  private baglantiVardi = false;
  private kapali = false;
  private readonly kaldir: (() => void)[] = [];
  private readonly ayar: Required<KomutAyari>;

  constructor(
    private readonly baglanti: A3Baglanti,
    ayar: KomutAyari = {},
  ) {
    this.ayar = {
      sureMs: ayar.sureMs ?? 15_000,
      baslatSureMs: ayar.baslatSureMs ?? 60_000,
      olayBeklemeMs: ayar.olayBeklemeMs ?? 1_000,
      yoklamaMs: ayar.yoklamaMs ?? 500,
      enCokKayit: ayar.enCokKayit ?? 50,
    };
    this.kaldir.push(baglanti.olaylar((o) => this.olay(o)));
    this.kaldir.push(baglanti.durumDinle(() => this.baglantiDegisti()));
  }

  kapat(): void {
    this.kapali = true;
    for (const k of this.kaldir) k();
    for (const b of this.bekleyenler.values()) for (const z of b.zamanlayicilar) clearTimeout(z);
    this.bekleyenler.clear();
  }

  /** En yeni başta. */
  kayitlar(): readonly KomutKaydi[] {
    return this.kayitlar_;
  }

  dinle(f: (k: readonly KomutKaydi[], son: KomutKaydi | null) => void): () => void {
    this.dinleyiciler.add(f);
    f(this.kayitlar_, this.kayitlar_[0] ?? null);
    return () => this.dinleyiciler.delete(f);
  }

  /**
   * Ekranın şu an gösterdiği çalıştırma kimliği ve durum revizyonu: sunucunun son
   * `calistirma/durum` çerçevesi. Çerçeve yoksa ya da bağlantı kopuksa null.
   */
  beklenen(): { run_id: string | null; durum_rev: number } | null {
    if (this.baglanti.bilgi.durum !== 'bagli') return null;
    const c = this.baglanti.son('calistirma/durum');
    if (c === undefined) return null;
    return { run_id: c.yuk.run_id === '' ? null : c.yuk.run_id, durum_rev: Number(c.yuk.durum_rev) };
  }

  private yeni(komut: KomutAdi): KomutKaydi {
    const k: KomutKaydi = {
      yerel: ++this.yerelNo,
      komut,
      etiket: KOMUT_ETIKETLERI[komut],
      asama: 'gonderiliyor',
      komut_no: null,
      hata_kodu: null,
      ileti: 'sunucuya gönderiliyor…',
      uygulama_adimi: null,
      gonderildi: Date.now(),
      sonuc_zamani: null,
      gonderildi_mi: true,
      ozet: '',
    };
    this.kayitlar_.unshift(k);
    while (this.kayitlar_.length > this.ayar.enCokKayit) {
      const eski = this.kayitlar_.pop()!;
      if (eski.komut_no !== null && this.komutNoyaGore.get(eski.komut_no) === eski) this.komutNoyaGore.delete(eski.komut_no);
    }
    this.bildir(k);
    return k;
  }

  private bildir(k: KomutKaydi | null): void {
    for (const f of this.dinleyiciler) {
      try {
        f(this.kayitlar_, k);
      } catch (h) {
        console.error(h);
      }
    }
  }

  /** Aynı komut HTTP yanıtı beklerken ikinci kez gönderilmez (çift tıklama). */
  private gonderilirken(komut: KomutAdi): boolean {
    return this.kayitlar_.some((k) => k.komut === komut && k.asama === 'gonderiliyor');
  }

  private engel(komut: KomutAdi, kod: string, ileti: string): KomutKaydi {
    const k = this.yeni(komut);
    k.asama = 'reddedildi';
    k.hata_kodu = kod;
    k.ileti = ileti;
    k.gonderildi_mi = false;
    k.sonuc_zamani = Date.now();
    this.bildir(k);
    return k;
  }

  /**
   * Çalıştırma kontrolü komutu gönderir. Sonuç `kuyrukta` → (`uygulandi` | `reddedildi`)
   * ya da `bilinmiyor` olur; söz nihai (ya da bilinmiyor) kayıtla çözülür, hiç reddetmez.
   * `ek`: komuta özgü alanlar (`carpan`, `periyot_sayisi`, `gecersizligi_onayla`).
   */
  async gonder(komut: CalistirmaKomutu, ek: Record<string, unknown> = {}): Promise<KomutKaydi> {
    if (this.kapali) throw new Error('komut yöneticisi kapatıldı');
    if (this.gonderilirken(komut)) {
      return this.engel(komut, 'gonderiliyor', 'Aynı komut zaten gönderiliyor; yanıt bekleniyor.');
    }
    const beklenen = this.beklenen();
    if (beklenen === null) {
      return this.engel(komut, 'durum_yok', 'Gönderilmedi: sunucudan güncel durum gelmedi ya da bağlantı yok.');
    }
    const k = this.yeni(komut);
    let yanit;
    try {
      yanit = await this.baglanti.http.calistirma(komut, beklenen, ek);
    } catch (h) {
      this.gonderimHatasi(k, h);
      return k;
    }
    k.komut_no = yanit.komut_no;
    this.komutNoyaGore.set(yanit.komut_no, k);
    k.asama = 'kuyrukta';
    k.ileti = 'sunucu kabul etti (kuyrukta); uygulanması bekleniyor';
    this.bildir(k);
    return this.sonucuBekle(k, komut === 'baslat' ? this.ayar.baslatSureMs : this.ayar.sureMs);
  }

  /**
   * Emniyet girdisi ve yer istasyonu komutlarının ortak yolu (W5-web-emniyet). Çağıran,
   * isteği kuran işlevi verir; kimlik, revizyon ve jeton kararları çağıranın (emniyet-istek.ts)
   * işidir. Bu yöntem yalnız kaydı ve sonucu izler: 202 → komut_sonucu / GET /komutlar.
   *  - `istekSureMs` içinde HTTP yanıtı gelmezse kayıt "bilinmiyor"a düşer (komut gitmiş olabilir).
   *  - Gönderim başladıktan sonra ağ hatası "bilinmiyor"dur; uygulanmış olabilir.
   *  - `cifteEngel`: açıksa yanıt beklenirken aynı komut ikinci kez gönderilmez.
   * Hiçbir koşulda isteği kendiliğinden yinelemez.
   */
  async gonderGenel(
    komut: KomutAdi,
    gonder: () => Promise<{ komut_no: number }>,
    secenek: { istekSureMs?: number; cifteEngel?: boolean; ozet?: string } = {},
  ): Promise<KomutKaydi> {
    if (this.kapali) throw new Error('komut yöneticisi kapatıldı');
    if (secenek.cifteEngel === true && this.gonderilirken(komut)) {
      return this.engel(komut, 'gonderiliyor', 'Aynı komut zaten gönderiliyor; yanıt bekleniyor.');
    }
    const k = this.yeni(komut);
    k.ozet = secenek.ozet ?? '';
    const sureMs = secenek.istekSureMs ?? 10_000;
    let zamanlayici: ReturnType<typeof setTimeout> | undefined;
    const sureAsimi = new Promise<'zaman_asimi'>((coz) => {
      zamanlayici = setTimeout(() => coz('zaman_asimi'), sureMs);
    });
    const istek = gonder();
    istek.catch(() => undefined); // süre aşımından sonra gelen hata yutulur
    let yanit: { komut_no: number } | 'zaman_asimi';
    try {
      yanit = await Promise.race([istek, sureAsimi]);
    } catch (h) {
      clearTimeout(zamanlayici);
      this.gonderimHatasi(k, h);
      return k;
    }
    clearTimeout(zamanlayici);
    if (yanit === 'zaman_asimi') {
      this.bitir(k, 'bilinmiyor', 'zaman_asimi', hataIletisi('zaman_asimi'), null);
      return k;
    }
    k.komut_no = yanit.komut_no;
    this.komutNoyaGore.set(yanit.komut_no, k);
    k.asama = 'kuyrukta';
    k.ileti = 'sunucu kabul etti (kuyrukta); uygulanması bekleniyor';
    this.bildir(k);
    return this.sonucuBekle(k, this.ayar.sureMs);
  }

  /** Hiç istek göndermeden "uygulanmadı" kaydı bırakır (yerel engel; ekranda UYGULANMADI). */
  engelle(komut: KomutAdi, kod: string, ileti: string, ozet = ''): KomutKaydi {
    const k = this.engel(komut, kod, ileti);
    k.ozet = ozet;
    this.bildir(k);
    return k;
  }

  /**
   * Senaryo yükleme (A3-K77): senkron uç; 200 yanıtı yüklemenin sonucudur. Doğrulamadan
   * geçmezse 422 `senaryo_gecersiz` ve A5 raporu `A3Hatasi.ayrinti.rapor`'dadır.
   */
  async senaryoYukle(girdi: { ad: string } | { icerik: string }): Promise<{ kayit: KomutKaydi; yanit: YanitSenaryoYukle | null; hata: A3Hatasi | null }> {
    if (this.kapali) throw new Error('komut yöneticisi kapatıldı');
    if (this.gonderilirken('senaryo_yukle')) {
      return { kayit: this.engel('senaryo_yukle', 'gonderiliyor', 'Aynı komut zaten gönderiliyor; yanıt bekleniyor.'), yanit: null, hata: null };
    }
    const beklenen = this.beklenen();
    if (beklenen === null) {
      return { kayit: this.engel('senaryo_yukle', 'durum_yok', 'Gönderilmedi: sunucudan güncel durum gelmedi ya da bağlantı yok.'), yanit: null, hata: null };
    }
    const k = this.yeni('senaryo_yukle');
    try {
      const y = await this.baglanti.http.senaryoYukle(girdi, beklenen);
      k.komut_no = y.komut_no;
      this.komutNoyaGore.set(y.komut_no, k);
      this.bitir(k, 'uygulandi', null, `senaryo yüklendi (${y.senaryo_ozeti.slice(0, 15)}…)`, null);
      return { kayit: k, yanit: y, hata: null };
    } catch (h) {
      this.gonderimHatasi(k, h);
      return { kayit: k, yanit: null, hata: h instanceof A3Hatasi ? h : null };
    }
  }

  private gonderimHatasi(k: KomutKaydi, h: unknown): void {
    if (h instanceof A3Hatasi && h.kod !== 'ag_hatasi' && h.kod !== 'yanit_cozulemedi') {
      k.komut_no = h.komut_no;
      if (h.komut_no !== null) this.komutNoyaGore.set(h.komut_no, k);
      this.bitir(k, 'reddedildi', h.kod, hataIletisi(h.kod, h.message, h.ayrinti), null);
      return;
    }
    // Ağ hatası ya da çözülemeyen yanıt: istek sunucuya ulaşmış olabilir.
    const kod = h instanceof A3Hatasi ? h.kod : 'ag_hatasi';
    this.bitir(k, 'bilinmiyor', kod, hataIletisi(kod, h instanceof Error ? h.message : null), null);
  }

  private bitir(k: KomutKaydi, asama: KomutAsamasi, kod: string | null, ileti: string, adim: number | null): void {
    k.asama = asama;
    k.hata_kodu = kod;
    k.ileti = ileti;
    k.uygulama_adimi = adim;
    k.sonuc_zamani = Date.now();
    const b = this.bekleyenler.get(k.yerel);
    if (b !== undefined && asama !== 'kuyrukta') {
      for (const z of b.zamanlayicilar) clearTimeout(z);
      this.bekleyenler.delete(k.yerel);
      b.coz(k);
    }
    this.bildir(k);
  }

  private sonucUygula(k: KomutKaydi, s: { sonuc: string; hata_kodu?: string | null; uygulama_adimi?: number | null }, hataIleti?: string): boolean {
    const adim = typeof s.uygulama_adimi === 'number' ? s.uygulama_adimi : null;
    if (s.sonuc === 'uygulandi') {
      this.bitir(k, 'uygulandi', null, 'uygulandı', adim);
      return true;
    }
    if (s.sonuc === 'reddedildi') {
      const kod = s.hata_kodu ?? 'bilinmiyor';
      this.bitir(k, 'reddedildi', kod, hataIletisi(kod, hataIleti), adim);
      return true;
    }
    return false;
  }

  private sonucuBekle(k: KomutKaydi, sureMs: number): Promise<KomutKaydi> {
    const komutNo = k.komut_no!;
    return new Promise<KomutKaydi>((coz) => {
      const ara = this.sonucArabellegi.get(komutNo);
      if (ara !== undefined && this.arabellektenUygula(k, ara)) {
        coz(k);
        return;
      }
      const son = Date.now() + sureMs;
      const b: Bekleyen = { son, zamanlayicilar: [], coz };
      this.bekleyenler.set(k.yerel, b);
      const yokla = (): void => {
        if (this.bekleyenler.get(k.yerel) !== b) return;
        if (Date.now() >= son) {
          this.bitir(k, 'bilinmiyor', 'zaman_asimi', hataIletisi('zaman_asimi'), null);
          return;
        }
        void this.baglanti.http.komutDurumu(komutNo).then(
          (y) => {
            if (this.bekleyenler.get(k.yerel) !== b) return;
            if (!this.sorguUygula(k, y)) b.zamanlayicilar.push(setTimeout(yokla, this.ayar.yoklamaMs));
          },
          () => {
            if (this.bekleyenler.get(k.yerel) !== b) return;
            b.zamanlayicilar.push(setTimeout(yokla, this.ayar.yoklamaMs));
          },
        );
      };
      // Olay kaçarsa sorgula; olay gelirse `olay()` bekleyeni çözer.
      b.zamanlayicilar.push(setTimeout(yokla, Math.min(this.ayar.olayBeklemeMs, sureMs)));
      // Süre dolunca (sorgu yanıt vermese de) bilinmiyor.
      b.zamanlayicilar.push(
        setTimeout(() => {
          if (this.bekleyenler.get(k.yerel) === b) this.bitir(k, 'bilinmiyor', 'zaman_asimi', hataIletisi('zaman_asimi'), null);
        }, sureMs + 5),
      );
    });
  }

  private arabellektenUygula(k: KomutKaydi, a: Record<string, unknown>): boolean {
    return this.sonucUygula(k, {
      sonuc: String(a.sonuc),
      hata_kodu: typeof a.hata_kodu === 'string' ? a.hata_kodu : null,
      uygulama_adimi: typeof a.uygulama_adimi === 'number' ? a.uygulama_adimi : null,
    });
  }

  private sorguUygula(k: KomutKaydi, y: YanitKomut): boolean {
    if (y.durum === 'kuyrukta') return false;
    const kod = typeof y.hata?.kod === 'string' ? y.hata.kod : null;
    const iletiS = typeof y.hata?.ileti === 'string' ? y.hata.ileti : undefined;
    return this.sonucUygula(k, { sonuc: y.durum, hata_kodu: kod, uygulama_adimi: y.uygulama_adimi ?? null }, iletiS);
  }

  private olay(o: OlayKaydi): void {
    if (o.tur !== 'komut_sonucu' || o.ayrinti === null) return;
    const no = o.ayrinti.komut_no;
    if (typeof no !== 'number') return;
    this.sonucArabellegi.set(no, o.ayrinti);
    if (this.sonucArabellegi.size > SONUC_ARABELLEGI) this.sonucArabellegi.delete(this.sonucArabellegi.keys().next().value as number);
    const k = this.komutNoyaGore.get(no);
    // Bekleyen komut ya da sonradan sonucu öğrenilen "bilinmiyor" kayıt.
    if (k !== undefined && (k.asama === 'kuyrukta' || k.asama === 'bilinmiyor')) this.arabellektenUygula(k, o.ayrinti);
  }

  private baglantiDegisti(): void {
    const bagli = this.baglanti.bilgi.durum === 'bagli';
    if (!bagli && this.baglantiVardi) {
      // Bağlantı koptu: kuyruktaki komutların sonucu bilinmiyor; geç gelen olay/sorgu düzeltir.
      for (const k of this.kayitlar_) {
        if (k.asama === 'kuyrukta') this.bitir(k, 'bilinmiyor', 'baglanti_koptu', hataIletisi('baglanti_koptu'), null);
      }
    }
    if (bagli && this.baglantiVardi === false) this.baglantiVardi = true;
    if (bagli) {
      this.baglantiVardi = true;
      // Toparlanma: sonucu bilinmeyenleri bir kez sorgula.
      for (const k of this.kayitlar_) {
        if (k.asama !== 'bilinmiyor' || k.komut_no === null) continue;
        const ara = this.sonucArabellegi.get(k.komut_no);
        if (ara !== undefined && this.arabellektenUygula(k, ara)) continue;
        const no = k.komut_no;
        void this.baglanti.http.komutDurumu(no).then(
          (y) => {
            if (k.asama === 'bilinmiyor') this.sorguUygula(k, y);
          },
          () => undefined,
        );
      }
    }
  }
}
