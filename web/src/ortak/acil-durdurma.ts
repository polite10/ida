// Ortak kabuktaki tek yönlü sanal acil durdurma. Yetkili gösterge yalnız
// emniyet/girdiler telemetrisinden gelir (A3-K90); komut sonucu ayrıca gösterilir.

import type { A3Baglanti } from '../a3/baglanti';
import type { KomutYoneticisi } from '../a3/komut';
import { komutSatiri } from './denetim';
import { el, sinifSec, yaz } from './dom';

export function acilDurdurmaDenetimi(b: A3Baglanti, komutlar: KomutYoneticisi): { kok: HTMLElement } {
  const dugme = el('button', { class: 'dugme acil-durdurma-dugme', type: 'button' }, 'SANAL ACİL DURDUR');
  const durum = el('span', { class: 'acil-durdurma-durum', role: 'status', 'aria-live': 'polite' }, 'Sanal girdi: telemetri bekleniyor');
  const neden = el('span', { class: 'denetim-yerel uyari', role: 'status' });
  const sonuc = el('span', { class: 'denetim-son', role: 'status', 'aria-live': 'polite' });
  const kok = el('div', { class: 'denetim acil-durdurma', role: 'group', 'aria-label': 'Sanal acil durdurma' }, dugme, durum,
    el('span', { class: 'denetim-ileti' }, neden, sonuc));
  let telemetri = 'telemetri bekleniyor';
  let telemetriAlindi = false;
  let telemetriTaze = false;
  const gonderiliyor = (): boolean => komutlar.kayitlar().some((k) => k.komut === 'acil_durdur' && k.asama === 'gonderiliyor');

  const guncelle = (): void => {
    const bagli = b.bilgi.durum === 'bagli';
    const engel = !bagli ? 'Gönderilemez: sunucuyla bağlantı yok.'
      : gonderiliyor() ? 'İstek gönderiliyor; HTTP yanıtı bekleniyor.' : '';
    dugme.setAttribute('aria-disabled', engel === '' ? 'false' : 'true');
    dugme.classList.toggle('kapali', engel !== '');
    dugme.title = engel || 'Sanal acil durdurmaya bas';
    yaz(neden, engel);
    const bayat = !bagli ? ' · bayat / bağlantı yok'
      : telemetriAlindi && !telemetriTaze ? ' · bayat / yeni telemetri bekleniyor' : '';
    yaz(durum, `Sanal girdi: ${telemetri}${bayat}`);
  };

  // Her tıklama yalnız sabit basma yöntemine gider; kısayol ve otomatik gönderim yok.
  dugme.addEventListener('click', () => {
    // Çift tıklama ilk isteğin sonucunu yeni bir yerel ret kaydıyla maskelemez.
    if (!gonderiliyor()) void komutlar.acilDurdur();
  });
  b.abone('emniyet/girdiler', (c) => {
    const v = c.yuk.acil_durdurma;
    telemetri = c.gecersiz.includes('acil_durdurma') || (v !== 0 && v !== 1) ? 'GEÇERSİZ'
      : v === 1 ? 'BASILI' : 'serbest';
    telemetriAlindi = true;
    telemetriTaze = b.bilgi.durum === 'bagli';
    guncelle();
  });
  b.durumDinle(() => {
    if (b.bilgi.durum !== 'bagli') telemetriTaze = false;
    guncelle();
  });
  komutlar.dinle((liste) => {
    const k = liste.find((x) => x.komut === 'acil_durdur');
    // W5-K51: doğrulanmış ret ile belirsiz sonuç yalnız stop şeridinde ayrılır.
    const ozet = k?.asama === 'reddedildi' ? `UYGULANMADI: ${k.ileti}`
      : k?.asama === 'bilinmiyor' ? `SONUÇ BİLİNMİYOR: ${k.ileti}`
        : k?.asama === 'kuyrukta' ? 'KUYRUKTA · Uygulama sonucu bekleniyor' : null;
    yaz(sonuc, k === undefined ? '' : ozet === null ? komutSatiri(k)
      : `${k.etiket}${k.komut_no !== null ? ` #${k.komut_no}` : ''} — ${ozet}`);
    sinifSec(sonuc, ['iyi', 'uyari', 'kotu', 'vurgu'], k === undefined ? null
      : k.asama === 'reddedildi' ? 'kotu' : k.asama === 'uygulandi' ? 'iyi'
        : k.asama === 'gonderiliyor' ? 'vurgu' : 'uyari');
    guncelle();
  });
  guncelle();
  return { kok };
}
