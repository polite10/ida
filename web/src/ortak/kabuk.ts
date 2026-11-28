// Panel kabuğu: üst çubuk (marka, panel sekmeleri, sahte veri rozeti),
// ortak durum şeridi (08 §2.1) ve bağlantının kurulması.

import { A3Baglanti, adrestenAyar, type BaglantiBilgisi } from '../a3/baglanti';
import { GECERLILIK_NEDENLERI, YUKLER } from '../a3/katalog';
import { EmniyetIstekleri } from '../a3/emniyet-istek';
import { KomutYoneticisi } from '../a3/komut';
import { acilDurdurmaSeridi } from './acil';
import { calismaDenetimi } from './denetim';
import { buyuk, durumAdi, kipAdi, sayi, simZamani, sure_ns, tamsayi } from './bicim';
import { el, sinifSec, yaz } from './dom';

export type PanelAdi = 'durum' | 'grafik' | 'harita' | 'sonar' | 'atis' | 'emniyet' | 'senaryo';

export const PANELLER: readonly { ad: PanelAdi; baslik: string; kisa: string; aciklama: string }[] = [
  { ad: 'durum', baslik: 'Araç ve sistem', kisa: 'Durum', aciklama: 'Araç durumu, güç ve iticiler, kart sağlığı, gerçek zaman sayaçları, olay günlüğü' },
  { ad: 'grafik', baslik: 'Canlı grafikler', kisa: 'Grafik', aciklama: 'Seçilen büyüklüklerin zaman serileri; yer gerçeği ve kart kestirimi ayrı renkte' },
  { ad: 'harita', baslik: 'Harita', kisa: 'Harita', aciklama: 'NED yerel düzlem: gerçek iz, kestirim izi, yol noktaları, hedef' },
  { ad: 'sonar', baslik: 'Sonar', kisa: 'Sonar', aciklama: 'Ping360 kutupsal tarama ve Ping1D zaman–menzil şeridi' },
  { ad: 'atis', baslik: 'Atış kontrol ve emniyet', kisa: 'Emniyet', aciklama: 'Emniyet zinciri durumu, sanal girdiler ve kart geri okuması (yalnız gösterim)' },
  { ad: 'emniyet', baslik: 'Emniyet girdileri ve yer istasyonu', kisa: 'Girdiler', aciklama: 'Sanal acil durdurma, silahlandırma fişi ve emniyet pimi (gevşetme iki adımlı); yer istasyonu komutu' },
  { ad: 'senaryo', baslik: 'Senaryo', kisa: 'Senaryo', aciklama: 'Senaryo dosyaları: listele, doğrula, yükle; yüklü senaryo ve ön denetim raporu' },
];

export const MARKA_SVG = `<svg class="marka-simge" viewBox="0 0 24 24" fill="none" aria-hidden="true">
<circle cx="12" cy="12" r="10.5" stroke="#2e3a47"/>
<path d="M12 12 L12 2.2 A9.8 9.8 0 0 1 20.5 7.1 Z" fill="rgba(103,232,249,0.18)"/>
<path d="M12 12 L20.5 7.1" stroke="#67e8f9" stroke-width="1.4" stroke-linecap="round"/>
<circle cx="12" cy="12" r="6" stroke="#2e3a47"/>
<path d="M8.6 14.2 L12 7.6 L15.4 14.2 L12 12.8 Z" fill="#d9e1e8"/>
</svg>`;

export interface Panel {
  baglanti: A3Baglanti;
  /** Durum değiştiren komutların tek yolu (çalıştırma denetimi ve senaryo yükleme). */
  komutlar: KomutYoneticisi;
  /** Emniyet girdisi ve yer istasyonu isteklerinin tek yolu (acil durdurma düğmesi ve emniyet paneli). */
  emniyet: EmniyetIstekleri;
  icerik: HTMLElement;
  /** Bağlantı koptuysa ya da hiç kurulmadıysa true (veri bayat). */
  bayat(): boolean;
}

export interface PanelSecenegi {
  /** İlk bağlantıda HTTP'den yüklenecek geçmiş olay sayısı (A3-K45). */
  gecmisOlay?: number;
  icerikSinifi?: string;
}

function hashBagla(href: string): string {
  return href + location.hash;
}

