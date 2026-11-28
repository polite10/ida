// Sahte A3 sunucusu: A3 tel biçimini (HTTP + WebSocket, ikili çerçeveler)
// konuşan, yalnız geliştirme için Node sunucusu. Ürünle dağıtılmaz; gerçek
// A3 sunucusu sim sunucusunun içinde C++ ile yazılacaktır.
//
// Uygulanan: erişim koruması (A3-K02, K04–K11), sürüm el sıkışması (K15–K17),
// abonelik ve seyreltme (K29–K37), yavaş istemci kuyrukları (K40–K42),
// canlılık (K46), istemci iletisi denetimi (K47, K48), olay geçmişi (K45) ve
// panellerin okuduğu GET uçları. Durum değiştiren uçlar bu dilimde yoktur.

import { createHash, randomBytes, timingSafeEqual } from 'node:crypto';
import { readFileSync, statSync } from 'node:fs';
import { createServer, type IncomingMessage, type ServerResponse } from 'node:http';
import type { Duplex } from 'node:stream';
import { dirname, extname, join, normalize, resolve, sep } from 'node:path';
import { WebSocketServer, type WebSocket, type RawData } from 'ws';

import { BAYRAK, kanalBul } from '../../src/a3/cerceve';
import { cerceveKodla, iletiBirlestir, yukKodla } from '../../src/a3/kodlayici';
import {
  A3_SURUMU,
  KANALLAR,
  OLAY_TURLERI,
  WS_ALT_PROTOKOLU,
  YUKLER,
  type KanalTanimi,
  type KodlamaGirdisi,
  type YukBaglantiDurum as YukBaglantiDurumCozulmus,
} from '../../src/a3/katalog';
import { ONDENETIM_MADDELERI, SahteDunya, type EmniyetKipi, type Kip, type Uretim } from './dunya';
import { semaDogrula, semaOku } from './sema';

type YukBaglantiDurum = KodlamaGirdisi<YukBaglantiDurumCozulmus>;

export interface SahteSunucuAyari {
  /** Varsayılan 47100 (A3-K01); 0 = işletim sistemi atar. */
  port?: number;
  /** Yalnız geliştirme: sabit jeton (64 küçük onaltılık). Verilmezse rastgele (A3-K04). */
  jeton?: string;
  /** Derlenmiş panel dosyalarının dizini (`dist`); yoksa statik yollar bilgi metni döner. */
  statik?: string | null;
  katalogYolu: string;
  kip?: Kip;
  tohum?: number;
  /** Koşu başlayınca sim zamanını bu saniyeye atla (yalnız geliştirme). */
  atla?: number;
  /** Dünya ve yayın tiki aralığı (ms); varsayılan 20 (A3-K33: 50 Hz). */
  tikMs?: number;
  /** Yalnız testler: bağlantı başına gönderim basıncını öykünür (true = soket yazamıyor). */
  basinc?: (baglantiId: number) => boolean;
  /** Yalnız testler: dünya ilerlemez; yalnız enjekte edilen üretimler yayınlanır. */
  dunyaDurdur?: boolean;
  /** Emniyet gösterimini sınamak için: kart geri okuması bilinmiyor ya da geçersiz değerler. */
  emniyetKipi?: EmniyetKipi;
  /** Elle yönetim: çalıştırma yalnız A3 komutlarıyla başlar ve ilerler (panelden denetim). */
  elle?: boolean;
  /** Onay jetonu ömrü (ms); varsayılan 10 000 (A3-K94). Testler jetonu `jetonlariSonlandir` ile de düşürür. */
  jetonOmruMs?: number;
}

export interface SahteSunucu {
  readonly port: number;
  readonly jeton: string;
  readonly acilisAdresi: string;
  readonly dunya: SahteDunya;
  /** Bütün bağlantıları 1001 ile kapatır (A3-K49) ve dinlemeyi bırakır. */
  kapat(): Promise<void>;
  /** Test için: açık WebSocket bağlantısı sayısı. */
  baglantiSayisi(): number;
  /** Test ve geliştirme: süreç sürerken bütün WebSocket bağlantılarını düşürür (ağ kopması benzetimi). */
  baglantilariKopar(): void;
  /** Yalnız testler: üretim enjeksiyonu. */
  readonly test: {
    ping360Ekle(sensorId: number, seq: number): void;
    olayEkle(): void;
    /** Kuyruktaki komutları uygulamaz (sonuç gecikmesi benzetimi); false verilince boşaltır. */
    komutlariTut(tut: boolean): void;
    /** Verilmiş bütün onay jetonlarını sunucu tarafında geçersiz kılar (süre dolumu öykünmesi). */
    jetonlariSonlandir(): void;
    /** Başka bir kısıtlamanın kabulünü öykünür: `emniyet_rev` bir artar ve yayınlanır. */
    emniyetRevArtir(): void;
    /** Alınan durum değiştiren isteklerin kaydı (yöntem, yol, başlıklar, gövde, yanıt). */
    readonly istekKaydi: { yontem: string; yol: string; istemci: string | null; govde: unknown; http: number; yanit: unknown }[];
  };
}

const YAYIN_TIKI_HZ = 50;
const GECERLI_HZ = new Set([50, 25, 10, 5, 2, 1]);
const OLAY_KUYRUGU = 4096;
const OLAY_GECMISI = 65536;
const ILETI_AZAMI = 1 << 20;
/** Soket yazma tamponunda bekleyen bu kadar bayttan fazlası varsa bağlantı "basınçlı" sayılır (tahmin). */
const GONDERIM_ESIGI = 1 << 20;
const TIP: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.map': 'application/json',
};

interface SonDeger {
  kanal: KanalTanimi;
  anahtar: string;
  uretimNo: number;
  t_ns: number;
  sira: number;
  yuk: Uint8Array;
}

interface KuyrukCercevesi {
  kanal: KanalTanimi;
  anahtar: string;
  t_ns: number;
  sira: number;
  yuk: Uint8Array;
}

interface Abone {
  hz: number | null;
}

interface Baglanti {
  id: number;
  ws: WebSocket;
  abonelik: Map<string, Abone>;
  gonderilen: Map<string, number>; // kanal|anahtar -> uretimNo
  diziKuyrugu: Map<string, KuyrukCercevesi[]>; // kanal -> kuyruk
  dusmeIsareti: Set<string>; // kanal|anahtar
  olayKuyrugu: Uint8Array[];
  ezilen: number;
  dusenDizi: number;
  sonCanlilik: number;
  kapaniyor: boolean;
}

interface OlayKaydi {
  sira: number;
  t_ns: number;
  cerceve: Uint8Array;
  json: Record<string, unknown>;
}

// ---- Sahte senaryo dosyaları ve A5 benzeri doğrulama (yalnız geliştirme) ----

const SENARYO_DOSYALARI: Record<string, string> = {
  'z1.toml': `# Z1: yüzeyde silahlanma, 3 m dalış, dört yol noktası, yüzeye çıkış (SAHTE içerik)
[meta]
ad = "z1"
aciklama = "Sahte sunucunun kinematik senaryosu"

[arac]
kutle = 18.5

[ortam]
derinlik = 7.0

[[gorev]]
tur = "DIVE_TO"
derinlik = 3.0
`,
  'dalis-yuzey.toml': `# Dalış ve yüzeye çıkış (SAHTE içerik)
[meta]
ad = "dalis-yuzey"

[arac]
kutle = 18.5

[emniyet_girdileri.pim]
f1 = "cikarilmis"

[ortam]
derinlik = 7.0
`,
  'hatali.toml': `# Bilerek hatalı: A5 doğrulama raporu örneği (SAHTE içerik)
[meta]
ad = "hatali"

[arac]
kutle = -3.0

[ortam]
derinlik = 7.0
`,
};

function sha256Hex(m: string | Buffer): string {
  return createHash('sha256').update(m).digest('hex');
}

function senaryoBaytlariUret(ad: string): Buffer {
  const s = SahteDunya.senaryo();
  (s.meta as Record<string, unknown>).ad = ad.replace(/\.toml$/, '');
  return Buffer.from(JSON.stringify(s));
}

interface A5Bulgu {
  kural: string;
  onem: 'hata' | 'uyari';
  asama: number;
  dosya: string;
  yol: string;
  satir: number | null;
  ileti: string;
}

