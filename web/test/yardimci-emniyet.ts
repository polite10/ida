// Bellek içi taşıma: gerçek A3Baglanti, HTTP istemcisi, çözücü ve komut yöneticisi
// çalışır; yalnız fetch/WebSocket sınırı elle sürülür. Soketli sunucu testlerinin
// yerine geçmez. Yanıt, olay ve kanal çerçevesi birbirinden bağımsız verilebilir.
import assert from 'node:assert/strict';
import { A3Baglanti } from '../src/a3/baglanti';
import { kanalBul } from '../src/a3/cerceve';
import { EmniyetIstekleri } from '../src/a3/emniyet-istek';
import { KATALOG_OZETI } from '../src/a3/katalog';
import { cerceveKodla, yukKodla } from '../src/a3/kodlayici';
import { KomutYoneticisi } from '../src/a3/komut';

export const KALDIR = { girdi: 'acil_durdurma', hedef: 'serbest' } as const;

export function kapi<T>(): { soz: Promise<T>; ac(v: T): void } {
  let ac!: (v: T) => void;
  const soz = new Promise<T>((coz) => { ac = coz; });
  return { soz, ac };
}

export async function bellekKur() {
  const hazir = kapi<void>();
  let ws!: BellekWS;
  class BellekWS {
    readyState = 1;
    binaryType = '';
    onmessage: ((e: { data: unknown }) => void) | null = null;
    onclose: ((e: { code: number; reason: string }) => void) | null = null;
    onerror: (() => void) | null = null;
    constructor() { ws = this; hazir.ac(); }
    send(): void { /* Abonelikler elle sürülen sunucuda zaten biliniyor. */ }
    close(): void { this.readyState = 3; }
    metin(o: unknown): void { this.onmessage?.({ data: JSON.stringify(o) }); }
    ikili(u: Uint8Array): void {
      this.onmessage?.({ data: u.buffer.slice(u.byteOffset, u.byteOffset + u.byteLength) });
    }
  }
  const istekler: { yol: string; govde: Record<string, unknown> }[] = [];
  const ag = { hata: false, yanitKayip: false };
  const saat = { t: 100_000 };
  const jeton = { kod: 'a'.repeat(32), rev: 1, beklet: null as null | Promise<void> };
  let komutNo = 0;
  let sira = 0;
  const kabul = kapi<void>();
  const f: typeof fetch = async (u, init) => {
    const yol = new URL(String(u)).pathname.replace('/api/v1', '');
    const govde = typeof init?.body === 'string' ? JSON.parse(init.body) as Record<string, unknown> : {};
    if (ag.hata) throw new TypeError('taşıma kapalı');
    istekler.push({ yol, govde });
    const json = (v: unknown, status = 200): Response => new Response(JSON.stringify(v), { status });
    if (yol === '/surum') return json({ a3_surumu: '1.0', katalog_ozeti: KATALOG_OZETI });
    if (yol === '/istemciler') return json({ sonuc: 'kabul', istemci_id: 1 });
    if (yol === '/emniyet/onay_jetonu') {
      const y = { onay_jetonu: jeton.kod, gecerlilik_ms: 10000, run_id: '20261128-120000-000001', emniyet_rev: jeton.rev };
      await jeton.beklet;
      return json(y);
    }
    if (yol === '/emniyet/degistir') {
      kabul.ac();
      if (ag.yanitKayip) throw new TypeError('istek alındı, yanıt kayboldu');
      if (govde.hedef_durum === 'serbest') {
        return json({ sonuc: 'ret', komut_no: ++komutNo, hata: { kod: 'onay_jetonu_gecersiz', ileti: 'jeton süresi doldu' } }, 409);
      }
      return json({ sonuc: 'kuyrukta', komut_no: ++komutNo }, 202);
    }
    assert.fail(`beklenmeyen taşıma isteği: ${yol}`);
  };
  const b = new A3Baglanti({
    taban: 'http://bellek.invalid', jeton: 'e'.repeat(64), fetchIslevi: f,
    WebSocketSinifi: BellekWS as unknown as typeof WebSocket,
  });
  const komutlar = new KomutYoneticisi(b);
  const em = new EmniyetIstekleri(b, komutlar, () => saat.t);
  b.baslat();
  await hazir.soz;
  ws.metin({ tur: 'merhaba', a3_surumu: '1.0', katalog_ozeti: KATALOG_OZETI,
    baglanti_id: 1, run_id: '20261128-120000-000001', son_olay_sirasi: 0, yayin_tavani_hz: 50 });
  assert.equal(b.bilgi.durum, 'bagli');
  const bildir = (acil: 0 | 1, rev = 1): void => {
    ws.ikili(cerceveKodla(kanalBul('emniyet/girdiler')!, { t_ns: 1, sira: ++sira, bayraklar: 0 }, yukKodla('yuk_emniyet_girdiler', {
      acil_durdurma: acil, silahlandirma_fisi: [0, 0], emniyet_pimi: [1, 1],
      son_degisim_adimi: null, son_degisim_kaynagi: 0, kart_acil_durdurma: 0,
      kart_silahlandirma_fisi: [0, 0], kart_emniyet_pimi: [1, 1], geri_okuma_farki: acil,
      emniyet_rev: rev, emniyet_rev_doydu: 0,
    })));
  };
  const uygulandi = (): void => {
    ws.ikili(cerceveKodla(kanalBul('olay')!, { t_ns: 1, sira: 1, bayraklar: 0 }, yukKodla('yuk_olay', {
      tur: 5, onem: 0, kaynak: 1, adim_no: 1, komut_no: komutNo, istemci_id: 1,
      ayrinti: JSON.stringify({ komut_no: komutNo, sonuc: 'uygulandi', uygulama_adimi: 1 }),
    })));
  };
  bildir(0);
  return {
    baglanti: b, komutlar, em, saat, jeton, ag, kabul, bildir, uygulandi, istekler,
    post: (yol: string) => istekler.filter((i) => i.yol === yol).length,
    kapat: () => { em.kapat(); komutlar.kapat(); b.kapat(); },
  };
}

export type BellekOrtami = Awaited<ReturnType<typeof bellekKur>>;
