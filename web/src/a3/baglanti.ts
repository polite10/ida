// A3 bağlantısı: sürüm el sıkışması, WebSocket, kanal aboneliği, yeniden
// bağlanma ve olay boşluğunun HTTP'den doldurulması (A3 §4.1–4.5).

import { Cozucu, type Cerceve } from './cerceve';
import { A3Hatasi, A3Http } from './http';
import type {
  Hz,
  WsAboneOl,
  WsAbonelikBirak,
  WsAbonelikYanitiKanal,
  WsMerhaba,
  WsSunucuIletisi,
  YanitSurum,
} from './iletiler';
import { A3_SURUMU, KANALLAR, KATALOG_OZETI, WS_ALT_PROTOKOLU, type KanalAdi, type KanalYuku, type YukOlay } from './katalog';
import { cercevedenOlay, jsondanOlay, type OlayKaydi } from './olay';
import { siraEski, siraFarki, siraGeri, siraSiralayici, siraYeni } from './sira';

export type BaglantiDurumu =
  | 'kapali'
  | 'baglaniyor'
  | 'bagli'
  | 'kopuk'
  | 'erisim_reddedildi'
  | 'surum_uyusmazligi';

export interface BaglantiBilgisi {
  durum: BaglantiDurumu;
  /** Art arda başarısız deneme sayısı; bağlanınca 0. */
  deneme: number;
  /** Sonraki denemenin duvar saati anı (ms, `Date.now()`), yoksa null. */
  sonrakiDeneme: number | null;
  /** İnsan için kısa açıklama (Türkçe). */
  ileti: string;
  surum: YanitSurum | null;
  merhaba: WsMerhaba | null;
  /** A3-K15: küçük sürüm ya da katalog özeti farkı. */
  uyarilar: string[];
  /** Son kapanış kodu (A3 §3.7). */
  kapanisKodu: number | null;
  /** Son başarılı bağlantının kurulduğu an (ms). */
  bagliOldugu: number | null;
  /** Kaç kez yeniden bağlanıldı (ilk bağlantı hariç). */
  yenidenBaglanma: number;
  /**
   * Kurtarılamayan canlı olay aralıkları: `sonra`'dan sonraki `kadar`'a kadar (dahil)
   * olaylar alınamadı (410 ya da yinelenen geçici hata; A3-K45, A3 §5: kayıttan okunur).
   */
  olayBosluklari: { sonra: number; kadar: number }[];
}

export interface A3BaglantiAyari {
  /** Sunucu kökü, ör. `http://127.0.0.1:47100`. */
  taban: string;
  jeton: string;
  /** İlk bağlantıda yüklenecek geçmiş olay sayısı (A3-K45; en çok 1000). */
  gecmisOlay?: number;
  WebSocketSinifi?: typeof WebSocket;
  fetchIslevi?: typeof fetch;
  /** Yeniden deneme gecikmesi: ilk ve azami (ms). */
  ilkGecikmeMs?: number;
  azamiGecikmeMs?: number;
  /** Geçmiş isteği geçici hatasında ilk yeniden deneme gecikmesi (ms); 5 deneme, ikiye katlanır. */
  gecmisYenidenDenemeMs?: number;
}

type Dinleyici<T> = (c: Cerceve<T>) => void;

interface Abonelik {
  hz: Hz | undefined;
  dinleyiciler: Set<Dinleyici<unknown>>;
}

const ZORUNLU = new Set<string>(KANALLAR.filter((k) => k.zorunlu_abonelik).map((k) => k.ad));

function buyukSurum(s: string): string {
  return s.split('.')[0] ?? s;
}

export class A3Baglanti {
  readonly http: A3Http;
  readonly cozucu = new Cozucu();
  readonly bilgi: BaglantiBilgisi = {
    durum: 'kapali',
    deneme: 0,
    sonrakiDeneme: null,
    ileti: 'başlatılmadı',
    surum: null,
    merhaba: null,
    uyarilar: [],
    kapanisKodu: null,
    bagliOldugu: null,
    yenidenBaglanma: 0,
    olayBosluklari: [],
  };
  /** Son abonelik yanıtı, kanal adına göre. */
  readonly abonelikSonuclari = new Map<string, WsAbonelikYanitiKanal>();
  /** Kanal başına son çerçeve (anahtarlı kanallarda son gelen anahtar). */
  private readonly sonCerceve = new Map<string, Cerceve>();

