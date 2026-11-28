// Panel 7 — Emniyet girdileri ve yer istasyonu komutu (W5-web-emniyet).
//
// Emniyet kararını sunucu verir; bu panel istek gönderir ve sunucunun bildirdiğini gösterir:
//  - Tablodaki "yetkili" ve "kart" sütunları yalnız `emniyet/girdiler` çerçevesinden gelir
//    (W5-K51); bir düğmeye basmak bu değerleri değiştirmez.
//  - Emniyeti artıran yön (acil durdurmayı bas, fişi çıkar, pimi tak) tek adımdır.
//  - Emniyeti azaltan yön iki adımdır: "Gevşet…" onay jetonu ister; ardından onay kutusu
//    (önceden işaretli gelmez) işaretlenip "Gönder"e basılır. Jeton süresi dolarsa, revizyon
//    değişirse, bağlantı kopar ya da istek reddedilirse iki adım baştan yapılır; hiçbir şey
//    kendiliğinden yinelenmez (W5-K52). Klavye kısayolu ve toplu eylem yoktur (W5-K53).
//  - Yer istasyonu komutları yalnız sunucunun listelediği komutlardır; atış isteği komutları
//    gösterilmez (W5-K54, A3-K99).
// Onay sayfa içindedir; tarayıcı `confirm`/`alert` kutusu kullanılmaz.

import { sonucMetni, type GirdiAdi, type HedefDurum, type KomutTanimi, type Secim, type Tup } from '../a3/emniyet-istek';
import { EmniyetIstekleri } from '../a3/emniyet-istek';
import type { KomutKaydi } from '../a3/komut';
import { tamsayi } from '../ortak/bicim';
import { el, kart, sinifSec, yaz } from '../ortak/dom';
import { girdiGosterimi } from '../ortak/emniyet';
import { olayGunlugu } from '../ortak/gunluk';
import { panelBaslat } from '../ortak/kabuk';
import { formAlanlari, parametreleriOku, type FormAlani } from '../ortak/sema-form';

interface SatirTanimi {
  ad: string;
  girdi: GirdiAdi;
  tup?: Tup;
  /** Emniyeti artıran yönün hedefi ve düğme yazısı. */
  artir: { hedef: HedefDurum; yazi: string };
  /** Emniyeti azaltan yönün hedefi ve düğme yazısı. */
  azalt: { hedef: HedefDurum; yazi: string };
}

const SATIRLAR: readonly SatirTanimi[] = [
  { ad: 'Acil durdurma', girdi: 'acil_durdurma', artir: { hedef: 'basili', yazi: 'Acil durdurmaya bas' }, azalt: { hedef: 'serbest', yazi: 'Serbest bırak…' } },
  { ad: 'Silahlandırma fişi · f1', girdi: 'silahlandirma_fisi', tup: 'f1', artir: { hedef: 'cikarilmis', yazi: 'Fişi çıkar' }, azalt: { hedef: 'takili', yazi: 'Fişi tak…' } },
  { ad: 'Silahlandırma fişi · f2', girdi: 'silahlandirma_fisi', tup: 'f2', artir: { hedef: 'cikarilmis', yazi: 'Fişi çıkar' }, azalt: { hedef: 'takili', yazi: 'Fişi tak…' } },
  { ad: 'Emniyet pimi · f1', girdi: 'emniyet_pimi', tup: 'f1', artir: { hedef: 'takili', yazi: 'Pimi tak' }, azalt: { hedef: 'cikarilmis', yazi: 'Pimi çıkar…' } },
  { ad: 'Emniyet pimi · f2', girdi: 'emniyet_pimi', tup: 'f2', artir: { hedef: 'takili', yazi: 'Pimi tak' }, azalt: { hedef: 'cikarilmis', yazi: 'Pimi çıkar…' } },
];

const SINIFLAR = ['kisitlayan', 'gevsek', 'yok', 'gecersiz', 'bilinmiyor'];
const secimOf = (s: SatirTanimi, hedef: HedefDurum): Secim => (s.tup === undefined ? { girdi: s.girdi, hedef } : { girdi: s.girdi, tup: s.tup, hedef });

