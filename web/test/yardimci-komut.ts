// W5-web-komut testlerinin ortak kurulumu: elle yönetilen sahte sunucu, kayıt tutan fetch
// (istek ve yanıt gövdeleri `a3-kanallar.json` şemalarına karşı doğrulanır) ve bekleme yardımcıları.

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { sahteSunucuBaslat, type SahteSunucu } from '../arac/sahte-a3/sunucu';
import { semaDogrula, semaOku, type SemaKatalogu } from '../arac/sahte-a3/sema';
import { A3Baglanti } from '../src/a3/baglanti';
import { KomutYoneticisi, type KomutAyari } from '../src/a3/komut';
import { YUKLER } from '../src/a3/katalog';

export const KATALOG_YOLU = resolve(process.cwd(), '..', 'docs', 'sozlesmeler', 'sema', 'a3-kanallar.json');
export const SEMA: SemaKatalogu = semaOku(readFileSync(KATALOG_YOLU, 'utf8'));
export const JETON = 'e'.repeat(64);
export const bekle = (ms: number): Promise<void> => new Promise((r) => setTimeout(r, ms));

export async function kosul(f: () => boolean, ms: number, ne: string): Promise<void> {
  const son = Date.now() + ms;
  while (Date.now() < son) {
    if (f()) return;
    await bekle(10);
  }
  assert.fail(`zaman aşımı: ${ne}`);
}

export interface IstekKaydi {
  yontem: string;
  yol: string;
  baslik: Record<string, string>;
  govde: unknown;
  http: number;
  yanit: unknown;
}

const DURUM_ADLARI = (YUKLER.yuk_calistirma_durum.find((a) => a.ad === 'durum') as unknown as { degerler: Record<number, string> }).degerler;

export interface Kurulum {
  sunucu: SahteSunucu;
  baglanti: A3Baglanti;
  komutlar: KomutYoneticisi;
  istekler: IstekKaydi[];
  /** Sunucunun kanaldan bildirdiği durum adı (ekranın gösterdiği). */
  kanalDurumu(): string | null;
  /** GET /durum (sunucunun yetkili anlık görüntüsü). */
  durum(): Promise<{ durum: string; run_id: string | null; durum_rev: number; adim_no: number; sim_t_ns: number }>;
  durumBekle(ad: string, ms?: number): Promise<void>;
  /** Kanalın durum_rev'i sunucunun GET /durum değerine yetişsin (komut ekranın gösterdiği revizyonla gider). */
  senkron(ms?: number): Promise<void>;
  kapat(): Promise<void>;
}

export interface KurulumAyari {
  kip?: 'sil' | 'hil_1x';
  komut?: KomutAyari;
  /** Başarısız POST'lar için fetch'i sarmala (ağ hatası benzetimi). */
  fetchHatasi?: () => boolean;
  ilkGecikmeMs?: number;
}

export async function kur(ayar: KurulumAyari = {}): Promise<Kurulum> {
  const sunucu = await sahteSunucuBaslat({ port: 0, jeton: JETON, katalogYolu: KATALOG_YOLU, statik: null, elle: true, kip: ayar.kip ?? 'sil' });
  const istekler: IstekKaydi[] = [];
  const sarili: typeof fetch = async (girdi, init) => {
    if (ayar.fetchHatasi?.() === true && init?.method === 'POST') throw new TypeError('fetch failed (benzetim)');
    const yanit = await fetch(girdi, init);
    const kopya = yanit.clone();
    const metin = await kopya.text();
    let json: unknown = null;
    try {
      json = metin === '' ? null : JSON.parse(metin);
    } catch {
      json = metin;
    }
    const baslik: Record<string, string> = {};
    const h = init?.headers as Record<string, string> | undefined;
    if (h !== undefined) for (const [k, v] of Object.entries(h)) baslik[k.toLowerCase()] = v;
    let govde: unknown = null;
    if (typeof init?.body === 'string') govde = JSON.parse(init.body);
    istekler.push({ yontem: init?.method ?? 'GET', yol: new URL(String(girdi)).pathname.replace('/api/v1', ''), baslik, govde, http: yanit.status, yanit: json });
    return yanit;
  };
  const baglanti = new A3Baglanti({ taban: `http://127.0.0.1:${sunucu.port}`, jeton: JETON, fetchIslevi: sarili, ilkGecikmeMs: ayar.ilkGecikmeMs ?? 100, azamiGecikmeMs: 300, gecmisOlay: 50 });
  baglanti.baslat();
  await kosul(() => baglanti.bilgi.durum === 'bagli' && baglanti.son('calistirma/durum') !== undefined, 5000, 'bağlantı ve ilk durum çerçevesi');
  const komutlar = new KomutYoneticisi(baglanti, ayar.komut);
  const kanalDurumu = (): string | null => {
    const c = baglanti.son('calistirma/durum');
    return c === undefined ? null : (DURUM_ADLARI[c.yuk.durum] ?? null);
  };
  return {
    sunucu,
    baglanti,
    komutlar,
    istekler,
    kanalDurumu,
    durum: () => baglanti.http.durum() as never,
    durumBekle: (ad, ms = 8000) => kosul(() => kanalDurumu() === ad, ms, `kanal durumu ${ad} (şimdi ${kanalDurumu()})`),
    senkron: async (ms = 3000) => {
      const hedef = ((await baglanti.http.durum()) as { durum_rev: number }).durum_rev;
      await kosul(() => Number(baglanti.son('calistirma/durum')?.yuk.durum_rev) === hedef, ms, `kanal durum_rev ${hedef}'a yetişsin`);
    },
    kapat: async () => {
      komutlar.kapat();
      baglanti.kapat();
      await sunucu.kapat();
    },
  };
}

/** İstek ve yanıt gövdelerini sözleşme şemalarına karşı doğrular. */
export function semayaUy(sema: string, deger: unknown, ne: string): void {
  const s = SEMA.$defs[sema];
  assert.ok(s !== undefined, `şema yok: ${sema}`);
  assert.deepEqual(semaDogrula(SEMA, s, deger), [], `${ne}: ${sema} şemasına uymuyor: ${JSON.stringify(deger)}`);
}