  private ws: WebSocket | null = null;
  private zamanlayici: ReturnType<typeof setTimeout> | null = null;
  private istekNo = 0;
  private durduruldu = true;
  private ilkBaglanti = true;
  private readonly abonelikler = new Map<string, Abonelik>();
  private readonly durumDinleyicileri = new Set<(b: BaglantiBilgisi) => void>();
  private readonly olayDinleyicileri = new Set<(o: OlayKaydi) => void>();
  /**
   * Canlı olay imleci: dağıtılmış son canlı olayın sırası; ilk `merhaba`'dan önce null.
   * Kayıttan gelen (kaynak_kayit) olaylar bu imleci etkilemez (A3-K27, K112).
   * Kurtarılmamış bir aralık boyunca ilerletilmez.
   */
  private imlec: number | null = null;
  /** Etkin geçmiş doldurma (yalnız biri). */
  private dolum: { hedef: number } | null = null;
  /** Doldurma sürerken gelen canlı olaylar. */
  private bekleyenOlaylar: OlayKaydi[] = [];
  private readonly WS: typeof WebSocket;

  constructor(private readonly ayar: A3BaglantiAyari) {
    this.http = new A3Http(ayar.taban, ayar.jeton, ayar.fetchIslevi);
    this.WS = ayar.WebSocketSinifi ?? WebSocket;
  }

  // ---- Dışa açık işlemler ----

  baslat(): void {
    if (!this.durduruldu) return;
    this.durduruldu = false;
    void this.baglan();
  }

  kapat(): void {
    this.durduruldu = true;
    if (this.zamanlayici !== null) clearTimeout(this.zamanlayici);
    this.zamanlayici = null;
    const ws = this.ws;
    this.ws = null;
    if (ws !== null) ws.close(1000);
    this.guncelle({ durum: 'kapali', ileti: 'kapatıldı', sonrakiDeneme: null });
  }

  /** Bekleyen yeniden denemeyi beklemeden hemen dener. */
  simdiDene(): void {
    if (this.durduruldu || this.bilgi.durum === 'bagli' || this.bilgi.durum === 'baglaniyor') return;
    if (this.zamanlayici !== null) clearTimeout(this.zamanlayici);
    this.zamanlayici = null;
    void this.baglan();
  }

  durumDinle(f: (b: BaglantiBilgisi) => void): () => void {
    this.durumDinleyicileri.add(f);
    f(this.bilgi);
    return () => this.durumDinleyicileri.delete(f);
  }

  /**
   * Kanala abone olur. Zorunlu kanallar (A3-K29) için yalnız dinleyici eklenir.
   * Aynı kanala farklı hızlarla abone olunursa en yüksek hız istenir.
   */
  abone<K extends KanalAdi>(kanal: K, dinleyici: Dinleyici<KanalYuku[K]>, hz?: Hz): () => void {
    let a = this.abonelikler.get(kanal);
    const yeni = a === undefined;
    if (a === undefined) {
      a = { hz, dinleyiciler: new Set() };
      this.abonelikler.set(kanal, a);
    }
    const oncekiHz = a.hz;
    if (hz !== undefined && (a.hz === undefined || hz > a.hz)) a.hz = hz;
    a.dinleyiciler.add(dinleyici as Dinleyici<unknown>);
    if (!ZORUNLU.has(kanal) && this.bilgi.durum === 'bagli' && (yeni || a.hz !== oncekiHz)) {
      this.aboneOlGonder([kanal]);
    }
    const son = this.sonCerceve.get(kanal);
    if (son !== undefined) this.cagir(dinleyici as Dinleyici<unknown>, son);
    return () => {
      const b = this.abonelikler.get(kanal);
      if (b === undefined) return;
      b.dinleyiciler.delete(dinleyici as Dinleyici<unknown>);
      if (b.dinleyiciler.size === 0) {
        this.abonelikler.delete(kanal);
        if (!ZORUNLU.has(kanal) && this.bilgi.durum === 'bagli') {
          const ileti: WsAbonelikBirak = { tur: 'abonelik_birak', istek_no: ++this.istekNo, kanallar: [kanal] };
          this.gonder(ileti);
        }
      }
    };
  }

  /** `olay` kanalının çözülmüş kayıtları; yeniden bağlanmada boşluk HTTP'den doldurulur (A3-K45). */
  olaylar(f: (o: OlayKaydi) => void): () => void {
    this.olayDinleyicileri.add(f);
    return () => this.olayDinleyicileri.delete(f);
  }

  son<K extends KanalAdi>(kanal: K): Cerceve<KanalYuku[K]> | undefined {
    return this.sonCerceve.get(kanal) as Cerceve<KanalYuku[K]> | undefined;
  }

