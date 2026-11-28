// Çalıştırma denetimi (ortak durum şeridinin altında, her panelde): başlat, duraklat,
// sürdür, adımla, hız, durdur, sıfırla.
//
// W5-K44: düğmelerin etkinliği ve gösterilen durum yalnız sunucunun bildirdiği
// `calistirma/durum` çerçevesinden gelir. Bir düğmeye basmak durumu değiştirmez; komut
// "gönderiliyor" → "kuyrukta" → "uygulandı" / "reddedildi" / "bilinmiyor" diye ayrı gösterilir
// ve durum şeridi sunucu yeni durumu bildirince değişir. Etkin olmayan düğme nedenini söyler.
// Bu dosya emniyet ya da atış uçlarına istek atmaz (W5-K46).

import type { A3Baglanti } from '../a3/baglanti';
import { dugmeler, type DugmeAdi } from '../a3/etkinlik';
import type { CalistirmaDurumAdi, KipAdi } from '../a3/iletiler';
import { YUKLER } from '../a3/katalog';
import type { KomutKaydi, KomutYoneticisi } from '../a3/komut';
import { el, sinifSec, yaz } from './dom';

type DegerHaritasi = { degerler: Record<number, string> };
const DURUM_ADLARI = (YUKLER.yuk_calistirma_durum.find((a) => a.ad === 'durum') as unknown as DegerHaritasi).degerler;
const KIP_ADLARI = (YUKLER.yuk_calistirma_durum.find((a) => a.ad === 'kip') as unknown as DegerHaritasi).degerler;

const ASAMA: Record<KomutKaydi['asama'], { metin: string; sinif: string }> = {
  gonderiliyor: { metin: 'gönderiliyor', sinif: 'vurgu' },
  kuyrukta: { metin: 'kuyrukta · uygulanmadı', sinif: 'uyari' },
  uygulandi: { metin: 'uygulandı', sinif: 'iyi' },
  reddedildi: { metin: 'reddedildi', sinif: 'kotu' },
  bilinmiyor: { metin: 'sonuç bilinmiyor', sinif: 'uyari' },
};

const DUGME_YAZILARI: Record<Exclude<DugmeAdi, 'yukle'>, string> = {
  baslat: 'Başlat',
  duraklat: 'Duraklat',
  surdur: 'Sürdür',
  adimla: 'Adımla',
  hiz: 'Hız uygula',
  durdur: 'Durdur',
  sifirla: 'Sıfırla',
};

/** `adimla` periyot sayısı (A3-K65): 1–10 000 tamsayı. Geçersizse hata iletisi. */
export function periyotSayisiOku(metin: string): { deger: number } | { hata: string } {
  const t = metin.trim();
  if (!/^\d+$/.test(t)) return { hata: 'Adım sayısı 1–10 000 arasında bir tamsayı olmalı.' };
  const n = Number(t);
  if (n < 1 || n > 10_000) return { hata: 'Adım sayısı 1–10 000 arasında olmalı (bir adım = 5 ms kontrol periyodu).' };
  return { deger: n };
}

/** `hiz` çarpanı (A3-K66): 0 (olabildiğince hızlı) ya da [0,01; 100]. */
export function carpanOku(metin: string): { deger: number } | { hata: string } {
  const t = metin.trim().replace(',', '.');
  if (!/^\d+(\.\d+)?$/.test(t)) return { hata: 'Hız çarpanı bir sayı olmalı (0 = olabildiğince hızlı).' };
  const c = Number(t);
  if (c !== 0 && (c < 0.01 || c > 100)) return { hata: 'Hız çarpanı 0 (olabildiğince hızlı) ya da 0,01–100 arasında olmalı.' };
  return { deger: c };
}

export function komutSatiri(k: KomutKaydi): string {
  const a = ASAMA[k.asama];
  let son = '';
  if (k.asama === 'reddedildi' || k.asama === 'bilinmiyor') son = `: ${k.ileti}`;
  else if (k.asama === 'uygulandi' && k.uygulama_adimi !== null) son = ` (adım ${k.uygulama_adimi})`;
  return `${k.etiket}${k.komut_no !== null ? ` #${k.komut_no}` : ''} — ${a.metin}${son}`;
}

