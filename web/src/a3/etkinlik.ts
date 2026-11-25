// Çalıştırma denetimi düğmelerinin etkinliği: A3 Tablo 4.7a (durum × komut) ve
// Tablo 4.7b (kipe göre komutlar). Saf işlevdir; girdisi yalnız sunucunun bildirdiği
// durum ve kiptir (W5-K44). Etkin olmayan düğme nedenini söyler.

import type { CalistirmaDurumAdi, KipAdi } from './iletiler';

export type DugmeAdi = 'baslat' | 'duraklat' | 'surdur' | 'adimla' | 'hiz' | 'durdur' | 'sifirla' | 'yukle';

export const DUGME_ADLARI: readonly DugmeAdi[] = ['baslat', 'duraklat', 'surdur', 'adimla', 'hiz', 'durdur', 'sifirla', 'yukle'];

export interface DugmeDurumu {
  etkin: boolean;
  /** Etkin değilse nedeni; etkinse boş (ya da uyarı: HIL'de duraklatma). */
  neden: string;
  /** `duraklat` HIL kipinde `gecersizligi_onayla: true` ister (Tablo 4.7b). */
  onay: 'gecersizlik' | null;
}

/** Tablo 4.7a: durum → o durumda geçerli komutlar ("—" olmayan hücreler). */
const GECERLI: Readonly<Record<CalistirmaDurumAdi, readonly DugmeAdi[]>> = {
  bos: ['yukle'],
  yuklendi: ['yukle', 'baslat', 'sifirla'],
  hazirlaniyor: ['durdur', 'sifirla'],
  kosuyor: ['duraklat', 'hiz', 'durdur', 'sifirla'],
  duraklatildi: ['surdur', 'adimla', 'hiz', 'durdur', 'sifirla'],
  bitti: ['yukle', 'sifirla'],
  durduruldu: ['yukle', 'sifirla'],
  hata: ['yukle', 'sifirla'],
  izleme: [],
};

const DURUM_ADI: Readonly<Record<CalistirmaDurumAdi, string>> = {
  bos: 'boş (senaryo yüklenmedi)',
  yuklendi: 'yüklendi',
  hazirlaniyor: 'hazırlanıyor (ön denetim sürüyor)',
  kosuyor: 'koşuyor',
  duraklatildi: 'duraklatıldı',
  bitti: 'bitti',
  durduruldu: 'durduruldu',
  hata: 'hata',
  izleme: 'izleme',
};

const GEREKEN: Readonly<Record<DugmeAdi, string>> = {
  baslat: 'yüklenmiş bir senaryo gerekir (durum: yüklendi)',
  duraklat: 'çalıştırma koşarken geçerlidir',
  surdur: 'çalıştırma duraklatılmışken geçerlidir',
  adimla: 'çalıştırma duraklatılmışken geçerlidir',
  hiz: 'çalıştırma koşarken ya da duraklatılmışken geçerlidir',
  durdur: 'çalıştırma hazırlanırken, koşarken ya da duraklatılmışken geçerlidir',
  sifirla: 'çalıştırma izlemede ya da hiç yüklenmemişken geçerli değildir',
  yukle: 'senaryo yalnız çalıştırma yokken ya da bittikten sonra yüklenir',
};

const HIL: readonly KipAdi[] = ['hil_1x', 'hil_bariyer'];

/**
 * Her düğme için etkinlik. `durum`/`kip` null ise sunucudan henüz durum gelmemiştir:
 * hiçbir düğme etkin değildir (ekran sunucunun söylediğinden fazlasını varsaymaz).
 */
export function dugmeler(durum: CalistirmaDurumAdi | null, kip: KipAdi | null): Record<DugmeAdi, DugmeDurumu> {
  const sonuc = {} as Record<DugmeAdi, DugmeDurumu>;
  for (const d of DUGME_ADLARI) {
    if (durum === null || kip === null) {
      sonuc[d] = { etkin: false, neden: 'sunucudan henüz durum gelmedi', onay: null };
      continue;
    }
    if (durum === 'izleme') {
      sonuc[d] = { etkin: false, neden: 'izleme sürüyor; canlı çalıştırma komutları verilemez (izleme_etkin)', onay: null };
      continue;
    }
    if (!GECERLI[durum].includes(d)) {
      sonuc[d] = { etkin: false, neden: `durum ${DURUM_ADI[durum]}; ${GEREKEN[d]}`, onay: null };
      continue;
    }
    const hil = HIL.includes(kip);
    if (hil && (d === 'adimla' || d === 'hiz')) {
      sonuc[d] = { etkin: false, neden: `HIL kipinde (${kip}) ${d === 'adimla' ? 'adımlama' : 'hız değişimi'} yok; yalnız 1×`, onay: null };
      continue;
    }
    if (d === 'duraklat' && hil) {
      sonuc[d] = { etkin: true, neden: 'HIL kipinde duraklatma çalıştırmayı geçersiz kılar; onay istenir', onay: 'gecersizlik' };
      continue;
    }
    sonuc[d] = { etkin: true, neden: '', onay: null };
  }
  return sonuc;
}