function sonucSatiri(k: KomutKaydi, yer = false): HTMLElement {
  const m = sonucMetni(k);
  const baslik = yer && k.asama === 'uygulandi' ? 'SUNUCU İŞLEDİ' : m.baslik;
  const ayrinti = yer && k.asama === 'uygulandi' ? 'komut emülatör hattına verildi; kartın yanıtı ayrı olayla gelir (A3-K102)' : m.ayrinti;
  return el(
    'li',
    { class: 'islem' },
    el('span', { class: 'zaman' }, new Date(k.gonderildi).toLocaleTimeString('tr-TR')),
    el('span', { class: `cip ${m.sinif}` }, baslik),
    el('span', { class: 'islem-metin' }, k.ozet, el('small', null, ayrinti)),
  );
}

panelBaslat(
  'emniyet',
  ({ baglanti: b, komutlar, emniyet: em, icerik }) => {
    icerik.classList.add('emniyet-izgara');

    // ---- 1. Sanal emniyet girdileri ----
    const revEl = el('span', { class: 'rev', title: 'Emniyet revizyonu (A3-K117); sunucudan' }, 'rev —');
    const farkCip = el('span', { class: 'cip notr' }, '—');
    const girdi = kart('Sanal emniyet girdileri · sunucunun bildirdiği durum', farkCip, revEl);
    const govde = el('tbody');
    const satirlar = SATIRLAR.map((s) => {
      const yetkili = el('td', { class: 'durum-hucre' }, '—');
      const kartD = el('td', { class: 'durum-hucre' }, '—');
      const fark = el('td', null, '—');
      const artir = el('button', { class: 'dugme emniyet-artir', type: 'button', 'data-girdi': s.girdi, 'data-tup': s.tup ?? '', 'data-yon': 'artiran' }, s.artir.yazi);
      const azalt = el('button', { class: 'dugme emniyet-azalt', type: 'button', 'data-girdi': s.girdi, 'data-tup': s.tup ?? '', 'data-yon': 'azaltan' }, s.azalt.yazi);
      artir.addEventListener('click', () => void em.artir(secimOf(s, s.artir.hedef)));
      azalt.addEventListener('click', () => void em.jetonIste(secimOf(s, s.azalt.hedef)).then((r) => 'hata' in r && yaz(jetonHata, `UYGULANMADI — onay jetonu alınamadı: ${r.hata.ileti}`)));
      govde.append(el('tr', null, el('td', null, s.ad), yetkili, kartD, fark, el('td', { class: 'eylem' }, artir, azalt)));
      return { yetkili, kartD, fark };
    });
    const girdiAlt = el('div', { class: 'aciklama-satiri' }, 'Sunucu henüz bildirmedi.');
    girdi.govde.append(
      el(
        'table',
        { class: 'tablo girdi-tablo' },
        el('thead', null, el('tr', null, el('th', null, 'Girdi'), el('th', null, 'Yetkili · sim'), el('th', null, 'Kart geri okuması'), el('th', null, 'Fark'), el('th', null, 'İstek'))),
        govde,
      ),
      girdiAlt,
      el('div', { class: 'aciklama-satiri' }, 'Düğmeye basmak bu tabloyu değiştirmez: değerler yalnız sunucu bildirince değişir. "…" ile biten düğmeler emniyeti azaltır ve iki adım ister.'),
    );

    // ---- 2. Gevşetme onayı (iki adımlı) ----
    const jetonHata = el('div', { class: 'onay-ileti', role: 'status' });
    const onay = kart('Gevşetme onayı');
    const onayIc = el('div', { class: 'onay-ic' });
    onay.govde.append(onayIc, jetonHata);
    let gosterilenJeton: string | null = null;

    const onayCiz = (): void => {
      const j = em.jeton();
      const yerJeton = j !== null && j.tur === 'yer_istasyonu';
      if (j === null || j.tur !== 'emniyet') {
        if (gosterilenJeton !== null || onayIc.childElementCount === 0) {
          gosterilenJeton = null;
          const neden = em.jetonNedeni();
          onayIc.replaceChildren(el('div', { class: 'aciklama-satiri' }, yerJeton ? 'Bekleyen onay yer istasyonu komutu içindir (aşağıda).' : 'Bekleyen gevşetme yok.'));
          if (neden !== '') onayIc.append(el('div', { class: 'onay-ileti uyari' }, neden));
        } else {
          const n = onayIc.querySelector('.onay-ileti.uyari');
          const neden = em.jetonNedeni();
          if (n !== null) yaz(n, neden);
        }
        return;
      }
      if (gosterilenJeton !== j.jeton) {
        // Yeni jeton: onay kutusu her zaman işaretsiz başlar (W5-K53).
        gosterilenJeton = j.jeton;
        jetonHata.textContent = '';
        const kutu = el('input', { type: 'checkbox', id: 'gevset-onay', class: 'onay-kutu' });
        const gonder = el('button', { class: 'dugme onay-gonder', type: 'button', disabled: true }, 'Gönder (emniyeti azalt)');
        const vazgec = el('button', { class: 'dugme', type: 'button' }, 'Vazgeç');
        kutu.addEventListener('change', () => {
          gonder.disabled = !kutu.checked;
        });
        gonder.addEventListener('click', () => {
          if (j.secim === undefined) return;
          void em.gevset(j.secim, kutu.checked);
        });
        vazgec.addEventListener('click', () => em.vazgec());
        onayIc.replaceChildren(
          el('div', { class: 'onay-baslik' }, `Onay bekleniyor: ${j.ozet}`),
          el('div', { class: 'onay-kalan aciklama-satiri' }, ''),
          el('label', { class: 'onay-etiket', for: 'gevset-onay' }, kutu, ' Bu değişikliğin emniyeti azalttığını anlıyorum ve onaylıyorum.'),
          el('div', { class: 'onay-dugmeler' }, gonder, vazgec),
        );
      }
      const kalan = onayIc.querySelector('.onay-kalan');
      if (kalan !== null) yaz(kalan, `Jeton tek kullanımlıktır; ${Math.ceil(em.kalanMs() / 1000)} s içinde gönderilmezse düşer ve iki adım baştan yapılır.`);
    };

    // ---- 3. Son işlemler ----
    const islemler = kart('Son emniyet istekleri · sonuç');
    const islemListe = el('ul', { class: 'islem-liste', 'aria-live': 'polite' });
    islemler.govde.append(islemListe);
    const islemCiz = (): void => {
      const l = komutlar.kayitlar().filter((k) => k.komut === 'emniyet_degistir').slice(0, 8);
      islemListe.replaceChildren(...(l.length > 0 ? l.map((k) => sonucSatiri(k)) : [el('li', { class: 'gunluk-bos' }, 'Bu oturumda emniyet isteği gönderilmedi.')]));
    };

    // ---- 4. Emniyet olayları ----
    const gunluk = olayGunlugu(b, 200, { suzgec: (o) => o.onem === 'emniyet' || o.tur === 'emniyet_girdisi_degisti' || o.tur === 'gevsetme_reddedildi' || o.tur === 'emniyet_rev_doydu' });
    const olay = kart('Emniyet olayları', gunluk.sayac);
    olay.govde.classList.add('sifir', 'kaydir');
    olay.govde.append(gunluk.liste);

    // ---- 5. Yer istasyonu komutu ----
    const yer = kart('Yer istasyonu komutu');
    const listeDurum = el('div', { class: 'aciklama-satiri' }, 'Komut listesi sunucudan alınıyor…');
    const secici = el('select', { class: 'denetim-girdi yer-secici', 'aria-label': 'Yer istasyonu komutu', disabled: true });
    const alanKutu = el('div', { class: 'yer-alanlar' });
    const yerOnay = el('div', { class: 'onay-ic' });
    const yerEylem = el('div', { class: 'onay-dugmeler' });
    const yerIleti = el('div', { class: 'onay-ileti', role: 'status' });
    const yerIslemListe = el('ul', { class: 'islem-liste', 'aria-live': 'polite' });
    const yerGunluk = olayGunlugu(b, 60, { suzgec: (o) => o.tur === 'yer_istasyonu_komutu' || o.tur === 'yer_istasyonu_yaniti' });
    yer.bas.append(yerGunluk.sayac);
    yer.govde.append(listeDurum, secici, alanKutu, yerOnay, yerEylem, yerIleti, el('div', { class: 'aciklama-satiri' }, 'Son yer istasyonu komutları'), yerIslemListe, yerGunluk.liste);
    yerGunluk.liste.classList.add('yer-gunluk');

    let katalog: KomutTanimi[] = [];
    let alanlar: { alan: FormAlani; girdi: HTMLInputElement | HTMLSelectElement }[] = [];
    let desteklenmiyor: string | null = null;

    const seciliKomut = (): KomutTanimi | null => katalog.find((k) => k.ad === secici.value) ?? null;
    const metinler = (): Record<string, string> => {
      const m: Record<string, string> = {};
      for (const { alan, girdi: g } of alanlar) m[alan.ad] = g instanceof HTMLInputElement && g.type === 'checkbox' ? String(g.checked) : g.value;
      return m;
    };

    const alanlariCiz = (): void => {
      alanlar = [];
      desteklenmiyor = null;
      alanKutu.replaceChildren();
      yerIleti.textContent = '';
      const k = seciliKomut();
      if (k === null) return;
      const f = formAlanlari(k.parametreler);
      if ('desteklenmiyor' in f) {
        desteklenmiyor = f.desteklenmiyor;
        alanKutu.append(el('div', { class: 'onay-ileti uyari' }, `Bu komut bu formda gönderilemez: ${f.desteklenmiyor}.`));
        return;
      }
      for (const a of f.alanlar) {
        let g: HTMLInputElement | HTMLSelectElement;
        if (a.tur === 'secim') {
          g = el('select', { class: 'denetim-girdi', id: `yer-${a.ad}` }, ...(a.secenekler ?? []).map((s) => el('option', { value: s }, s)));
          if (!a.zorunlu) g.prepend(el('option', { value: '' }, '— verilmesin —'));
          g.value = a.zorunlu ? (a.secenekler?.[0] ?? '') : '';
        } else if (a.tur === 'mantik') {
          g = el('input', { type: 'checkbox', id: `yer-${a.ad}` });
        } else {
          g = el('input', { type: 'text', class: 'denetim-girdi genis', id: `yer-${a.ad}`, inputmode: a.tur === 'metin' ? 'text' : 'decimal', placeholder: a.tur === 'liste' ? 'virgülle ayrılmış' : '' });
        }
        alanKutu.append(
          el('label', { class: 'yer-alan', for: `yer-${a.ad}`, title: a.aciklama }, el('span', { class: 'yer-alan-ad' }, a.ad + (a.zorunlu ? ' *' : '')), g, el('small', { class: 'yer-alan-hata', 'data-alan': a.ad }, '')),
        );
        alanlar.push({ alan: a, girdi: g });
      }
    };

    let yerGosterilenJeton: string | null | undefined;
    const yerCiz = (): void => {
      const k = seciliKomut();
      const j = em.jeton();
      const buJeton = j !== null && j.tur === 'yer_istasyonu' && k !== null && j.komut === k.ad ? j : null;
      for (const { girdi: g } of alanlar) g.disabled = buJeton !== null; // jeton alındıktan sonra parametreler dondurulur
      const l = komutlar.kayitlar().filter((x) => x.komut === 'yer_istasyonu_komut').slice(0, 5);
      yerIslemListe.replaceChildren(...(l.length > 0 ? l.map((x) => sonucSatiri(x, true)) : [el('li', { class: 'gunluk-bos' }, 'Bu oturumda yer istasyonu komutu gönderilmedi.')]));
      const anahtar = buJeton?.jeton ?? null;
      if (anahtar !== yerGosterilenJeton || yerEylem.childElementCount === 0) {
        yerGosterilenJeton = anahtar;
        yerOnay.replaceChildren();
        yerEylem.replaceChildren();
        if (k === null || desteklenmiyor !== null) return;
        if (!EmniyetIstekleri.ikiAdimli(k)) {
          const gonder = el('button', { class: 'dugme', type: 'button' }, 'Gönder');
          gonder.addEventListener('click', () => yerGonder(false));
          yerEylem.append(gonder);
        } else if (buJeton === null) {
          const jetonDugme = el('button', { class: 'dugme', type: 'button' }, 'Onay jetonu iste (1. adım)');
          jetonDugme.addEventListener('click', () => void yerJetonIste());
          yerEylem.append(jetonDugme);
          yerOnay.append(el('div', { class: 'aciklama-satiri' }, 'Bu komut izni genişletir: iki adım gerekir (onay jetonu, sonra onay kutusu).'));
        } else {
          const kutu = el('input', { type: 'checkbox', id: 'yer-onay', class: 'onay-kutu' });
          const gonder = el('button', { class: 'dugme', type: 'button', disabled: true }, 'Gönder (2. adım)');
          const vazgec = el('button', { class: 'dugme', type: 'button' }, 'Vazgeç');
          kutu.addEventListener('change', () => {
            gonder.disabled = !kutu.checked;
          });
          gonder.addEventListener('click', () => yerGonder(kutu.checked));
          vazgec.addEventListener('click', () => em.vazgec());
          yerOnay.append(
            el('div', { class: 'onay-baslik' }, `Onay bekleniyor: ${buJeton.ozet}`),
            el('div', { class: 'onay-kalan aciklama-satiri' }, ''),
            el('label', { class: 'onay-etiket', for: 'yer-onay' }, kutu, ' Bu komutun yetkiyi genişlettiğini anlıyorum ve onaylıyorum.'),
          );
          yerEylem.append(gonder, vazgec);
        }
      }
      const kalan = yerOnay.querySelector('.onay-kalan');
      if (kalan !== null && buJeton !== null) yaz(kalan, `Jeton tek kullanımlıktır; ${Math.ceil(em.kalanMs() / 1000)} s içinde gönderilmezse düşer ve iki adım baştan yapılır.`);
      if (buJeton === null && k !== null && EmniyetIstekleri.ikiAdimli(k) && em.jetonNedeni() !== '' && yerIleti.textContent === '') yerIleti.textContent = em.jetonNedeni();
    };

    const parametreOku = (): Record<string, unknown> | null => {
      const r = parametreleriOku(alanlar.map((x) => x.alan), metinler());
      for (const h of alanKutu.querySelectorAll('.yer-alan-hata')) h.textContent = '';
      if ('hatalar' in r) {
        for (const [ad, ileti] of Object.entries(r.hatalar)) {
          const h = alanKutu.querySelector(`.yer-alan-hata[data-alan="${ad}"]`);
          if (h !== null) h.textContent = ileti;
        }
        yerIleti.textContent = 'UYGULANMADI — parametrelerde hata var; komut gönderilmedi.';
        return null;
      }
      yerIleti.textContent = '';
      return r.parametreler;
    };

    async function yerJetonIste(): Promise<void> {
      const k = seciliKomut();
      const p = parametreOku();
      if (k === null || p === null) return;
      const r = await em.yerJetonIste(k.ad, p);
      if ('hata' in r) yerIleti.textContent = `UYGULANMADI — onay jetonu alınamadı: ${r.hata.ileti}`;
    }

    function yerGonder(onayVerildi: boolean): void {
      const k = seciliKomut();
      const j = em.jeton();
      // Jetonlu komutta parametreler jeton alınırken dondurulmuştur; gönderilen onlardır.
      const p = j !== null && j.tur === 'yer_istasyonu' && j.komut === k?.ad ? (j.parametreler ?? {}) : parametreOku();
      if (k === null || p === null) return;
      void em.yerGonder(k.ad, p, onayVerildi);
    }

    secici.addEventListener('change', () => {
      alanlariCiz();
      yerGosterilenJeton = undefined;
      yerCiz();
    });
    const katalogYukle = async (): Promise<void> => {
      const r = await em.yerKomutlari();
      if ('hata' in r) {
        listeDurum.textContent = `Komut listesi alınamadı: ${r.hata.ileti}`;
        listeDurum.classList.add('uyari');
        return;
      }
      katalog = r.komutlar;
      listeDurum.classList.remove('uyari');
      listeDurum.textContent =
        `Sunucunun listesi: ${katalog.length} komut.` + (r.gizlenen.length > 0 ? ` Atış isteği komutları bu panelde gösterilmez (${r.gizlenen.join(', ')}).` : '');
      secici.replaceChildren(
        ...katalog.map((k) => {
          const f = formAlanlari(k.parametreler);
          const etiket = `${k.ad} — ${k.sinif === 'daraltici' ? 'daraltıcı' : k.sinif === 'genisletici' ? 'genişletici · iki adım' : 'nötr'}${'desteklenmiyor' in f ? ' · form yok' : ''}`;
          return el('option', { value: k.ad }, etiket);
        }),
      );
      secici.disabled = katalog.length === 0;
      alanlariCiz();
      yerGosterilenJeton = undefined;
      yerCiz();
    };
    // Bağlantı kurulunca (ya da yeniden kurulunca) liste yeniden alınır; sunucunun listesi esastır.
    let listeAlindi = false;
    b.durumDinle((bi) => {
      if (bi.durum === 'bagli' && !listeAlindi) {
        listeAlindi = true;
        void katalogYukle();
      } else if (bi.durum !== 'bagli') listeAlindi = false;
    });

    // ---- Yerleşim ----
    for (const [k, a] of [
      [girdi, 'girdi'],
      [onay, 'onay'],
      [islemler, 'islem'],
      [olay, 'olay'],
      [yer, 'yer'],
    ] as const) {
      k.kok.style.gridArea = a;
      icerik.append(k.kok);
    }

    // ---- Veri: yalnız sunucunun bildirdikleri ----
    const FARK_METNI: Record<string, string> = { esit: 'eşit', bilinmiyor: 'bilinmiyor', gecersiz: 'geçersiz', fark: 'FARK' };
    b.abone(
      'emniyet/girdiler',
      (c) => {
        const y = c.yuk;
        const g = girdiGosterimi(y, c.gecersiz);
        g.satirlar.forEach((gs, i) => {
          const s = satirlar[i]!;
          yaz(s.yetkili, gs.yetkili.metin);
          yaz(s.kartD, gs.kart.metin);
          sinifSec(s.yetkili, SINIFLAR, gs.yetkili.sinif);
          sinifSec(s.kartD, SINIFLAR, gs.kart.sinif);
          yaz(s.fark, FARK_METNI[gs.fark] ?? gs.fark);
          sinifSec(s.fark, ['fark-cip'], gs.fark === 'fark' || gs.fark === 'gecersiz' ? 'fark-cip' : null);
        });
        yaz(farkCip, g.ozet.metin);
        sinifSec(farkCip, ['iyi', 'kotu', 'notr', 'uyari'], g.ozet.sinif);
        yaz(revEl, `rev ${y.emniyet_rev === null ? '—' : tamsayi(y.emniyet_rev)}${y.emniyet_rev_doydu === 1 ? ' · DOYDU (gevşetme kabul edilmez)' : ''}`);
        yaz(girdiAlt, `son değişim: ${y.son_degisim_adimi === null ? 'yok' : `adım ${tamsayi(y.son_degisim_adimi)}`} · alım zamanı sunucu saatiyle`);
      },
      10,
    );

    const hepsiniCiz = (): void => {
      onayCiz();
      islemCiz();
      yerCiz();
    };
    em.dinle(hepsiniCiz);
    komutlar.dinle(hepsiniCiz);
    // Jeton geri sayımı: yalnız jeton varken ekranı tazeler.
    setInterval(() => {
      if (em.jeton() !== null) hepsiniCiz();
    }, 500);
    hepsiniCiz();
  },
  { gecmisOlay: 100 },
);