  // ---- İç işleyiş ----

  private guncelle(p: Partial<BaglantiBilgisi>): void {
    Object.assign(this.bilgi, p);
    for (const f of this.durumDinleyicileri) {
      try {
        f(this.bilgi);
      } catch (h) {
        console.error(h);
      }
    }
  }

  private async baglan(): Promise<void> {
    if (this.durduruldu) return;
    this.guncelle({ durum: 'baglaniyor', ileti: 'sunucu sürümü soruluyor', sonrakiDeneme: null });
    let surum: YanitSurum;
    try {
      surum = await this.http.surum();
    } catch (h) {
      if (h instanceof A3Hatasi && h.kod === 'erisim_reddedildi') {
        // A3 §5: erişim hatası gösterilir, yeniden denenmez.
        this.durduruldu = true;
        this.guncelle({
          durum: 'erisim_reddedildi',
          ileti: 'erişim reddedildi: jeton geçersiz; sim sunucusunun yazdığı açılış adresiyle açın',
        });
        return;
      }
      this.yenidenDene(h instanceof Error ? h.message : String(h));
      return;
    }
    if (this.durduruldu) return;
    if (buyukSurum(surum.a3_surumu) !== buyukSurum(A3_SURUMU)) {
      // A3-K15: büyük sürüm farklıysa bağlanılmaz.
      this.durduruldu = true;
      this.guncelle({
        durum: 'surum_uyusmazligi',
        surum,
        ileti: `sürüm uyuşmazlığı: sunucu A3 ${surum.a3_surumu}, panel A3 ${A3_SURUMU}`,
      });
      return;
    }
    const uyarilar: string[] = [];
    if (surum.a3_surumu !== A3_SURUMU) uyarilar.push(`A3 küçük sürüm farkı: sunucu ${surum.a3_surumu}, panel ${A3_SURUMU}`);
    if (surum.katalog_ozeti !== KATALOG_OZETI) uyarilar.push('kanal kataloğu panelin derlendiği katalogdan farklı');
    this.guncelle({ surum, uyarilar, ileti: 'WebSocket açılıyor' });
    this.wsAc();
  }

  private wsAc(): void {
    const wsUrl = this.ayar.taban.replace(/^http/, 'ws').replace(/\/$/, '') + '/ws';
    let ws: WebSocket;
    try {
      ws = new this.WS(wsUrl, [WS_ALT_PROTOKOLU, `ida-erisim.${this.ayar.jeton}`]);
    } catch (h) {
      this.yenidenDene(h instanceof Error ? h.message : String(h));
      return;
    }
    ws.binaryType = 'arraybuffer';
    this.ws = ws;
    this.istekNo = 0;
    ws.onmessage = (e: MessageEvent) => {
      if (this.ws !== ws) return;
      if (typeof e.data === 'string') this.metinIletisi(e.data);
      else if (e.data instanceof ArrayBuffer) this.ikiliIleti(e.data);
    };
    ws.onclose = (e: CloseEvent) => {
      if (this.ws !== ws) return;
      this.ws = null;
      // Yarım kalan doldurma bırakılır; imleç kurtarılmamış aralıkta ilerlemedi,
      // yeni `merhaba` aradaki olayları yeniden ister.
      this.dolum = null;
      this.bekleyenOlaylar = [];
      if (this.durduruldu) return;
      this.bilgi.kapanisKodu = e.code;
      if (e.code === 4002) {
        this.durduruldu = true;
        this.guncelle({ durum: 'erisim_reddedildi', ileti: 'erişim jetonu geçersizleşti (4002)' });
        return;
      }
      this.yenidenDene(`bağlantı kapandı (${e.code}${e.reason ? ': ' + e.reason : ''})`);
    };
    ws.onerror = () => {
      // Ayrıntı tarayıcıda gizlidir; kapanış olayı ardından gelir.
    };
  }

  private yenidenDene(neden: string): void {
    if (this.durduruldu) return;
    const deneme = this.bilgi.deneme + 1;
    const ilk = this.ayar.ilkGecikmeMs ?? 500;
    const azami = this.ayar.azamiGecikmeMs ?? 5000;
    const gecikme = Math.min(azami, ilk * 2 ** Math.min(deneme - 1, 8)) * (0.85 + 0.3 * Math.random());
    this.guncelle({
      durum: 'kopuk',
      deneme,
      sonrakiDeneme: Date.now() + gecikme,
      ileti: neden,
    });
    if (this.zamanlayici !== null) clearTimeout(this.zamanlayici);
    this.zamanlayici = setTimeout(() => {
      this.zamanlayici = null;
      void this.baglan();
    }, gecikme);
  }

