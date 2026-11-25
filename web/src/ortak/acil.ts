// Ortak durum şeridindeki "ACİL DURDUR" düğmesi (her panelde, tek adım).
//
// W5-K51: düğmeye basmak göstergeyi değiştirmez. Gösterilen acil durdurma durumu YALNIZ
// sunucunun `emniyet/girdiler` çerçevesinden gelir; isteğin sonucu (UYGULANDI / UYGULANMADI /
// SONUÇ BİLİNMİYOR) ayrı satırda yazılır. Kısayol yoktur (W5-K53): düğme yalnız tıklanır.
// İstek mantığı `a3/emniyet-istek.ts`'tedir; bu dosya yalnız görünümdür.

import type { A3Baglanti } from '../a3/baglanti';
import { EmniyetIstekleri, sonucMetni } from '../a3/emniyet-istek';
import type { KomutYoneticisi } from '../a3/komut';
import { el, sinifSec, yaz } from './dom';
import { girdiGosterimi } from './emniyet';

export function acilDurdurmaSeridi(b: A3Baglanti, em: EmniyetIstekleri, komutlar: KomutYoneticisi): { kok: HTMLElement } {
  const dugme = el(
    'button',
    { class: 'dugme acil-dugme', type: 'button', title: 'Sanal acil durdurmayı basar (emniyeti artırır; tek adım). Gösterge, sunucu uyguladığını bildirince değişir.' },
    'ACİL DURDUR',
  );
  const durum = el('span', { class: 'acil-durum', role: 'status' });
  const sonuc = el('span', { class: 'acil-sonuc', role: 'alert', 'aria-live': 'assertive' });
  const kok = el(
    'div',
    { class: 'acil-serit', role: 'group', 'aria-label': 'Acil durdurma' },
    el('span', { class: 'etiket' }, 'Acil durdurma'),
    dugme,
    durum,
    sonuc,
  );

  const durumGuncelle = (): void => {
    const c = b.son('emniyet/girdiler');
    const bayat = b.bilgi.durum !== 'bagli';
    if (c === undefined) {
      yaz(durum, 'sunucu: durum henüz gelmedi');
      sinifSec(durum, ['iyi', 'uyari', 'kotu'], 'uyari');
      return;
    }
    const satir = girdiGosterimi(c.yuk, c.gecersiz).satirlar[0]!;
    const metin = satir.yetkili.metin;
    yaz(durum, `sunucu: ${metin}${bayat ? ' · BAYAT (bağlantı yok)' : ''}`);
    sinifSec(durum, ['iyi', 'uyari', 'kotu'], bayat || satir.yetkili.sinif === 'gecersiz' || satir.yetkili.sinif === 'bilinmiyor' || satir.yetkili.sinif === 'yok' ? 'uyari' : satir.yetkili.sinif === 'kisitlayan' ? 'kotu' : 'iyi');
  };

  const sonucGuncelle = (): void => {
    const k = komutlar.kayitlar().find((x) => x.komut === 'emniyet_degistir' && x.ozet.startsWith('Acil durdurma'));
    if (k === undefined) {
      yaz(sonuc, '');
      return;
    }
    const m = sonucMetni(k);
    yaz(sonuc, `${m.baslik} — ${k.ozet} — ${m.ayrinti}`);
    sinifSec(sonuc, ['iyi', 'uyari', 'kotu', 'vurgu'], m.sinif);
  };

  dugme.addEventListener('click', () => {
    void em.artir({ girdi: 'acil_durdurma', hedef: 'basili' });
  });
  b.abone('emniyet/girdiler', () => durumGuncelle(), 10);
  b.durumDinle(() => durumGuncelle());
  komutlar.dinle(() => sonucGuncelle());
  durumGuncelle();
  return { kok };
}