export function calismaDenetimi(b: A3Baglanti, komutlar: KomutYoneticisi): { kok: HTMLElement } {
  let durum: CalistirmaDurumAdi | null = null;
  let kip: KipAdi | null = null;

  const adimGirdi = el('input', { class: 'denetim-girdi', type: 'text', inputmode: 'numeric', value: '1', size: '5', 'aria-label': 'Adım sayısı (kontrol periyodu, 5 ms)', title: 'Adım sayısı: her adım bir kontrol periyodu (5 ms); 1–10 000' });
  const hizGirdi = el('input', { class: 'denetim-girdi', type: 'text', inputmode: 'decimal', value: '1', size: '5', 'aria-label': 'Hız çarpanı', title: 'Hız çarpanı: 0 = olabildiğince hızlı, ya da 0,01–100' });
  const dugmeEl = new Map<DugmeAdi, HTMLButtonElement>();
  const yerel = el('span', { class: 'denetim-yerel', role: 'status' });
  const sonKomut = el('span', { class: 'denetim-son', role: 'status', 'aria-live': 'polite' });
  const gecmis = el('span', { class: 'denetim-gecmis' });

  const dugme = (ad: Exclude<DugmeAdi, 'yukle'>): HTMLButtonElement => {
    const d = el('button', { class: 'dugme denetim-dugme', type: 'button', 'data-komut': ad }, DUGME_YAZILARI[ad]);
    d.addEventListener('click', () => void bas(ad));
    d.addEventListener('focus', () => gosterNeden(ad));
    d.addEventListener('mouseenter', () => gosterNeden(ad));
    d.addEventListener('mouseleave', () => yaz(yerel, ''));
    dugmeEl.set(ad, d);
    return d;
  };

  const kok = el(
    'div',
    { class: 'denetim', role: 'group', 'aria-label': 'Çalıştırma denetimi' },
    el('span', { class: 'etiket' }, 'Çalıştırma'),
    dugme('baslat'),
    dugme('duraklat'),
    dugme('surdur'),
    el('span', { class: 'denetim-grup' }, dugme('adimla'), adimGirdi, el('span', { class: 'denetim-birim' }, '× 5 ms')),
    el('span', { class: 'denetim-grup' }, dugme('hiz'), hizGirdi, el('span', { class: 'denetim-birim' }, '× (0 = azami)')),
    dugme('durdur'),
    dugme('sifirla'),
    el('span', { class: 'denetim-ileti' }, yerel, sonKomut, gecmis),
  );

  const durumlar = (): ReturnType<typeof dugmeler> => {
    const g = dugmeler(durum, kip);
    if (b.bilgi.durum !== 'bagli') {
      for (const ad of Object.keys(g) as DugmeAdi[]) g[ad] = { etkin: false, neden: 'sunucuyla bağlantı yok; ekrandaki durum bayat', onay: null };
    }
    return g;
  };

  const gosterNeden = (ad: DugmeAdi): void => {
    const d = durumlar()[ad];
    yaz(yerel, d.etkin && d.onay === null ? '' : d.neden);
    sinifSec(yerel, ['uyari'], d.etkin ? null : 'uyari');
  };

  const guncelle = (): void => {
    const g = durumlar();
    for (const [ad, d] of dugmeEl) {
      const s = g[ad];
      d.setAttribute('aria-disabled', s.etkin ? 'false' : 'true');
      d.classList.toggle('kapali', !s.etkin);
      d.title = s.etkin ? (s.neden !== '' ? s.neden : DUGME_YAZILARI[ad as Exclude<DugmeAdi, 'yukle'>]) : `Şu an kullanılamaz: ${s.neden}`;
    }
  };

  const komutGoster = (liste: readonly KomutKaydi[]): void => {
    const k = liste[0];
    if (k === undefined) {
      yaz(sonKomut, '');
      return;
    }
    yaz(sonKomut, komutSatiri(k));
    sinifSec(sonKomut, ['iyi', 'uyari', 'kotu', 'vurgu'], ASAMA[k.asama].sinif);
    sonKomut.title = liste
      .slice(0, 8)
      .map((x) => `${new Date(x.gonderildi).toLocaleTimeString('tr-TR')}  ${komutSatiri(x)}`)
      .join('\n');
    yaz(gecmis, liste.length > 1 ? `· önceki: ${liste.length - 1}` : '');
  };

  async function bas(ad: Exclude<DugmeAdi, 'yukle'>): Promise<void> {
    const d = durumlar()[ad];
    if (!d.etkin) {
      yaz(yerel, d.neden);
      sinifSec(yerel, ['uyari'], 'uyari');
      return;
    }
    yaz(yerel, '');
    const ek: Record<string, unknown> = {};
    if (ad === 'adimla') {
      const n = periyotSayisiOku(adimGirdi.value);
      if ('hata' in n) return void (yaz(yerel, n.hata), sinifSec(yerel, ['uyari'], 'uyari'));
      ek.periyot_sayisi = n.deger;
    }
    if (ad === 'hiz') {
      const c = carpanOku(hizGirdi.value);
      if ('hata' in c) return void (yaz(yerel, c.hata), sinifSec(yerel, ['uyari'], 'uyari'));
      ek.carpan = c.deger;
    }
    if (ad === 'duraklat' && d.onay === 'gecersizlik') {
      if (!window.confirm('HIL kipinde duraklatma çalıştırmayı zamanlama bakımından geçersiz kılar (hil_duraklatma). Yine de duraklatılsın mı?')) return;
      ek.gecersizligi_onayla = true;
    }
    await komutlar.gonder(ad, ek);
  }

  b.abone('calistirma/durum', (c) => {
    durum = (DURUM_ADLARI[c.yuk.durum] ?? null) as CalistirmaDurumAdi | null;
    kip = (KIP_ADLARI[c.yuk.kip] ?? null) as KipAdi | null;
    guncelle();
  });
  b.durumDinle(() => guncelle());
  komutlar.dinle((liste) => komutGoster(liste.filter((k) => k.komut !== 'acil_durdur')));
  guncelle();
  return { kok };
}