  private gonder(ileti: WsAboneOl | WsAbonelikBirak): void {
    if (this.ws !== null && this.ws.readyState === 1) this.ws.send(JSON.stringify(ileti));
  }

  private aboneOlGonder(kanallar: string[]): void {
    const liste: WsAboneOl['kanallar'] = [];
    for (const k of kanallar) {
      if (ZORUNLU.has(k)) continue;
      const a = this.abonelikler.get(k);
      liste.push(a?.hz !== undefined ? { kanal: k, hz: a.hz } : { kanal: k });
    }
    for (let i = 0; i < liste.length; i += 64) {
      this.gonder({ tur: 'abone_ol', istek_no: ++this.istekNo, kanallar: liste.slice(i, i + 64) });
    }
  }

  private metinIletisi(veri: string): void {
    let ileti: WsSunucuIletisi;
    try {
      ileti = JSON.parse(veri) as WsSunucuIletisi;
    } catch {
      return;
    }
    switch (ileti.tur) {
      case 'merhaba':
        this.merhabaAlindi(ileti);
        return;
      case 'abonelik_yaniti':
        for (const k of ileti.kanallar) this.abonelikSonuclari.set(k.kanal, k);
        return;
      case 'hata':
        console.warn('A3 hata iletisi:', ileti.kod, ileti.ileti);
        return;
      default:
        return;
    }
  }

  private merhabaAlindi(m: WsMerhaba): void {
    const yeniden = !this.ilkBaglanti;
    this.ilkBaglanti = false;
    const uyarilar = [...this.bilgi.uyarilar];
    if (m.katalog_ozeti !== KATALOG_OZETI && !uyarilar.some((u) => u.startsWith('kanal kataloğu'))) {
      uyarilar.push('kanal kataloğu panelin derlendiği katalogdan farklı');
    }
    this.guncelle({
      durum: 'bagli',
      deneme: 0,
      sonrakiDeneme: null,
      merhaba: m,
      uyarilar,
      ileti: 'bağlı',
      bagliOldugu: Date.now(),
      yenidenBaglanma: this.bilgi.yenidenBaglanma + (yeniden ? 1 : 0),
    });
    this.aboneOlGonder([...this.abonelikler.keys()]);

    // ---- Olay imleci (A3-K27, K45; seri aritmetik: iskelet §3.1) ----
    const son = m.son_olay_sirasi >>> 0;
    if (this.imlec === null) {
      // İlk bağlantı: istenirse son n olay geçmişten
      const n = Math.min(1000, this.ayar.gecmisOlay ?? 0);
      if (n > 0 && son !== 0) {
        this.imlec = son >= n ? son - n : 0;
        this.doldurBaslat(son);
      } else this.imlec = son;
      return;
    }
    if (siraYeni(son, this.imlec)) {
      // Yeniden bağlanma: aradaki olaylar
      this.doldurBaslat(son);
    } else if (siraEski(son, this.imlec)) {
      // Sıra geriledi (sarım değil): yeni sim süreci; yeni sürecin olayları baştan, en çok 1000
      this.dolum = null;
      this.bekleyenOlaylar = [];
      this.imlec = son >= 1000 ? son - 1000 : 0;
      this.guncelle({ uyarilar: [...this.bilgi.uyarilar, 'olay sırası geriledi: sim süreci yeniden başlamış'] });
      if (son !== this.imlec) this.doldurBaslat(son);
    }
  }

  /** Canlı imleçten `hedef`'e kadar geçmişi ister; o sürede gelen canlı olaylar bekletilir. */
  private doldurBaslat(hedef: number): void {
    if (this.dolum !== null) {
      if (siraYeni(hedef, this.dolum.hedef)) this.dolum.hedef = hedef;
      return;
    }
    this.dolum = { hedef };
    void this.olaylariDoldur(this.dolum);
  }