function a5Raporu(ad: string, icerik: string): { gecerli: boolean; davranis_ozeti: string | null; kaynak_ozetleri: Record<string, string>; bulgular: A5Bulgu[] } {
  const bulgular: A5Bulgu[] = [];
  icerik.split('\n').forEach((satir, i) => {
    if (/^\s*kutle\s*=\s*-/.test(satir)) {
      bulgular.push({ kural: 'A5-K10', onem: 'hata', asama: 2, dosya: ad, yol: 'arac.kutle', satir: i + 1, ileti: 'kütle pozitif olmalı (sahte doğrulayıcı)' });
    }
    if (/^\s*f1\s*=\s*"cikarilmis"/.test(satir)) {
      bulgular.push({ kural: 'A5-K71', onem: 'uyari', asama: 3, dosya: ad, yol: 'emniyet_girdileri.pim.f1', satir: i + 1, ileti: 'emniyeti azaltan başlangıç durumu (pim çıkarılmış)' });
    }
  });
  const gecerli = !bulgular.some((b) => b.onem === 'hata');
  return {
    gecerli,
    davranis_ozeti: gecerli ? sha256Hex(senaryoBaytlariUret(ad)) : null,
    kaynak_ozetleri: { [ad]: sha256Hex(icerik) },
    bulgular,
  };
}

/** Tablo 4.7a: komut → komutun geçerli olduğu durumlar (sözleşmeden bağımsız yazıldı; testler bunu da sınar). */
const KOMUT_DURUMLARI: Record<string, readonly string[]> = {
  baslat: ['yuklendi'],
  duraklat: ['kosuyor'],
  surdur: ['duraklatildi'],
  adimla: ['duraklatildi'],
  hiz: ['kosuyor', 'duraklatildi'],
  durdur: ['hazirlaniyor', 'kosuyor', 'duraklatildi'],
  sifirla: ['yuklendi', 'hazirlaniyor', 'kosuyor', 'duraklatildi', 'bitti', 'durduruldu', 'hata'],
  yukle: ['bos', 'yuklendi', 'bitti', 'durduruldu', 'hata'],
};
const KOMUT_SEMASI: Record<string, string> = {
  baslat: 'istek_baslat',
  duraklat: 'istek_duraklat',
  surdur: 'istek_calistirma',
  adimla: 'istek_adimla',
  hiz: 'istek_hiz',
  durdur: 'istek_calistirma',
  sifirla: 'istek_sifirla',
  yukle: 'istek_senaryo_yukle',
};

