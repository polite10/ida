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
import { extname, join, normalize, resolve, sep } from 'node:path';
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

function ret(res: ServerResponse, http: number, kod: string, ileti: string, ayrinti?: Record<string, unknown>): void {
  const govde = JSON.stringify({ sonuc: 'ret', komut_no: null, hata: ayrinti ? { kod, ileti, ayrinti } : { kod, ileti } });
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
    ...(ayar.tohum !== undefined ? { tohum: ayar.tohum } : {}),
  });
  if (ayar.atla !== undefined && ayar.atla > 0) dunya.atla(ayar.atla);
  const senaryoBaytlari = Buffer.from(JSON.stringify(SahteDunya.senaryo()));
  const senaryoOzeti = 'sha256:' + createHash('sha256').update(senaryoBaytlari).digest('hex');

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

  function olayiIsle(o: { tur: number; onem: number; kaynak: number; adim_no: number | null; ayrinti: Record<string, unknown> }): void {
    const kanal = kanalBul('olay')!;
    const sira = ++olaySira;
    const t_ns = dunya.t_ns;
    const yuk = yukKodla('yuk_olay', {
      tur: o.tur,
      onem: o.onem,
      kaynak: o.kaynak,
      adim_no: o.adim_no,
      komut_no: 0,
      istemci_id: 0,
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
        komut_no: 0,
        istemci_id: 0,
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
        son_komut_no: 0,
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
    return ret(res, 404, 'bulunamadi', 'uç yok ya da sahte sunucu bu ucu uygulamıyor (W3-web dilim 1)');
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
