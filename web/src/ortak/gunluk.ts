// Olay günlüğü bileşeni (olay kanalı + yeniden bağlanmada HTTP geçmişi).

import type { A3Baglanti } from '../a3/baglanti';
import type { OlayKaydi } from '../a3/olay';
import { simZamani } from './bicim';
import { el, yaz } from './dom';

const ONEM_ADI: Record<string, string> = { bilgi: 'bilgi', uyari: 'uyarı', hata: 'hata', emniyet: 'emniyet' };

/** A3 §3.5 olay türlerinin ekran adları; bilinmeyen tür kod adıyla gösterilir (A3-K18). */
const TUR_ADI: Record<string, string> = {
  calistirma_durumu_degisti: 'Çalıştırma durumu',
  gecerlilik_dustu: 'Geçerlilik düştü',
  gercek_zaman_kacirildi: 'Gerçek zaman kaçırıldı',
  parti_kacirildi_ozeti: 'Kaçırılan partiler',
  komut_sonucu: 'Komut sonucu',
  ariza_degisti: 'Arıza',
  emniyet_girdisi_degisti: 'Emniyet girdisi',
  atis_olayi_sonucu: 'Atış olayı sonucu',
  a1_oturum_degisti: 'A1 oturumu',
  kart_sifirlandi: 'Kart sıfırlandı',
  yer_istasyonu_komutu: 'Yer istasyonu komutu',
  yer_istasyonu_yaniti: 'Yer istasyonu yanıtı',
  senaryo_yuklendi: 'Senaryo yüklendi',
  on_denetim_sonucu: 'Ön denetim',
  kayit_durumu: 'Kayıt',
  kayit_eksik: 'Kayıt eksik',
  surec_durumu: 'Yardımcı süreç',
  olay_kaybi: 'Olay kaybı',
  erisim_reddedildi: 'Erişim reddedildi',
  izleme_durumu: 'İzleme',
  senaryo_ogesi_uygulandi: 'Senaryo öğesi',
  uyari: 'Uyarı',
  tekrar_mudahalesi: 'Tekrar müdahalesi',
  yapilandirma_reddedildi: 'Yapılandırma reddedildi',
  atama_celiskisi: 'Atama çelişkisi',
  gevsetme_reddedildi: 'Gevşetme reddedildi',
  donma_onceki_cikti_yok: 'Donma: önceki çıktı yok',
  halka_emekliye_ayrildi: 'Halka emekliye ayrıldı',
  tuketici_kamera_kapali: 'Tüketici: kamera kapalı',
  emniyet_rev_doydu: 'Emniyet revizyonu doydu',
};

/** Olay ayrıntısındaki sık değerlerin ekran yazımı (yalnız görünüm). */
const DEGER_ADI: Record<string, string> = {
  takili: 'takılı',
  cikarilmis: 'çıkarılmış',
  basili: 'basılı',
  serbest: 'serbest',
  artiran: 'artıran',
  azaltan: 'azaltan',
  uygulandi: 'uygulandı',
  reddedildi: 'reddedildi',
  kaldirildi: 'kaldırıldı',
  bitti: 'bitti',
  gecti: 'geçti',
  kaldi: 'kaldı',
  silahlandirma_fisi: 'silahlandırma fişi',
  emniyet_pimi: 'emniyet pimi',
  acil_durdurma: 'acil durdurma',
  operator: 'operatör',
  emniyet_girdisi: 'emniyet girdisi',
  ariza: 'arıza',
  kosuyor: 'koşuyor',
  yuklendi: 'yüklendi',
  hazirlaniyor: 'hazırlanıyor',
  duraklatildi: 'duraklatıldı',
  durduruldu: 'durduruldu',
  izleme: 'izleme',
  bos: 'boş',
};

export function olayTurEkranAdi(tur: string): string {
  return TUR_ADI[tur] ?? tur.replaceAll('_', ' ');
}

/** Bilinen türler için kısa özet; bilinmeyende sıkıştırılmış JSON (A3-K18). */
export function olayOzeti(o: OlayKaydi): string {
  const a = o.ayrinti;
  if (a === null) return o.ayrinti_ham ?? '';
  const s = (k: string): string => {
    const v = a[k];
    if (v === undefined || v === null) return '';
    return DEGER_ADI[String(v)] ?? String(v);
  };
  switch (o.tur) {
    case 'calistirma_durumu_degisti':
      return `${s('onceki')} → ${s('sonraki')}${a['neden'] ? ` (${s('neden')})` : ''}`;
    case 'ariza_degisti':
      return `${s('fault_id')}: ${s('islem')} · ${s('kaynak')}`;
    case 'emniyet_girdisi_degisti':
      return `${s('girdi')}${a['tup'] ? ' ' + s('tup') : ''}: ${s('onceki')} → ${s('sonraki')} · ${s('yon')} · ${s('kaynak')} · rev ${s('emniyet_rev')}`;
    case 'senaryo_ogesi_uygulandi':
      return `${s('olay_kimligi')} (${s('tur')}): ${s('sonuc')}`;
    case 'on_denetim_sonucu':
      return `${s('sonuc')} · rapor ${s('rapor_kimligi')}`;
    case 'atis_olayi_sonucu':
    case 'gevsetme_reddedildi':
    case 'emniyet_rev_doydu':
    default: {
      const j = JSON.stringify(a);
      return j.length > 220 ? j.slice(0, 217) + '…' : j;
    }
  }
}

export interface GunlukSecenegi {
  /** Yalnız bu önemdeki ya da bu türlerdeki olaylar. */
  suzgec?: (o: OlayKaydi) => boolean;
}

export function olayGunlugu(b: A3Baglanti, azami: number, secenek: GunlukSecenegi = {}): { liste: HTMLElement; sayac: HTMLElement } {
  const liste = el('ul', { class: 'gunluk', 'aria-live': 'polite' });
  const sayac = el('span', { class: 'kart-not sabit' }, '0');
  const bos = el('li', { class: 'gunluk-bos' }, el('span'), el('span'), el('span', { class: 'yok' }, 'Henüz olay yok'));
  liste.append(bos);
  let n = 0;
  let sonRun = '';
  b.olaylar((o) => {
    if (secenek.suzgec && !secenek.suzgec(o)) return;
    bos.remove();
    n++;
    yaz(sayac, String(n));
    const onem = ONEM_ADI[o.onem] ?? o.onem;
    const li = el(
      'li',
      { title: `sıra ${o.sira} · kaynak ${o.kaynak}${o.adim_no !== null ? ` · adım ${o.adim_no}` : ''}${o.gecmisten ? ' · geçmişten' : ''}` },
      el('span', { class: 'zaman' }, simZamani(o.t_ns)),
      el('span', { class: `onem ${o.onem}` }, onem),
      el('span', null, el('span', { class: 'tur', title: o.tur }, olayTurEkranAdi(o.tur)), ' ', el('span', { class: 'ayrinti' }, olayOzeti(o))),
    );
    // Yeni çalıştırma başlangıcını ayırıcıyla göster.
    if (o.tur === 'calistirma_durumu_degisti' && o.ayrinti?.['sonraki'] === 'hazirlaniyor') {
      const run = String(o.sira);
      if (run !== sonRun) {
        sonRun = run;
        liste.prepend(el('li', { class: 'gunluk-ayirici' }, el('span'), el('span'), el('span', null, 'yeni çalıştırma')));
      }
    }
    liste.prepend(li);
    while (liste.children.length > azami) liste.lastElementChild?.remove();
  });
  return { liste, sayac };
}