function ustCubuk(etkin: PanelAdi | 'index'): { kok: HTMLElement; sahte: HTMLElement } {
  const marka = el('a', { class: 'marka', href: hashBagla('/'), title: 'Panel listesi' });
  marka.innerHTML = MARKA_SVG;
  marka.append(
    el('span', { style: 'display:flex;flex-direction:column;gap:4px' }, el('span', { class: 'marka-ad' }, 'İDA'), el('span', { class: 'marka-alt' }, 'Laboratuvar')),
  );
  const sekmeler = el('nav', { class: 'sekmeler', 'aria-label': 'Paneller' });
  PANELLER.forEach((p, i) => {
    const a = el(
      'a',
      { class: 'sekme', href: hashBagla(`/panel/${p.ad}.html`), title: p.aciklama },
      el('span', { class: 'sekme-no' }, String(i + 1).padStart(2, '0')),
      el('span', null, p.kisa === p.baslik ? p.baslik : p.kisa),
    );
    if (p.ad === etkin) a.setAttribute('aria-current', 'page');
    sekmeler.append(a);
  });
  const sahte = el('span', { class: 'sahte-rozet', hidden: true, title: 'Sahte A3 sunucusu: kinematik senaryo, fizik değil' }, 'SAHTE VERİ');
  const kok = el('header', { class: 'ust' }, marka, sekmeler, el('div', { class: 'ust-sag' }, sahte));
  return { kok, sahte };
}

interface Hucre {
  kok: HTMLElement;
  deger: HTMLElement;
}

function hucre(etiket: string, sinif = ''): Hucre {
  const deger = el('div', { class: 'serit-deger' });
  const kok = el('div', { class: `serit-hucre ${sinif}` }, el('span', { class: 'etiket' }, etiket), deger);
  return { kok, deger };
}

const CALISMA_RENK: Record<string, string> = {
  kosuyor: 'iyi',
  hazirlaniyor: 'vurgu',
  duraklatildi: 'uyari',
  hata: 'kotu',
  izleme: 'vurgu',
};

const KIP_ADLARI = (YUKLER.yuk_calistirma_durum.find((a) => a.ad === 'kip') as { degerler: Record<number, string> }).degerler;
const DURUM_ADLARI = (YUKLER.yuk_calistirma_durum.find((a) => a.ad === 'durum') as { degerler: Record<number, string> }).degerler;
const OTURUM_ADLARI = (YUKLER.yuk_calistirma_durum.find((a) => a.ad === 'a1_oturum') as { degerler: Record<number, string> }).degerler;