  private async olaylariDoldur(dolum: { hedef: number }): Promise<void> {
    const ws = this.ws;
    const ilkBekleme = this.ayar.gecmisYenidenDenemeMs ?? 1000;
    let hata: unknown = null;
    try {
      let denemeler = 0;
      for (let sayfa = 0; sayfa < 100 && this.imlec !== null && siraYeni(dolum.hedef, this.imlec); ) {
        let y;
        try {
          y = await this.http.olaylar(this.imlec, 1000);
        } catch (h) {
          if (this.dolum !== dolum || this.ws !== ws) return;
          // 410: kurtarılamaz. Diğer hatalar geçicidir: en çok 5 kez, artan beklemeyle.
          if ((h instanceof A3Hatasi && h.kod === 'gecmis_yok') || denemeler >= 4) {
            hata = h;
            break;
          }
          await new Promise((r) => setTimeout(r, ilkBekleme * 2 ** denemeler));
          denemeler++;
          if (this.dolum !== dolum || this.ws !== ws) return;
          continue;
        }
        if (this.dolum !== dolum || this.ws !== ws) return;
        denemeler = 0;
        sayfa++;
        let ilerledi = false;
        for (const j of y.olaylar) {
          const o = jsondanOlay(j);
          if (this.imlec !== null && siraYeni(o.sira, this.imlec)) {
            this.olayDagit(o);
            ilerledi = true;
          }
        }
        if (y.tamam || !ilerledi) break;
      }
      if (hata === null && this.imlec !== null && siraYeni(dolum.hedef, this.imlec)) hata = new Error('geçmiş eksik döndü');
    } finally {
      if (this.dolum === dolum && this.ws === ws) {
        if (hata !== null && this.imlec !== null) this.boslukBildir(this.imlec, dolum.hedef, hata);
        this.dolum = null;
        const imlec = this.imlec ?? 0;
        const bekleyen = this.bekleyenOlaylar.sort((a, b) => siraSiralayici(imlec)(a.sira, b.sira));
        this.bekleyenOlaylar = [];
        for (const o of bekleyen) this.canliOlay(o);
      }
    }
  }

  /** Kurtarılamayan aralığı görünür kılar ve imleci aralığın sonuna taşır. */
  private boslukBildir(sonra: number, kadar: number, neden: unknown): void {
    console.warn('olay geçmişi alınamadı:', neden);
    const ilk = (sonra + 1) >>> 0;
    const kod = neden instanceof A3Hatasi ? neden.kod : 'hata';
    this.guncelle({
      olayBosluklari: [...this.bilgi.olayBosluklari, { sonra, kadar }],
      uyarilar: [...this.bilgi.uyarilar, `olay ${ilk}–${kadar} alınamadı (${kod}); yalnız kayıtta (A6)`],
    });
    this.imlec = kadar;
  }

  private olayDagit(o: OlayKaydi): void {
    if (!o.kaynak_kayit) this.imlec = o.sira;
    for (const f of this.olayDinleyicileri) {
      try {
        f(o);
      } catch (h) {
        console.error(h);
      }
    }
  }

  /** Canlı olay: doldurma sürüyorsa beklet; yeni değilse at; boşluk varsa doldur. */
  private canliOlay(o: OlayKaydi): void {
    if (this.dolum !== null) {
      this.bekleyenOlaylar.push(o);
      return;
    }
    if (this.imlec === null) {
      this.olayDagit(o);
      return;
    }
    if (!siraYeni(o.sira, this.imlec)) return; // yineleme ya da eski
    if (siraFarki(o.sira, this.imlec) > 1) {
      // Akışta boşluk: önce aradakileri iste (A3-K45)
      this.bekleyenOlaylar.push(o);
      this.doldurBaslat(siraGeri(o.sira, 1));
      return;
    }
    this.olayDagit(o);
  }

  private cagir(f: Dinleyici<unknown>, c: Cerceve): void {
    try {
      f(c);
    } catch (h) {
      console.error(h);
    }
  }

  private ikiliIleti(veri: ArrayBuffer): void {
    for (const c of this.cozucu.ileti(veri)) {
      const ad = c.kanal.ad;
      this.sonCerceve.set(ad, c);
      if (ad === 'olay') {
        const o = cercevedenOlay(c as Cerceve<YukOlay>);
        // Kayıttan gelen olaylar (izleme; A3-K112) canlı imlece göre elenmez ve imleci değiştirmez.
        if (o.kaynak_kayit) this.olayDagit(o);
        else this.canliOlay(o);
      }
      const a = this.abonelikler.get(ad);
      if (a !== undefined) for (const f of a.dinleyiciler) this.cagir(f, c);
    }
  }
}

/**
 * Tarayıcıda açılış adresinden (A3-K05: `#erisim=<jeton>`) bağlantı ayarı çıkarır.
 * Jeton yoksa null.
 */
export function adrestenAyar(konum: Location): A3BaglantiAyari | null {
  const parca = new URLSearchParams(konum.hash.replace(/^#/, ''));
  const jeton = parca.get('erisim');
  if (jeton === null || !/^[0-9a-f]{64}$/.test(jeton)) return null;
  return { taban: konum.origin, jeton };
}