function ret(res: ServerResponse, http: number, kod: string, ileti: string, ayrinti?: Record<string, unknown>, komutNo: number | null = null): void {
  const govde = JSON.stringify({ sonuc: 'ret', komut_no: komutNo, hata: ayrinti ? { kod, ileti, ayrinti } : { kod, ileti } });
  res.writeHead(http, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
  res.end(govde);
}

function json(res: ServerResponse, http: number, nesne: unknown, ek: Record<string, string> = {}): void {
  res.writeHead(http, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', ...ek });
  res.end(JSON.stringify(nesne));
}

function sabitSureliEsit(a: string, b: string): boolean {
  const x = Buffer.from(a);
  const y = Buffer.from(b);
  if (x.length !== y.length) {
    timingSafeEqual(y, y);
    return false;
  }
  return timingSafeEqual(x, y);
}

function degerAdi(yuk: keyof typeof YUKLER, alan: string, kod: number): string {
  const a = (YUKLER[yuk] as readonly { ad: string; degerler?: Readonly<Record<number, string>> }[]).find((x) => x.ad === alan);
  return a?.degerler?.[kod] ?? String(kod);
}

export async function sahteSunucuBaslat(ayar: SahteSunucuAyari): Promise<SahteSunucu> {
  const katalogBaytlari = readFileSync(ayar.katalogYolu);
  const katalogOzeti = 'sha256:' + createHash('sha256').update(katalogBaytlari).digest('hex');
  const jeton = ayar.jeton ?? randomBytes(32).toString('hex');
  if (!/^[0-9a-f]{64}$/.test(jeton)) throw new Error('jeton 64 küçük onaltılık karakter olmalı');
  const statik = ayar.statik ? resolve(ayar.statik) : null;
  const dunya = new SahteDunya({
    kip: ayar.kip ?? 'sil',
    emniyetKipi: ayar.emniyetKipi ?? 'olagan',
    elle: ayar.elle ?? false,
    ...(ayar.tohum !== undefined ? { tohum: ayar.tohum } : {}),
  });
  if (ayar.atla !== undefined && ayar.atla > 0) dunya.atla(ayar.atla);
  const sema = semaOku(katalogBaytlari.toString('utf8'));
  let senaryoBaytlari = senaryoBaytlariUret(dunya.senaryoAdi);
  let senaryoOzeti = 'sha256:' + createHash('sha256').update(senaryoBaytlari).digest('hex');

  let port = 0;
  const izinliHost = (): Set<string> => new Set([`127.0.0.1:${port}`, `localhost:${port}`, `[::1]:${port}`]);
  const izinliOrigin = (): Set<string> =>
    new Set([`http://127.0.0.1:${port}`, `http://localhost:${port}`, `http://[::1]:${port}`]);

  // ---- Durum ----
  const son = new Map<string, SonDeger>();
  const uretimSayaci = new Map<string, number>();
  const baglantilar = new Set<Baglanti>();
  const olayGecmisi: OlayKaydi[] = [];
  let olaySira = 0;
  let baglantiNo = 0;
  let istemciNo = 0;
  const istemciler = new Set<number>();
  let tik = 0;
  const istekKaydi: SahteSunucu['test']['istekKaydi'] = [];
  let komutNo = 0;
  let komutTutuldu = false;
  const komutSonuclari = new Map<number, { durum: 'kuyrukta' | 'uygulandi' | 'reddedildi'; uygulama_adimi?: number; hata?: Record<string, unknown> }>();
  interface BekleyenKomut {
    no: number;
    istemci: number;
    komut: string;
    govde: Record<string, unknown>;
  }
  const komutKuyrugu: BekleyenKomut[] = [];
  // ---- Emniyet ve yer istasyonu (W5-web-emniyet): onay jetonları, A3-K91…K95, K100…K104, K118 ----
  interface JetonKaydi {
    istemci: number;
    run_id: string | null;
    rev: number;
    islem: string;
    alindi: number;
  }
  const jetonlar = new Map<string, JetonKaydi>();
  const jetonOmru = ayar.jetonOmruMs ?? 10_000;
  const kanonik = (v: unknown): string => {
    if (Array.isArray(v)) return `[${v.map(kanonik).join(',')}]`;
    if (v !== null && typeof v === 'object') {
      const o = v as Record<string, unknown>;
      return `{${Object.keys(o).sort().map((k) => `${JSON.stringify(k)}:${kanonik(o[k])}`).join(',')}}`;
    }
    return JSON.stringify(v);
  };
  interface KomutTanimiSahte {
    ad: string;
    sinif: 'daraltici' | 'genisletici' | 'notr';
    onay: boolean;
    parametreler: Record<string, unknown>;
    sonuc: unknown;
    bilesik: boolean;
  }
  const yerKatalogu: { surum: string; komutlar: KomutTanimiSahte[] } = (() => {
    try {
      const yol = join(dirname(ayar.katalogYolu), '..', '..', '..', 'protocol', 'komutlar.json');
      return JSON.parse(readFileSync(yol, 'utf8')) as { surum: string; komutlar: KomutTanimiSahte[] };
    } catch {
      const bos = { type: 'object', additionalProperties: false, properties: {}, required: [] };
      return {
        surum: '1.0',
        komutlar: [
          { ad: 'arm', sinif: 'genisletici', onay: true, parametreler: bos, sonuc: null, bilesik: false },
          { ad: 'fc_disarm', sinif: 'daraltici', onay: false, parametreler: bos, sonuc: null, bilesik: false },
          { ad: 'fc_fire', sinif: 'genisletici', onay: true, parametreler: { type: 'object', additionalProperties: false, properties: { tube: { type: 'integer', minimum: 0, maximum: 1 } }, required: ['tube'] }, sonuc: null, bilesik: false },
        ],
      };
    }
  })();
  /** Yineleme anahtarı (A3-K53): istemci|yöntem|yol|istek_kimligi -> gövde + yanıt. */
  const yinelemeler = new Map<string, { govde: string; http: number; yanit: unknown }>();

  function anahtarOf(u: Uretim): string {
    return `${u.kanal}|${u.anahtar}`;
  }

  function uretimiIsle(u: Uretim): void {
    const kanal = kanalBul(u.kanal);
    if (kanal === undefined) return;
    const k = anahtarOf(u);
    const no = (uretimSayaci.get(k) ?? 0) + 1;
    uretimSayaci.set(k, no);
    const yuk = yukKodla(kanal.yuk, u.yuk as never);
    if (kanal.sinif === 'durum') {
      son.set(k, { kanal, anahtar: u.anahtar, uretimNo: no, t_ns: u.t_ns, sira: no >>> 0, yuk });
      return;
    }
    // dizi sınıfı: abone her bağlantının kuyruğuna (A3-K34, K41)
    for (const b of baglantilar) {
      if (!b.abonelik.has(kanal.ad)) continue;
      let q = b.diziKuyrugu.get(kanal.ad);
      if (q === undefined) {
        q = [];
        b.diziKuyrugu.set(kanal.ad, q);
      }
      q.push({ kanal, anahtar: u.anahtar, t_ns: u.t_ns, sira: no >>> 0, yuk });
      const sinir = kanal.kuyruk_siniri ?? 256;
      while (q.length > sinir) {
        const dusen = q.shift()!;
        b.dusenDizi++;
        b.dusmeIsareti.add(`${dusen.kanal.ad}|${dusen.anahtar}`);
      }
    }
  }

  function olayiIsle(o: { tur: number; onem: number; kaynak: number; adim_no: number | null; ayrinti: Record<string, unknown>; komut_no?: number; istemci_id?: number }): void {
    const kanal = kanalBul('olay')!;
    const sira = ++olaySira;
    const t_ns = dunya.t_ns;
    const yuk = yukKodla('yuk_olay', {
      tur: o.tur,
      onem: o.onem,
      kaynak: o.kaynak,
      adim_no: o.adim_no,
      komut_no: o.komut_no ?? 0,
      istemci_id: o.istemci_id ?? 0,
      ayrinti: JSON.stringify(o.ayrinti),
    });
    const cerceve = cerceveKodla(kanal, { t_ns, sira }, yuk);
    const tur = OLAY_TURLERI.find((x) => x.kod === o.tur);
    olayGecmisi.push({
      sira,
      t_ns,
      cerceve,
      json: {
        sira,
        t_ns,
        tur: tur?.ad ?? `tur_${o.tur}`,
        onem: degerAdi('yuk_olay', 'onem', o.onem),
        kaynak: degerAdi('yuk_olay', 'kaynak', o.kaynak),
        adim_no: o.adim_no,
        komut_no: o.komut_no ?? 0,
        istemci_id: o.istemci_id ?? 0,
        ayrinti: o.ayrinti,
      },
    });
    if (olayGecmisi.length > OLAY_GECMISI) olayGecmisi.shift();
    for (const b of baglantilar) {
      if (b.kapaniyor) continue;
      b.olayKuyrugu.push(cerceve);
      if (b.olayKuyrugu.length > OLAY_KUYRUGU) {
        // A3-K42: olay ezilmez; kuyruk taşınca bağlantı 4001 ile kapanır.
        b.kapaniyor = true;
        b.ws.close(4001, 'olay kuyrugu tasti');
      }
    }
  }

  function baglantiDurumYuku(b: Baglanti, temel: Uint8Array): Uint8Array {
    // Bağlantıya özgü üç alan (A3-K40, K41) bağlantı başına doldurulup yeniden kodlanır.
    const d = dunyaSonBaglantiDurumu;
    if (d === null) return temel;
    const y: YukBaglantiDurum = {
      ...d,
      bu_baglanti_ezilen_durum: b.ezilen,
      bu_baglanti_dusen_dizi: b.dusenDizi,
      bu_baglanti_olay_kuyrugu: b.olayKuyrugu.length,
    };
    return yukKodla('yuk_baglanti_durum', y);
  }
  let dunyaSonBaglantiDurumu: YukBaglantiDurum | null = null;

  function durumGonderilecekMi(b: Baglanti, kanal: KanalTanimi): boolean {
    if (kanal.zorunlu_abonelik) {
      const hz = kanal.varsayilan_hz ?? 10;
      return tik % Math.max(1, Math.round(YAYIN_TIKI_HZ / hz)) === 0;
    }
    const a = b.abonelik.get(kanal.ad);
    if (a === undefined || a.hz === null) return false;
    return tik % Math.max(1, Math.round(YAYIN_TIKI_HZ / a.hz)) === 0;
  }

  function gonder(b: Baglanti, cerceveler: Uint8Array[]): void {
    if (cerceveler.length === 0 || b.ws.readyState !== 1) return;
    let parca: Uint8Array[] = [];
    let boy = 0;
    for (const c of cerceveler) {
      if (boy + c.length > ILETI_AZAMI && parca.length > 0) {
        b.ws.send(iletiBirlestir(parca), { binary: true });
        parca = [];
        boy = 0;
      }
      parca.push(c);
      boy += c.length;
    }
    if (parca.length > 0) b.ws.send(iletiBirlestir(parca), { binary: true });
  }

  function ozetGonder(b: Baglanti, kanalAdlari: Iterable<string>): void {
    const out: Uint8Array[] = [];
    const istenen = new Set(kanalAdlari);
    for (const [k, s] of son) {
      if (!istenen.has(s.kanal.ad)) continue;
      const yuk = s.kanal.ad === 'baglanti/durum' ? baglantiDurumYuku(b, s.yuk) : s.yuk;
      let bayrak = BAYRAK.OZET;
      if (s.kanal.yer_gercegi) bayrak |= BAYRAK.YER_GERCEGI;
      out.push(cerceveKodla(s.kanal, { t_ns: s.t_ns, sira: s.sira, bayraklar: bayrak }, yuk));
      b.gonderilen.set(k, s.uretimNo);
    }
    gonder(b, out);
  }

  /**
   * A3-K39: gönderim basıncı. Soket henüz yazamadıysa (ws.bufferedAmount eşiği aşıyorsa)
   * bu tikte bu bağlantıya HİÇBİR sınıftan veri verilmez; olay ve dizi çerçeveleri
   * sınırlı uygulama kuyruklarında kalır (dizi: en eski düşer, K41; olay: 4096'yı
   * aşınca 4001, K42); durum yuvası ezilir (K40).
   */
  function basincli(b: Baglanti): boolean {
    return ayar.basinc !== undefined ? ayar.basinc(b.id) : b.ws.bufferedAmount > GONDERIM_ESIGI;
  }

  function yayinTiki(): void {
    tik++;
    for (const b of baglantilar) {
      if (b.kapaniyor || b.ws.readyState !== 1) continue;
      if (basincli(b)) continue;
      const out: Uint8Array[] = [];
      // olaylar (kayıpsız, sıralı)
      for (const c of b.olayKuyrugu) out.push(c);
      b.olayKuyrugu = [];
      // dizi kuyrukları
      for (const [, q] of b.diziKuyrugu) {
        for (const c of q) {
          const ka = `${c.kanal.ad}|${c.anahtar}`;
          let bayrak = c.kanal.yer_gercegi ? BAYRAK.YER_GERCEGI : 0;
          if (b.dusmeIsareti.delete(ka)) bayrak |= BAYRAK.ONCESINDE_DUSME;
          out.push(cerceveKodla(c.kanal, { t_ns: c.t_ns, sira: c.sira, bayraklar: bayrak }, c.yuk));
        }
        q.length = 0;
      }
      // durum sınıfı (A3-K33: her 50/h tikte, yeni değer varsa yalnız sonuncusu)
      for (const [k, s] of son) {
        if (!durumGonderilecekMi(b, s.kanal)) continue;
        const onceki = b.gonderilen.get(k) ?? 0;
        if (s.uretimNo <= onceki) continue;
        if (onceki > 0 && s.uretimNo - onceki > 1) b.ezilen += s.uretimNo - onceki - 1;
        b.gonderilen.set(k, s.uretimNo);
        const yuk = s.kanal.ad === 'baglanti/durum' ? baglantiDurumYuku(b, s.yuk) : s.yuk;
        const bayrak = s.kanal.yer_gercegi ? BAYRAK.YER_GERCEGI : 0;
        out.push(cerceveKodla(s.kanal, { t_ns: s.t_ns, sira: s.sira, bayraklar: bayrak }, yuk));
      }
      gonder(b, out);
    }
  }

  // ---- Dünya döngüsü ----
  let sonDuvar = performance.now();
  const tikMs = ayar.tikMs ?? 20;
  const dongu = setInterval(() => {
    const simdi = performance.now();
    const gecen = Math.min(250, simdi - sonDuvar);
    sonDuvar = simdi;
    if (ayar.dunyaDurdur !== true) {
      if (!komutTutuldu) komutlariUygula();
      for (const u of dunya.ilerle(gecen)) {
        if (u.kanal === 'baglanti/durum') dunyaSonBaglantiDurumu = u.yuk as YukBaglantiDurum;
        uretimiIsle(u);
      }
      for (const o of dunya.olayKuyrugu.splice(0)) olayiIsle(o);
    }
    yayinTiki();
  }, tikMs);

  const canlilik = setInterval(() => {
    const simdi = Date.now();
    for (const b of baglantilar) {
      if (simdi - b.sonCanlilik > 15_000) {
        b.kapaniyor = true;
        b.ws.close(4004, 'canlilik yok');
        continue;
      }
      if (b.ws.readyState === 1) b.ws.ping();
    }
  }, 5000);

  // ---- WebSocket ----
  const wss = new WebSocketServer({
    noServer: true,
    maxPayload: 64 * 1024, // A3-K47: büyük ileti 1009
    handleProtocols: (protokoller) => (protokoller.has(WS_ALT_PROTOKOLU) ? WS_ALT_PROTOKOLU : false),
  });

  function metinIletisi(b: Baglanti, veri: string): void {
    let ileti: unknown;
    try {
      ileti = JSON.parse(veri);
    } catch {
      b.ws.send(JSON.stringify({ tur: 'hata', istek_no: null, kod: 'gecersiz_ileti', ileti: 'JSON çözülemedi' }));
      return;
    }
    // A3-K48: şemaya uymayan ileti `hata` alır, bağlantı açık kalır. Özelliklere
    // erişmeden önce kök nesne ve her öğe doğrulanır.
    const nesneMi = (x: unknown): x is Record<string, unknown> => typeof x === 'object' && x !== null && !Array.isArray(x);
    const sema = (istekNo: number | null, ne: string): void => {
      b.ws.send(JSON.stringify({ tur: 'hata', istek_no: istekNo, kod: 'gecersiz_ileti', ileti: `şemaya uymuyor: ${ne}` }));
    };
    if (!nesneMi(ileti)) return sema(null, 'kök JSON nesnesi değil');
    const n = ileti;
    const istekNo = typeof n['istek_no'] === 'number' && Number.isInteger(n['istek_no']) && n['istek_no'] >= 1 ? n['istek_no'] : null;
    if (n['tur'] !== 'abone_ol' && n['tur'] !== 'abonelik_birak') {
      b.ws.send(JSON.stringify({ tur: 'hata', istek_no: istekNo, kod: 'bilinmeyen_tur', ileti: 'bilinmeyen ileti türü' }));
      return;
    }
    const liste = n['kanallar'];
    if (istekNo === null || !Array.isArray(liste) || liste.length < 1 || liste.length > 64) return sema(istekNo, 'istek_no ya da kanallar');
    if (n['tur'] === 'abone_ol') {
      for (const oge of liste as unknown[]) {
        if (!nesneMi(oge) || typeof oge['kanal'] !== 'string' || oge['kanal'].length > 64) return sema(istekNo, 'kanallar[] öğesi');
        for (const k of Object.keys(oge)) if (k !== 'kanal' && k !== 'hz') return sema(istekNo, `bilinmeyen alan ${k}`);
      }
    } else {
      for (const oge of liste as unknown[]) if (typeof oge !== 'string' || oge.length > 64) return sema(istekNo, 'kanallar[] öğesi');
    }
    const yanit: Record<string, unknown>[] = [];
    const yeniDurum: string[] = [];
    if (n['tur'] === 'abone_ol') {
      for (const oge of liste as unknown[]) {
        const o = oge as { kanal?: unknown; hz?: unknown };
        const ad = typeof o.kanal === 'string' ? o.kanal : '';
        const kanal = kanalBul(ad);
        if (kanal === undefined) {
          yanit.push({ kanal: ad, sonuc: 'ret', neden: 'bilinmeyen_kanal' });
          continue;
        }
        if (o.hz !== undefined && (typeof o.hz !== 'number' || !GECERLI_HZ.has(o.hz))) {
          yanit.push({ kanal: ad, sonuc: 'ret', neden: 'gecersiz_hz' });
          continue;
        }
        let etkin: number | null = null;
        if (kanal.sinif === 'durum') {
          const istenen = typeof o.hz === 'number' ? o.hz : (kanal.varsayilan_hz ?? 10);
          etkin = Math.min(istenen, kanal.azami_hz ?? istenen, YAYIN_TIKI_HZ); // A3-K32
        }
        if (!kanal.zorunlu_abonelik) {
          b.abonelik.set(kanal.ad, { hz: etkin });
          if (kanal.sinif === 'durum') yeniDurum.push(kanal.ad);
        }
        yanit.push({ kanal: ad, kanal_id: kanal.kanal_id, kanal_surumu: kanal.kanal_surumu, etkin_hz: etkin, sonuc: 'kabul' });
      }
    } else {
      for (const oge of liste as unknown[]) {
        const ad = typeof oge === 'string' ? oge : '';
        const kanal = kanalBul(ad);
        if (kanal === undefined) yanit.push({ kanal: ad, sonuc: 'ret', neden: 'bilinmeyen_kanal' });
        else if (kanal.zorunlu_abonelik) yanit.push({ kanal: ad, sonuc: 'ret', neden: 'zorunlu_kanal' });
        else {
          b.abonelik.delete(kanal.ad);
          b.diziKuyrugu.delete(kanal.ad);
          for (const k of [...b.gonderilen.keys()]) if (k.startsWith(kanal.ad + '|')) b.gonderilen.delete(k);
          yanit.push({ kanal: ad, kanal_id: kanal.kanal_id, kanal_surumu: kanal.kanal_surumu, sonuc: 'kabul' });
        }
      }
    }
    b.ws.send(JSON.stringify({ tur: 'abonelik_yaniti', istek_no: istekNo, kanallar: yanit }));
    if (yeniDurum.length > 0) ozetGonder(b, yeniDurum); // A3-K35
  }

  wss.on('connection', (ws: WebSocket) => {
    const b: Baglanti = {
      id: ++baglantiNo,
      ws,
      abonelik: new Map(),
      gonderilen: new Map(),
      diziKuyrugu: new Map(),
      dusmeIsareti: new Set(),
      olayKuyrugu: [],
      ezilen: 0,
      dusenDizi: 0,
      sonCanlilik: Date.now(),
      kapaniyor: false,
    };
    baglantilar.add(b);
    const oz = dunya.ozet();
    ws.send(
      JSON.stringify({
        tur: 'merhaba',
        a3_surumu: A3_SURUMU,
        katalog_ozeti: katalogOzeti,
        baglanti_id: b.id,
        run_id: oz.run_id,
        son_olay_sirasi: olaySira,
        yayin_tavani_hz: YAYIN_TIKI_HZ,
      }),
    );
    ozetGonder(
      b,
      KANALLAR.filter((k) => k.zorunlu_abonelik && k.sinif === 'durum').map((k) => k.ad),
    );
    ws.on('pong', () => {
      b.sonCanlilik = Date.now();
    });
    ws.on('message', (veri: RawData, ikili: boolean) => {
      b.sonCanlilik = Date.now();
      if (ikili) {
        b.kapaniyor = true;
        ws.close(1003, 'istemciden ikili ileti'); // A3-K47
        return;
      }
      metinIletisi(b, veri.toString());
    });
    ws.on('close', () => {
      baglantilar.delete(b);
    });
    ws.on('error', () => {
      baglantilar.delete(b);
    });
  });

  // ---- HTTP ----
  function erisimDenetle(req: IncomingMessage): string | null {
    const host = req.headers.host ?? '';
    if (!izinliHost().has(host)) return 'Host izinli değil';
    const origin = req.headers.origin;
    if (origin !== undefined && !izinliOrigin().has(origin)) return 'Origin izinli değil';
    return null;
  }

  function jetonDenetle(req: IncomingMessage): boolean {
    const a = req.headers.authorization ?? '';
    const m = /^Bearer ([0-9a-f]{64})$/.exec(a);
    return m !== null && sabitSureliEsit(m[1]!, jeton);
  }

  function govdeOku(req: IncomingMessage, sinir: number): Promise<Buffer | null> {
    return new Promise((coz) => {
      const parcalar: Buffer[] = [];
      let boy = 0;
      let asti = false;
      req.on('data', (p: Buffer) => {
        boy += p.length;
        if (boy > sinir) {
          asti = true;
          return;
        }
        parcalar.push(p);
      });
      req.on('end', () => coz(asti ? null : Buffer.concat(parcalar)));
      req.on('error', () => coz(null));
    });
  }

  function statikSun(yol: string, res: ServerResponse): void {
    if (statik === null) {
      res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
      res.end('Panel dosyaları derlenmedi: web/ dizininde `npm run derle` çalıştırın.\n');
      return;
    }
    const goreli = yol === '/' ? 'index.html' : yol.slice(1);
    const tam = normalize(join(statik, goreli));
    if (!tam.startsWith(statik + sep)) return ret(res, 404, 'bulunamadi', 'yok');
    try {
      const st = statSync(tam);
      if (!st.isFile()) return ret(res, 404, 'bulunamadi', 'yok');
      res.writeHead(200, { 'Content-Type': TIP[extname(tam)] ?? 'application/octet-stream', 'Cache-Control': 'no-store' });
      res.end(readFileSync(tam));
    } catch {
      ret(res, 404, 'bulunamadi', 'yok');
    }
  }

  async function api(req: IncomingMessage, res: ServerResponse, url: URL): Promise<void> {
    if (!jetonDenetle(req)) return ret(res, 403, 'erisim_reddedildi', 'jeton yok ya da yanlış');
    const yol = url.pathname.slice('/api/v1'.length);
    const y = req.method ?? 'GET';
    const oz = dunya.ozet();
    if (y === 'GET' && yol === '/surum') {
      return json(res, 200, {
        sonuc: 'kabul',
        a3_surumu: A3_SURUMU,
        katalog_ozeti: katalogOzeti,
        ws_alt_protokolu: WS_ALT_PROTOKOLU,
        a6_surumu: '1.0',
        sim_surumu: 'sahte-a3 0.1 (W3-web; sahte veri, fizik değil)',
        platform: process.platform === 'win32' ? 'windows-x64' : 'macos-arm64',
      });
    }
    if (y === 'GET' && yol === '/katalog') {
      res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
      res.end(katalogBaytlari);
      return;
    }
    if (y === 'POST' && yol === '/istemciler') {
      const g = await govdeOku(req, 1 << 20);
      if (g === null) return ret(res, 413, 'govde_cok_buyuk', 'gövde sınırdan büyük');
      if ((req.headers['content-type'] ?? '').split(';')[0]!.trim() !== 'application/json') {
        return ret(res, 415, 'icerik_turu', 'Content-Type application/json değil');
      }
      try {
        const n = g.length > 0 ? (JSON.parse(g.toString('utf8')) as unknown) : {};
        if (n === null || typeof n !== 'object' || Array.isArray(n)) throw new Error();
      } catch {
        return ret(res, 400, 'gecersiz_istek', 'JSON çözülemedi');
      }
      const id = ++istemciNo;
      istemciler.add(id);
      return json(res, 200, { sonuc: 'kabul', istemci_id: id });
    }
    const silme = /^\/istemciler\/(\d+)$/.exec(yol);
    if (y === 'DELETE' && silme !== null) {
      if (!istemciler.delete(Number(silme[1]))) return ret(res, 404, 'bulunamadi', 'istemci yok');
      return json(res, 200, { sonuc: 'kabul' });
    }
    if (y === 'GET' && yol === '/durum') {
      return json(res, 200, {
        sonuc: 'kabul',
        durum: oz.durum,
        kip: dunya.kip,
        run_id: oz.run_id,
        sim_t_ns: oz.t_ns,
        adim_no: oz.adim_no,
        gecerli: true,
        gercek_zamani_kacirdi: false,
        gecerlilik_nedenleri: [],
        sayaclar: { kacirilan_adim: 0, parti_kacirildi: 0 },
        kademeler: { gorunum: 'sade', kamera: 'kapali', sonar: 'sade', kayit: 'telemetri' },
        yayin_tavani_hz: YAYIN_TIKI_HZ,
        senaryo_ozeti: senaryoOzeti,
        emniyet_rev: oz.emniyet_rev,
        a1_run_id: null,
        durum_rev: oz.durum_rev,
        son_komut_no: komutNo,
      });
    }
    if (y === 'GET' && yol === '/olaylar') {
      const sonra = Number(url.searchParams.get('sonra') ?? 'NaN');
      const enCok = Number(url.searchParams.get('en_cok') ?? '1000');
      if (!Number.isInteger(sonra) || sonra < 0 || !Number.isInteger(enCok) || enCok < 1 || enCok > 1000) {
        return ret(res, 422, 'gecersiz_deger', 'sonra ≥ 0, 1 ≤ en_cok ≤ 1000');
      }
      const enEski = olayGecmisi[0]?.sira ?? olaySira + 1;
      if (sonra < enEski - 1 && sonra < olaySira) return ret(res, 410, 'gecmis_yok', 'istenen olaylar bellekte yok');
      const secilen = olayGecmisi.filter((o) => o.sira > sonra).slice(0, enCok);
      const tamam = secilen.length === 0 || secilen.at(-1)!.sira === olaySira;
      return json(res, 200, { sonuc: 'kabul', olaylar: secilen.map((o) => o.json), tamam });
    }
    if (y === 'GET' && (yol === '/on_denetim' || yol.startsWith('/on_denetim/'))) {
      const kimlik = dunya.raporKimligi;
      if (kimlik === '' || (yol !== '/on_denetim' && yol !== `/on_denetim/${kimlik}`)) {
        return ret(res, 404, 'bulunamadi', 'rapor yok');
      }
      return json(res, 200, {
        rapor_surumu: '1.0',
        rapor_kimligi: kimlik,
        sonuc: 'gecti',
        kip: dunya.kip,
        olusturma_utc: new Date().toISOString(),
        maddeler: ONDENETIM_MADDELERI(dunya.kip),
      });
    }
    if (y === 'GET' && yol === '/senaryo/cozumlenmis') {
      res.writeHead(200, {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-store',
        'Ida-Davranis-Ozeti': senaryoOzeti,
      });
      res.end(senaryoBaytlari);
      return;
    }
    if (y === 'GET' && yol === '/emniyet') {
      const e = dunya.yetkiliEmniyet();
      const ad = (v: number | undefined): string => (v === 1 ? 'takili' : 'cikarilmis');
      return json(res, 200, {
        sonuc: 'kabul',
        yetkili: {
          acil_durdurma: e.acil === 1 ? 'basili' : 'serbest',
          silahlandirma_fisi: { f1: ad(e.fis[0]), f2: ad(e.fis[1]) },
          emniyet_pimi: { f1: ad(e.pim[0]), f2: ad(e.pim[1]) },
        },
        emniyet_rev_doydu: false,
        son_degisim_adimi: e.sonAdim,
        emniyet_rev: oz.emniyet_rev,
      });
    }
    if (y === 'GET' && yol === '/yer_istasyonu/komutlar') return json(res, 200, yerKatalogu);
    if (y === 'POST' && (yol === '/emniyet/onay_jetonu' || yol === '/yer_istasyonu/onay_jetonu')) return jetonIsle(req, res, yol);
    if (y === 'POST' && (yol === '/emniyet/degistir' || yol === '/yer_istasyonu/komut')) return emniyetKomutIsle(req, res, yol);
    if (y === 'GET' && yol === '/arizalar/etkin') {
      const sinif = ['sensor', 'aktuator', 'guc', 'baglanti', 'protokol', 'sistem'];
      return json(res, 200, {
        sonuc: 'kabul',
        arizalar: dunya.etkinArizalar().map((f) => ({
          fault_id: f.fault_id,
          sinif: sinif[f.sinif] ?? 'sistem',
          hedef: f.hedef,
          tur: f.tur,
          kaynak: f.kaynak === 0 ? 'senaryo' : 'elle',
          baslangic_adimi: f.baslangic_adimi,
          bitis_adimi: f.bitis_adimi,
        })),
      });
    }

    // ---- Senaryo uçları (A3 §4.9) ----
    if (y === 'GET' && yol === '/senaryolar') {
      return json(res, 200, {
        sonuc: 'kabul',
        senaryolar: Object.entries(SENARYO_DOSYALARI).map(([ad, icerik]) => ({ ad, boyut_bayt: Buffer.byteLength(icerik), degisim_utc: '2026-11-25T09:00:00Z' })),
      });
    }
    const dosyaEs = /^\/senaryolar\/([^/]+)$/.exec(yol);
    if (y === 'GET' && dosyaEs !== null) {
      const ad = decodeURIComponent(dosyaEs[1]!);
      if (!/^[a-z0-9_-]{1,64}\.toml$/.test(ad)) return ret(res, 400, 'gecersiz_istek', 'senaryo adı kalıba uymuyor (A3-K78)');
      const icerik = SENARYO_DOSYALARI[ad];
      if (icerik === undefined) return ret(res, 404, 'bulunamadi', 'senaryo dosyası yok');
      return json(res, 200, { sonuc: 'kabul', ad, icerik });
    }
    if (y === 'POST' && yol === '/senaryo/dogrula') {
      const g = await jsonGovde(req, res, 4 << 20);
      if (g === null) return;
      const hata = semaDogrula(sema, sema.$defs.istek_senaryo_dogrula!, g);
      if (hata.length > 0) return ret(res, 400, 'gecersiz_istek', hata.join('; '));
      const b = g as { ad?: string; icerik?: string };
      const icerik = b.icerik ?? SENARYO_DOSYALARI[b.ad ?? ''];
      if (icerik === undefined) return ret(res, 404, 'bulunamadi', 'senaryo dosyası yok');
      const rapor = a5Raporu(b.ad ?? 'icerik', icerik);
      return json(res, 200, { sonuc: 'kabul', gecerli: rapor.gecerli, senaryo_ozeti: rapor.davranis_ozeti === null ? null : `sha256:${rapor.davranis_ozeti}`, rapor });
    }
    if (y === 'POST' && yol === '/senaryo/yukle') return komutIsle(req, res, 'yukle', yol);
    const calEs = /^\/calistirma\/([a-z_]+)$/.exec(yol);
    if (y === 'POST' && calEs !== null && ['baslat', 'duraklat', 'surdur', 'adimla', 'hiz', 'durdur', 'sifirla'].includes(calEs[1]!)) {
      return komutIsle(req, res, calEs[1]!, yol);
    }
    const komutEs = /^\/komutlar\/(\d+)$/.exec(yol);
    if (y === 'GET' && komutEs !== null) {
      const k = komutSonuclari.get(Number(komutEs[1]));
      if (k === undefined) return ret(res, 404, 'bulunamadi', 'komut yok');
      return json(res, 200, { sonuc: 'kabul', komut_no: Number(komutEs[1]), durum: k.durum, ...(k.uygulama_adimi !== undefined ? { uygulama_adimi: k.uygulama_adimi } : {}), ...(k.hata !== undefined ? { hata: k.hata } : {}) });
    }
    return ret(res, 404, 'bulunamadi', 'uç yok ya da sahte sunucu bu ucu uygulamıyor');
  }

  /** JSON gövde: içerik türü, boyut ve çözümleme denetimleri (A3-K51). Hata varsa yanıtı yazar, null döner. */
  async function jsonGovde(req: IncomingMessage, res: ServerResponse, sinir: number): Promise<unknown> {
    if ((req.headers['content-type'] ?? '').split(';')[0]!.trim() !== 'application/json') {
      ret(res, 415, 'icerik_turu', 'Content-Type application/json değil');
      return null;
    }
    const g = await govdeOku(req, sinir);
    if (g === null) {
      ret(res, 413, 'govde_cok_buyuk', 'gövde sınırdan büyük');
      return null;
    }
    try {
      return JSON.parse(g.toString('utf8')) as unknown;
    } catch {
      ret(res, 400, 'gecersiz_istek', 'JSON çözülemedi');
      return null;
    }
  }

  /**
   * Durum değiştiren uç (A3-K50…K57): kimlik, şema, yineleme, revizyon ve durum denetimleri eşzamanlı;
   * çalıştırma kontrolü komutları 202 ile kuyruğa girer ve sonuç `komut_sonucu` olayıyla gelir;
   * `yukle` senkron 200'dür.
   */
  async function komutIsle(req: IncomingMessage, res: ServerResponse, komut: string, yol: string): Promise<void> {
    const kayit = { yontem: 'POST', yol: '/api/v1' + yol, istemci: (req.headers['ida-istemci'] as string | undefined) ?? null, govde: null as unknown, http: 0, yanit: null as unknown };
    const yanitla = (http: number, nesne: unknown): void => {
      kayit.http = http;
      kayit.yanit = nesne;
      istekKaydi.push(kayit);
      json(res, http, nesne);
    };
    const reddet = (http: number, kod: string, ileti: string, ayrinti?: Record<string, unknown>, no: number | null = null): void =>
      yanitla(http, { sonuc: 'ret', komut_no: no, hata: ayrinti ? { kod, ileti, ayrinti } : { kod, ileti } });

    const istemciId = Number(kayit.istemci);
    if (kayit.istemci === null || !istemciler.has(istemciId)) {
      await govdeOku(req, 1 << 20);
      return reddet(400, 'gecersiz_istek', 'Ida-Istemci başlığı yok ya da istemci kayıtlı değil (A3-K50)');
    }
    const g = await jsonGovde(req, res, komut === 'yukle' ? 4 << 20 : 1 << 20);
    if (g === null) return;
    kayit.govde = g;
    const hata = semaDogrula(sema, sema.$defs[KOMUT_SEMASI[komut]!]!, g);
    if (hata.length > 0) return reddet(400, 'gecersiz_istek', hata.join('; '));
    const b = g as Record<string, unknown>;

    // Yineleme (A3-K53)
    const anahtar = `${istemciId}|POST|${yol}|${String(b.istek_kimligi)}`;
    const ham = JSON.stringify(g);
    const eski = yinelemeler.get(anahtar);
    if (eski !== undefined) {
      if (eski.govde !== ham) return reddet(409, 'kimlik_cakismasi', 'aynı istek kimliği farklı gövdeyle');
      return yanitla(eski.http, { ...(eski.yanit as Record<string, unknown>), yineleme: true });
    }
    const no = ++komutNo;
    const sonuc = (http: number, nesne: unknown): void => {
      yinelemeler.set(anahtar, { govde: ham, http, yanit: nesne });
      yanitla(http, nesne);
    };
    const red = (http: number, kod: string, ileti: string, ayrinti?: Record<string, unknown>): void =>
      sonuc(http, { sonuc: 'ret', komut_no: no, hata: ayrinti ? { kod, ileti, ayrinti } : { kod, ileti } });

    const oz = dunya.ozet();
    if (b.beklenen_run_id !== oz.run_id) {
      // Tablo 4.7a′: çalıştırma yokken null, varken güncel kimlik
      return red(409, 'beklenen_durum_uyusmuyor', 'beklenen_run_id güncel değil', { guncel_run_id: oz.run_id, guncel_durum: oz.durum, guncel_durum_rev: oz.durum_rev });
    }
    if (b.beklenen_durum_rev !== oz.durum_rev) {
      return red(409, 'durum_rev_uyusmuyor', 'beklenen_durum_rev güncel değil', { guncel_durum_rev: oz.durum_rev });
    }
    if (!KOMUT_DURUMLARI[komut]!.includes(oz.durum)) return red(409, 'gecersiz_durum', `komut ${oz.durum} durumunda geçerli değil`);
    const hil = dunya.kip === 'hil_1x';
    if (hil && (komut === 'adimla' || komut === 'hiz')) return red(409, 'kipte_gecersiz', 'HIL kipinde geçersiz komut');
    if (hil && komut === 'duraklat' && b.gecersizligi_onayla !== true) return red(409, 'onay_gerekli', 'HIL duraklatma gecersizligi_onayla ister');
    if (komut === 'hiz') {
      const c = b.carpan as number;
      if (c !== 0 && c < 0.01) return red(422, 'gecersiz_deger', 'carpan 0 ya da [0,01; 100] olmalı');
    }

    if (komut === 'yukle') {
      const ad = b.ad as string | undefined;
      const icerik = (b.icerik as string | undefined) ?? SENARYO_DOSYALARI[ad ?? ''];
      if (icerik === undefined) return red(404, 'bulunamadi', 'senaryo dosyası yok');
      const rapor = a5Raporu(ad ?? 'icerik', icerik);
      if (!rapor.gecerli) return red(422, 'senaryo_gecersiz', 'senaryo A5 doğrulamasından geçmedi', { rapor });
      const dosyaAdi = ad ?? 'icerik.toml';
      dunya.senaryoYukle(dosyaAdi);
      senaryoBaytlari = senaryoBaytlariUret(dosyaAdi);
      senaryoOzeti = 'sha256:' + sha256Hex(senaryoBaytlari);
      komutSonuclari.set(no, { durum: 'uygulandi', uygulama_adimi: dunya.adim_no });
      olayiIsle({ tur: 13, onem: 0, kaynak: 1, adim_no: null, komut_no: no, istemci_id: istemciId, ayrinti: { ad: dosyaAdi.replace(/\.toml$/, ''), senaryo_ozeti: senaryoOzeti } });
      return sonuc(200, { sonuc: 'kabul', komut_no: no, senaryo_ozeti: senaryoOzeti, rapor });
    }

    komutSonuclari.set(no, { durum: 'kuyrukta' });
    komutKuyrugu.push({ no, istemci: istemciId, komut, govde: b });
    return sonuc(202, { sonuc: 'kuyrukta', komut_no: no });
  }


  const CANLI = ['hazirlaniyor', 'kosuyor', 'duraklatildi'];
  const GIRDI_DEGERI: Record<string, number> = { basili: 1, serbest: 0, takili: 1, cikarilmis: 0 };
  const emniyetArtiranMi = (b: Record<string, unknown>): boolean =>
    b.girdi === 'acil_durdurma' ? b.hedef_durum === 'basili' : b.girdi === 'silahlandirma_fisi' ? b.hedef_durum === 'cikarilmis' : b.hedef_durum === 'takili';
  const islemOzeti = (yol: string, b: Record<string, unknown>): string =>
    yol.startsWith('/emniyet') ? `emniyet|${String(b.girdi)}|${String(b.tup ?? '')}|${String(b.hedef_durum)}` : `yer|${String(b.komut)}|${kanonik(b.parametreler)}`;

  /** Kimlik, gövde ve şema denetimleri (A3-K50, K51); hata varsa yanıtı yazar ve null döner. */
  async function onHazirlik(req: IncomingMessage, res: ServerResponse, yol: string, semaAdi: string) {
    const kayit = { yontem: 'POST', yol: '/api/v1' + yol, istemci: (req.headers['ida-istemci'] as string | undefined) ?? null, govde: null as unknown, http: 0, yanit: null as unknown };
    const yanitla = (http: number, nesne: unknown): void => {
      kayit.http = http;
      kayit.yanit = nesne;
      istekKaydi.push(kayit);
      json(res, http, nesne);
    };
    const reddet = (http: number, kod: string, ileti: string, ayrinti?: Record<string, unknown>, no: number | null = null): void =>
      yanitla(http, { sonuc: 'ret', komut_no: no, hata: ayrinti ? { kod, ileti, ayrinti } : { kod, ileti } });
    const istemciId = Number(kayit.istemci);
    if (kayit.istemci === null || !istemciler.has(istemciId)) {
      await govdeOku(req, 1 << 20);
      reddet(400, 'gecersiz_istek', 'Ida-Istemci başlığı yok ya da istemci kayıtlı değil (A3-K50)');
      return null;
    }
    const g = await jsonGovde(req, res, 1 << 20);
    if (g === null) return null;
    kayit.govde = g;
    const hata = semaDogrula(sema, sema.$defs[semaAdi]!, g);
    if (hata.length > 0) {
      reddet(400, 'gecersiz_istek', hata.join('; '));
      return null;
    }
    return { kayit, yanitla, reddet, istemciId, b: g as Record<string, unknown> };
  }

  function komutParametreHatasi(b: Record<string, unknown>): { http: number; kod: string; ileti: string } | null {
    const t = yerKatalogu.komutlar.find((k) => k.ad === b.komut);
    if (t === undefined) return { http: 422, kod: 'komut_listede_yok', ileti: 'komut katalogda yok (A3-K100)' };
    const h = semaDogrula(sema, t.parametreler, b.parametreler);
    return h.length > 0 ? { http: 422, kod: 'gecersiz_deger', ileti: h.join('; ') } : null;
  }

  /** `POST /emniyet/onay_jetonu` ve `POST /yer_istasyonu/onay_jetonu` (A3-K93, K94, K103). */
  async function jetonIsle(req: IncomingMessage, res: ServerResponse, yol: string): Promise<void> {
    const emn = yol.startsWith('/emniyet');
    const c = await onHazirlik(req, res, yol, emn ? 'istek_onay_jetonu_emniyet' : 'istek_onay_jetonu_yer_istasyonu');
    if (c === null) return;
    const oz = dunya.ozet();
    if (!CANLI.includes(oz.durum)) return c.reddet(409, 'canli_calistirma_yok', 'komut canlı bir çalıştırma gerektiriyor');
    if (c.b.beklenen_run_id !== oz.run_id) return c.reddet(409, 'beklenen_durum_uyusmuyor', 'beklenen_run_id güncel değil', { guncel_run_id: oz.run_id, guncel_durum: oz.durum });
    if (emn && emniyetArtiranMi(c.b)) return c.reddet(400, 'gecersiz_istek', 'emniyeti artıran yön onay jetonu istemez (A3-K92)');
    if (!emn) {
      const h = komutParametreHatasi(c.b);
      if (h !== null) return c.reddet(h.http, h.kod, h.ileti);
      const t = yerKatalogu.komutlar.find((k) => k.ad === c.b.komut)!;
      if (!t.onay) return c.reddet(400, 'gecersiz_istek', 'bu komut onay jetonu istemez (A3-K104)');
    }
    const kod = randomBytes(16).toString('hex');
    jetonlar.set(kod, { istemci: c.istemciId, run_id: oz.run_id, rev: oz.emniyet_rev, islem: islemOzeti(yol, c.b), alindi: Date.now() });
    return c.yanitla(200, { sonuc: 'kabul', onay_jetonu: kod, gecerlilik_ms: 10000, run_id: oz.run_id, emniyet_rev: oz.emniyet_rev });
  }

  /** `POST /emniyet/degistir` ve `POST /yer_istasyonu/komut` (A3-K91…K95, K100…K104, K118, K119). */
  async function emniyetKomutIsle(req: IncomingMessage, res: ServerResponse, yol: string): Promise<void> {
    const emn = yol.startsWith('/emniyet');
    const c = await onHazirlik(req, res, yol, emn ? 'istek_emniyet_degistir' : 'istek_yer_istasyonu_komut');
    if (c === null) return;
    const b = c.b;
    // Yineleme (A3-K53)
    const anahtar = `${c.istemciId}|POST|${yol}|${String(b.istek_kimligi)}`;
    const ham = JSON.stringify(b);
    const eski = yinelemeler.get(anahtar);
    if (eski !== undefined) {
      if (eski.govde !== ham) return c.reddet(409, 'kimlik_cakismasi', 'aynı istek kimliği farklı gövdeyle');
      return c.yanitla(eski.http, { ...(eski.yanit as Record<string, unknown>), yineleme: true });
    }
    const no = ++komutNo;
    const sonuc = (http: number, nesne: unknown): void => {
      yinelemeler.set(anahtar, { govde: ham, http, yanit: nesne });
      c.yanitla(http, nesne);
    };
    const red = (http: number, kod: string, ileti: string, ayrinti?: Record<string, unknown>): void =>
      sonuc(http, { sonuc: 'ret', komut_no: no, hata: ayrinti ? { kod, ileti, ayrinti } : { kod, ileti } });
    const oz = dunya.ozet();
    if (!CANLI.includes(oz.durum)) return red(409, 'canli_calistirma_yok', 'komut canlı bir çalıştırma gerektiriyor');
    let kisitlayan: boolean;
    if (emn) kisitlayan = emniyetArtiranMi(b);
    else {
      const h = komutParametreHatasi(b);
      if (h !== null) return red(h.http, h.kod, h.ileti);
      kisitlayan = yerKatalogu.komutlar.find((k) => k.ad === b.komut)!.sinif === 'daraltici';
    }
    if (!kisitlayan) {
      if (b.beklenen_run_id !== oz.run_id) return red(409, 'beklenen_durum_uyusmuyor', 'beklenen_run_id güncel değil', { guncel_run_id: oz.run_id, guncel_durum: oz.durum });
    }
    const onayli = emn ? !kisitlayan : yerKatalogu.komutlar.find((k) => k.ad === b.komut)!.onay;
    if (onayli) {
      // Gevşeten: A3-K93, K94, K103, K118. Nötr komut kısıtlayan değildir.
      const jetonKodu = typeof b.onay_jetonu === 'string' ? b.onay_jetonu : null;
      if (b.onay !== true || jetonKodu === null) return red(409, 'onay_gerekli', 'gevşetme onay ve onay jetonu ister (A3-K93)');
      if (typeof b.beklenen_emniyet_rev !== 'number') return red(400, 'gecersiz_istek', 'beklenen_emniyet_rev zorunlu (A3-K118)');
      const j = jetonlar.get(jetonKodu);
      jetonlar.delete(jetonKodu); // tek kullanım: kabul ya da ret (A3-K94)
      if (b.beklenen_emniyet_rev !== oz.emniyet_rev) return red(409, 'emniyet_rev_uyusmuyor', 'beklenen_emniyet_rev güncel değil', { guncel_emniyet_rev: oz.emniyet_rev });
      if (j === undefined || j.istemci !== c.istemciId || Date.now() - j.alindi >= jetonOmru || j.rev !== oz.emniyet_rev || j.run_id !== oz.run_id || j.islem !== islemOzeti(yol, b)) {
        return red(409, 'onay_jetonu_gecersiz', 'onay jetonu geçersiz ya da süresi geçmiş');
      }
    }
    komutSonuclari.set(no, { durum: 'kuyrukta' });
    komutKuyrugu.push({ no, istemci: c.istemciId, komut: emn ? 'emniyet' : 'yer_istasyonu', govde: b });
    return sonuc(202, { sonuc: 'kuyrukta', komut_no: no });
  }

  /** Adım sınırında emniyet ve yer istasyonu komutları (A3-K95: kısıtlayanlar önce, gevşetmeler revizyon artmadıysa). */
  function emniyetKomutUygula(k: BekleyenKomut, buSinirdaKisitlama: boolean): void {
    const b = k.govde;
    const oz = dunya.ozet();
    const emn = k.komut === 'emniyet';
    const t = emn ? null : yerKatalogu.komutlar.find((t) => t.ad === b.komut)!;
    const kisitlayan = emn ? emniyetArtiranMi(b) : t!.sinif === 'daraltici';
    const gevseten = emn ? !kisitlayan : t!.sinif === 'genisletici';
    let ayrinti: Record<string, unknown>;
    const kod = !kisitlayan && b.beklenen_run_id !== oz.run_id ? 'beklenen_durum_uyusmuyor'
      : !kisitlayan && !CANLI.includes(oz.durum) ? 'canli_calistirma_yok'
      : gevseten && buSinirdaKisitlama ? 'kisitlama_hakim'
      : gevseten && b.beklenen_emniyet_rev !== oz.emniyet_rev ? 'emniyet_rev_uyusmuyor' : null;
    if (kod !== null) {
      komutSonuclari.set(k.no, { durum: 'reddedildi', hata: { kod, ileti: kod } });
      ayrinti = { komut_no: k.no, sonuc: 'reddedildi', hata_kodu: kod, sonuc_ayrinti: { guncel_emniyet_rev: oz.emniyet_rev } };
    } else {
      if (emn) dunya.emniyetOperatorUygula(b.girdi as 'acil_durdurma', b.tup === undefined ? null : b.tup === 'f1' ? 0 : 1, GIRDI_DEGERI[b.hedef_durum as string]!);
      else if (kisitlayan) dunya.emniyetRevZorlaArtir(); // daraltıcı yer istasyonu komutu revizyonu artırır (A3-K117 md. 1)
      komutSonuclari.set(k.no, { durum: 'uygulandi', uygulama_adimi: dunya.adim_no });
      ayrinti = { komut_no: k.no, sonuc: 'uygulandi', uygulama_adimi: dunya.adim_no };
    }
    olayiIsle({ tur: 5, onem: 0, kaynak: 1, adim_no: dunya.adim_no, komut_no: k.no, istemci_id: k.istemci, ayrinti });
    if (!emn && ayrinti.sonuc === 'uygulandi') {
      olayiIsle({ tur: 11, onem: 0, kaynak: 6, adim_no: dunya.adim_no, komut_no: k.no, istemci_id: k.istemci, ayrinti: { komut_no: k.no, komut: b.komut, durum: 'karta_iletildi' } });
    }
  }

  /** A3-K95: kısıtlayan emniyet işlemleri, sonra ötekiler; sınıf içinde komut_no sırası. */
  function komutlariUygula(): void {
    const kisitlayanMi = (k: BekleyenKomut): boolean => k.komut === 'emniyet' ? emniyetArtiranMi(k.govde)
      : k.komut === 'yer_istasyonu' && yerKatalogu.komutlar.find((t) => t.ad === k.govde.komut)!.sinif === 'daraltici';
    const bekleyen = komutKuyrugu.splice(0).sort((a, b) => Number(kisitlayanMi(b)) - Number(kisitlayanMi(a)) || a.no - b.no);
    let buSinirdaKisitlama = false;
    for (const k of bekleyen) {
      if (k.komut === 'emniyet' || k.komut === 'yer_istasyonu') {
        emniyetKomutUygula(k, buSinirdaKisitlama);
        if (kisitlayanMi(k)) buSinirdaKisitlama = true;
        continue;
      }
      const oz = dunya.ozet();
      let kod: string | null = null;
      if (k.govde.beklenen_run_id !== oz.run_id) kod = 'beklenen_durum_uyusmuyor';
      else if (k.govde.beklenen_durum_rev !== oz.durum_rev) kod = 'durum_rev_uyusmuyor';
      else if (!KOMUT_DURUMLARI[k.komut]!.includes(oz.durum)) kod = 'gecersiz_durum';
      let ayrinti: Record<string, unknown>;
      if (kod !== null) {
        komutSonuclari.set(k.no, { durum: 'reddedildi', hata: { kod, ileti: kod } });
        ayrinti = { komut_no: k.no, sonuc: 'reddedildi', hata_kodu: kod };
      } else {
        dunya.uygula(k.komut, k.govde as { carpan?: number; periyot_sayisi?: number });
        komutSonuclari.set(k.no, { durum: 'uygulandi', uygulama_adimi: dunya.adim_no });
        ayrinti = { komut_no: k.no, sonuc: 'uygulandi', uygulama_adimi: dunya.adim_no };
      }
      olayiIsle({ tur: 5, onem: 0, kaynak: 1, adim_no: dunya.adim_no, komut_no: k.no, istemci_id: k.istemci, ayrinti });
    }
  }

  const sunucu = createServer((req, res) => {
    const neden = erisimDenetle(req);
    if (neden !== null) return ret(res, 403, 'erisim_reddedildi', neden);
    const url = new URL(req.url ?? '/', 'http://127.0.0.1');
    if (req.method === 'OPTIONS') return ret(res, 405, 'yontem_desteklenmiyor', 'OPTIONS desteklenmez'); // A3-K10
    if (url.pathname.startsWith('/api/')) {
      if (!url.pathname.startsWith('/api/v1/')) return ret(res, 404, 'bulunamadi', 'yok');
      void api(req, res, url).catch((h: unknown) => ret(res, 500, 'sunucu_hatasi', String(h)));
      return;
    }
    if (url.pathname === '/' || url.pathname.startsWith('/panel/')) {
      if (req.method !== 'GET' && req.method !== 'HEAD') return ret(res, 405, 'yontem_desteklenmiyor', 'yalnız GET');
      return statikSun(url.pathname, res);
    }
    return ret(res, 404, 'bulunamadi', 'yok');
  });

  sunucu.on('upgrade', (req: IncomingMessage, soket: Duplex, bas: Buffer) => {
    const reddet = (): void => {
      soket.write('HTTP/1.1 403 Forbidden\r\nConnection: close\r\nContent-Length: 0\r\n\r\n');
      soket.destroy();
    };
    const url = new URL(req.url ?? '/', 'http://127.0.0.1');
    if (url.pathname !== '/ws') {
      soket.write('HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n');
      soket.destroy();
      return;
    }
    if (erisimDenetle(req) !== null) return reddet();
    const protokoller = (req.headers['sec-websocket-protocol'] ?? '').split(',').map((s) => s.trim());
    const jetonlu = protokoller.find((p) => p.startsWith('ida-erisim.'));
    if (!protokoller.includes(WS_ALT_PROTOKOLU) || jetonlu === undefined || !sabitSureliEsit(jetonlu.slice(11), jeton)) {
      return reddet(); // A3-K07
    }
    wss.handleUpgrade(req, soket, bas, (ws) => wss.emit('connection', ws, req));
  });

  await new Promise<void>((coz, red) => {
    sunucu.once('error', red);
    sunucu.listen(ayar.port ?? 47100, '127.0.0.1', () => coz()); // A3-K02: yalnız döngü arabirimi
  });
  const adres = sunucu.address();
  port = typeof adres === 'object' && adres !== null ? adres.port : 0;

  return {
    port,
    jeton,
    acilisAdresi: `http://127.0.0.1:${port}/#erisim=${jeton}`,
    dunya,
    baglantiSayisi: () => baglantilar.size,
    test: {
      ping360Ekle: (sensorId: number, seq: number) => {
        uretimiIsle({
          kanal: 'sonar/ping360',
          t_ns: dunya.t_ns,
          anahtar: `${sensorId}:0`,
          yuk: {
            sensor_id: sensorId,
            inst: 0,
            seq,
            flags: 1,
            window_ns: 0,
            t_tx_ns: dunya.t_ns,
            t_hazir_ns: dunya.t_ns,
            t_teslim_ns: dunya.t_ns,
            parti_step_seq: null,
            fault_mask: 0,
            quality: null,
            model_version: 1,
            aci_grad: seq % 400,
            n_ornek: 4,
            ornek_periyodu: 80,
            darbe_suresi_us: 80,
            frekans_khz: 750,
            kazanc: 1,
            kademe: 1,
            ses_hizi_mps: 1481,
            ornekler: new Uint8Array([1, 2, 3, 4]),
          },
        });
      },
      komutlariTut: (tut: boolean) => {
        komutTutuldu = tut;
      },
      jetonlariSonlandir: () => jetonlar.clear(),
      emniyetRevArtir: () => dunya.emniyetRevZorlaArtir(),
      istekKaydi,
      olayEkle: () => olayiIsle({ tur: 22, onem: 1, kaynak: 0, adim_no: null, ayrinti: { kod: 'test', ileti: 'sınama olayı' } }),
    },
    baglantilariKopar: () => {
      for (const b of baglantilar) {
        b.kapaniyor = true;
        b.ws.terminate();
      }
    },
    kapat: () =>
      new Promise<void>((coz) => {
        clearInterval(dongu);
        clearInterval(canlilik);
        for (const b of baglantilar) b.ws.close(1001, 'sunucu kapaniyor');
        for (const c of wss.clients) c.terminate();
        sunucu.closeAllConnections();
        sunucu.close(() => coz());
      }),
  };
}