/** Ortak durum şeridi: bağlantı, çalıştırma durumu, kip, sim zamanı, geçerlilik, kaçırılan adım. */
function durumSeridi(b: A3Baglanti, sahteRozet: HTMLElement): { kok: HTMLElement } {
  const baglanti = hucre('Bağlantı', 'baglanti-hucre');
  const led = el('span', { class: 'led' });
  const bagMetin = el('span');
  const bagAlt = el('small');
  baglanti.deger.append(el('span', { style: 'display:inline-flex;align-items:center;gap:7px' }, led, bagMetin), bagAlt);

  const calisma = hucre('Çalıştırma');
  const calismaCip = el('span', { class: 'cip notr' }, '—');
  calisma.deger.append(calismaCip);
  const kip = hucre('Zaman kipi');
  const zaman = hucre('Sim zamanı');
  const carpan = hucre('Çarpan');
  const gecerlilik = hucre('Geçerlilik');
  const gecCip = el('span', { class: 'cip notr' }, '—');
  const gecNeden = el('small');
  gecerlilik.deger.append(gecCip, gecNeden);
  const kacan = hucre('Gerçek zaman · kaçırılan adım');
  const oturum = hucre('A1 oturumu');
  const run = hucre('Çalıştırma kimliği', 'genis');

  const kok = el(
    'div',
    { class: 'serit bayat', role: 'status', 'aria-live': 'off' },
    baglanti.kok,
    calisma.kok,
    kip.kok,
    zaman.kok,
    carpan.kok,
    gecerlilik.kok,
    kacan.kok,
    oturum.kok,
    run.kok,
  );

  let kipKod = 0;

  b.abone('calistirma/durum', (c) => {
    const y = c.yuk;
    kipKod = y.kip;
    const durumAd = DURUM_ADLARI[y.durum] ?? String(y.durum);
    yaz(calismaCip, buyuk(durumAdi(durumAd)));
    sinifSec(calismaCip, ['iyi', 'uyari', 'kotu', 'notr', 'vurgu'], CALISMA_RENK[durumAd] ?? 'notr');
    yaz(kip.deger, kipAdi(KIP_ADLARI[y.kip] ?? String(y.kip)));
    yaz(zaman.deger, simZamani(c.baslik.t_ns));
    carpan.deger.innerHTML = '';
    carpan.deger.append(
      `${sayi(y.gerceklesen_carpan, 2)}×`,
      el('small', null, y.hedef_carpan === 0 ? 'hedef azami' : `hedef ${sayi(y.hedef_carpan, 2)}×`),
    );
    const gecerli = y.gecerli === 1;
    yaz(gecCip, gecerli ? 'GEÇERLİ' : 'GEÇERSİZ');
    sinifSec(gecCip, ['iyi', 'kotu', 'notr'], gecerli ? 'iyi' : 'kotu');
    const nedenler: string[] = [];
    for (let bit = 0; bit < 32; bit++) if (y.gecerlilik_nedenleri & (1 << bit)) nedenler.push(GECERLILIK_NEDENLERI[bit] ?? `bit ${bit}`);
    yaz(gecNeden, nedenler.length > 0 ? nedenler.join(', ') : y.gercek_zamani_kacirdi ? 'gerçek zaman kaçırıldı' : '');
    gecerlilik.kok.title = nedenler.length > 0 ? 'Geçersizlik nedenleri: ' + nedenler.join(', ') : 'Çalıştırma zamanlama bakımından geçerli';
    const ot = OTURUM_ADLARI[y.a1_oturum] ?? String(y.a1_oturum);
    oturum.kok.hidden = y.a1_oturum === 255;
    yaz(oturum.deger, buyuk(ot.replace('_', ' ')));
    yaz(run.deger, y.run_id === '' ? '—' : y.run_id);
  });

  b.abone('sayac/gercek_zaman', (c) => {
    const y = c.yuk;
    const hil = kipKod === 2 || kipKod === 3;
    kacan.deger.innerHTML = '';
    const sayiEl = el('span', null, tamsayi(y.kacirilan_adim));
    // 08 §2.4: HIL kiplerinde sıfırdan farklı değer kırmızı; SIL'de gri, tanısal.
    if (y.kacirilan_adim > 0) sayiEl.style.color = hil ? 'var(--kotu)' : 'var(--metin-3)';
    kacan.deger.append(sayiEl, el('small', null, `ihlal ${tamsayi(y.adim_son_tarih_ihlali)} · azami ${sure_ns(y.azami_adim_gecikmesi_ns)}`));
    kacan.kok.title =
      `Kaçırılan adım: ${y.kacirilan_adim}\nSon tarih ihlali: ${y.adim_son_tarih_ihlali}\nAzami adım gecikmesi: ${sure_ns(y.azami_adim_gecikmesi_ns)}` +
      `\nKaçırılan parti: ${y.parti_kacirildi}\nBariyer bekleme: ${y.bariyer_bekleme}` +
      (hil ? '' : '\nSIL: sim tarafında gerçek zaman kaçırma tanısaldır (08 §2.4).');
  });

  const bagGuncelle = (bi: BaglantiBilgisi): void => {
    const renkler = ['iyi', 'uyari', 'kotu'];
    led.classList.remove('yanip');
    switch (bi.durum) {
      case 'bagli': {
        sinifSec(led, renkler, 'iyi');
        yaz(bagMetin, 'BAĞLI');
        yaz(bagAlt, bi.uyarilar.length > 0 ? '⚠ ' + bi.uyarilar.join('; ') : '');
        break;
      }
      case 'baglaniyor':
        sinifSec(led, renkler, 'uyari');
        led.classList.add('yanip');
        yaz(bagMetin, 'BAĞLANIYOR');
        yaz(bagAlt, '');
        break;
      case 'kopuk': {
        sinifSec(led, renkler, 'kotu');
        led.classList.add('yanip');
        yaz(bagMetin, 'KOPUK');
        const kalan = bi.sonrakiDeneme === null ? 0 : Math.max(0, Math.ceil((bi.sonrakiDeneme - Date.now()) / 1000));
        yaz(bagAlt, `${bi.deneme}. deneme · ${kalan} s`);
        break;
      }
      case 'erisim_reddedildi':
        sinifSec(led, renkler, 'kotu');
        yaz(bagMetin, 'ERİŞİM YOK');
        yaz(bagAlt, 'jeton geçersiz');
        break;
      case 'surum_uyusmazligi':
        sinifSec(led, renkler, 'kotu');
        yaz(bagMetin, 'SÜRÜM');
        yaz(bagAlt, bi.ileti);
        break;
      default:
        sinifSec(led, renkler, null);
        yaz(bagMetin, 'KAPALI');
        yaz(bagAlt, '');
    }
    baglanti.kok.title = `${new URL(b.http.taban).host} · ${bi.ileti}`;
    kok.classList.toggle('bayat', bi.durum !== 'bagli');
    document.body.classList.toggle('veri-bayat', bi.durum !== 'bagli' && bi.bagliOldugu !== null);
    sahteRozet.hidden = !(bi.surum?.sim_surumu.startsWith('sahte') ?? false);
  };
  b.durumDinle(bagGuncelle);
  setInterval(() => {
    if (b.bilgi.durum === 'kopuk') bagGuncelle(b.bilgi);
  }, 500);
  return { kok };
}

function iletiEkrani(baslik: string, ...paragraflar: (string | Node)[]): HTMLElement {
  return el(
    'div',
    { class: 'ileti-ekrani' },
    el('div', { class: 'ileti-kutusu' }, el('h1', null, baslik), ...paragraflar.map((p) => el('p', null, p))),
  );
}

/** Bağlantı kopukken sağ altta duran uyarı. */
function kopukPerdesi(b: A3Baglanti): HTMLElement {
  const metin = el('span');
  const dugme = el('button', { class: 'dugme', type: 'button' }, 'Şimdi dene');
  dugme.addEventListener('click', () => b.simdiDene());
  const kok = el('div', { class: 'kopuk-perde', role: 'alert' }, el('span', { class: 'led kotu yanip' }), metin, dugme);
  const guncelle = (bi: BaglantiBilgisi): void => {
    const acik = bi.durum === 'kopuk' && bi.bagliOldugu !== null;
    kok.classList.toggle('acik', acik);
    if (acik) {
      const kalan = bi.sonrakiDeneme === null ? 0 : Math.max(0, Math.ceil((bi.sonrakiDeneme - Date.now()) / 1000));
      yaz(metin, `Sim sunucusuyla bağlantı koptu; ekrandaki değerler bayat. Yeniden deneme ${kalan} s içinde.`);
    }
  };
  b.durumDinle(guncelle);
  setInterval(() => guncelle(b.bilgi), 500);
  return kok;
}

/** Paneli kurar; jeton yoksa ya da sürüm uyuşmazsa açıklayıcı ekran gösterir. */
export function panelBaslat(ad: PanelAdi, kur: (p: Panel) => void, secenek: PanelSecenegi = {}): void {
  const p = PANELLER.find((x) => x.ad === ad)!;
  document.title = `${p.baslik} · İDA`;
  const ust = ustCubuk(ad);
  const ayar = adrestenAyar(location);
  const kok = el('div', { class: 'sayfa' });
  document.body.append(kok);
  if (ayar === null) {
    kok.append(
      ust.kok,
      el('div'),
      iletiEkrani(
        'Erişim jetonu yok',
        'Paneller sim sunucusunun başlangıçta yazdığı açılış adresiyle açılır (…/#erisim=<jeton>). Jeton adresin # sonrasındadır ve sunucuya gönderilmez (A3-K05).',
        'Geliştirmede: web/ dizininde `npm run gelistir` çalıştırın ve yazılan adresi açın.',
      ),
    );
    return;
  }
  const b = new A3Baglanti({ ...ayar, gecmisOlay: secenek.gecmisOlay ?? 0 });
  const serit = durumSeridi(b, ust.sahte);
  const komutlar = new KomutYoneticisi(b);
  const denetim = calismaDenetimi(b, komutlar);
  const emniyet = new EmniyetIstekleri(b, komutlar);
  const acil = acilDurdurmaSeridi(b, emniyet, komutlar);
  const icerik = el('main', { class: `icerik ${secenek.icerikSinifi ?? ''}` });
  kok.append(ust.kok, el('div', { class: 'ust-blok' }, serit.kok, denetim.kok, acil.kok), icerik, kopukPerdesi(b));

  let ekran: HTMLElement | null = null;
  b.durumDinle((bi) => {
    const engel = bi.durum === 'surum_uyusmazligi' || (bi.durum === 'erisim_reddedildi' && bi.bagliOldugu === null);
    if (engel && ekran === null) {
      ekran =
        bi.durum === 'surum_uyusmazligi'
          ? iletiEkrani('A3 sürüm uyuşmazlığı', bi.ileti, 'Panel bağlanmadı (A3-K15). Panelleri sim sunucusuyla aynı sürümden derleyin.')
          : iletiEkrani('Erişim reddedildi', 'Jeton geçersiz. Sim sunucusu her başlatmada yeni jeton üretir; sunucunun yazdığı son açılış adresini kullanın.');
      icerik.replaceWith(ekran);
    }
  });

  kur({ baglanti: b, komutlar, emniyet, icerik, bayat: () => b.bilgi.durum !== 'bagli' });
  b.baslat();
}
