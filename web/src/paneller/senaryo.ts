// Panel 6 — Senaryo (W5-web-komut; A3 §3.6, §3.8, §4.9).
//
// Dosya listesi, seçilen dosyanın metni (salt okunur), doğrulama (A5 raporu), yükleme,
// yüklü çözümlenmiş senaryonun davranış özeti ve ön denetim raporu. Form tabanlı düzenleyici
// ve dosya yazma yoktur (kapsam dışı). Bir uç sunucuda yoksa (404 `bulunamadi` ya da 405)
// ilgili bölüm "sunucu bu ucu sunmuyor" der ve yeniden denemez (W5-K45); çalıştırma denetimi
// bu panele bağlı değildir. Gösterilen her şey sunucunun yanıtıdır (W5-K44).

import { A3Hatasi } from '../a3/http';
import { dugmeler } from '../a3/etkinlik';
import { hatadanIleti, ucYok } from '../a3/hata';
import type { A5Raporu, CalistirmaDurumAdi, KipAdi, OnDenetimRaporu, SenaryoListeOgesi } from '../a3/iletiler';
import { YUKLER } from '../a3/katalog';
import { el, kart, sinifSec, yaz } from '../ortak/dom';
import { panelBaslat } from '../ortak/kabuk';
import { bulguKonumu, davranisOzeti, satirOnemleri, sayiMetni } from './senaryo-ozet';

type DegerHaritasi = { degerler: Record<number, string> };
const DURUM_ADLARI = (YUKLER.yuk_calistirma_durum.find((a) => a.ad === 'durum') as unknown as DegerHaritasi).degerler;
const KIP_ADLARI = (YUKLER.yuk_calistirma_durum.find((a) => a.ad === 'kip') as unknown as DegerHaritasi).degerler;

const YOK_METNI = 'Sunucu bu ucu sunmuyor (404 / 405); bu bölüm kullanılamıyor.';
const ONDENETIM_DURUM: Record<string, { ad: string; sinif: string }> = {
  gecti: { ad: 'geçti', sinif: 'iyi' },
  kaldi: { ad: 'kaldı', sinif: 'kotu' },
  uyari: { ad: 'uyarı', sinif: 'uyari' },
  atlandi: { ad: 'atlandı', sinif: 'notr' },
};

panelBaslat(
  'senaryo',
  ({ baglanti: b, komutlar, icerik }) => {
    icerik.classList.add('senaryo-izgara');
    const http = b.http;

    // ---- Durum (yalnız sunucudan) ----
    let durum: CalistirmaDurumAdi | null = null;
    let kip: KipAdi | null = null;
    b.abone('calistirma/durum', (c) => {
      durum = (DURUM_ADLARI[c.yuk.durum] ?? null) as CalistirmaDurumAdi | null;
      kip = (KIP_ADLARI[c.yuk.kip] ?? null) as KipAdi | null;
      dugmeleriGuncelle();
    });

    // Sunucunun sunmadığı uçlar: ilk 404/405'ten sonra yeniden denenmez.
    const yok = { liste: false, cozumlenmis: false };

    // ---- 1. Dosya listesi ----
    const listeEl = el('ul', { class: 'senaryo-liste', role: 'listbox', 'aria-label': 'Senaryo dosyaları' });
    const listeIleti = el('div', { class: 'senaryo-ileti' });
    const dogrulaDugme = el('button', { class: 'dugme', type: 'button', id: 'dogrula' }, 'Doğrula');
    const yukleDugme = el('button', { class: 'dugme', type: 'button', id: 'yukle' }, 'Yükle');
    const eylemIleti = el('div', { class: 'senaryo-ileti' });
    const kListe = kart('Senaryo dosyaları');
    kListe.govde.append(listeEl, listeIleti, el('div', { class: 'senaryo-eylem' }, dogrulaDugme, yukleDugme), eylemIleti);

    // ---- 2. Dosya metni ----
    const metinEl = el('pre', { class: 'senaryo-metin', tabindex: '0', 'aria-label': 'Senaryo dosyası metni (salt okunur)' });
    const metinBaslik = el('span', { class: 'kart-not' }, 'salt okunur');
    const kMetin = kart('Dosya metni', metinBaslik);
    kMetin.govde.append(metinEl);

    // ---- 3. Doğrulama raporu ----
    const raporOzet = el('div', { class: 'senaryo-rapor-ozet' });
    const raporTablo = el('div', { class: 'senaryo-bulgular' });
    const kRapor = kart('Doğrulama raporu (A5)');
    kRapor.govde.append(raporOzet, raporTablo);

    // ---- 4. Yüklü senaryo ----
    const yukluOzet = el('div', { class: 'senaryo-yuklu' });
    const yukluJson = el('pre', { class: 'senaryo-json' });
    const yukluAyrinti = el('details', { class: 'senaryo-ayrinti' }, el('summary', null, 'Çözümlenmiş senaryo (A5 kanonik JSON)'), yukluJson);
    const kYuklu = kart('Yüklü senaryo');
    kYuklu.govde.append(yukluOzet, yukluAyrinti);

    // ---- 5. Ön denetim ----
    const odOzet = el('div', { class: 'senaryo-rapor-ozet' });
    const odTablo = el('div', { class: 'senaryo-od' });
    const kOd = kart('Ön denetim raporu');
    kOd.govde.append(odOzet, odTablo);

    kListe.kok.classList.add('a-liste');
    kMetin.kok.classList.add('a-metin');
    kRapor.kok.classList.add('a-rapor');
    kYuklu.kok.classList.add('a-yuklu');
    kOd.kok.classList.add('a-od');
    icerik.append(kListe.kok, kMetin.kok, kRapor.kok, kYuklu.kok, kOd.kok);

    // ---- Durum değişkenleri ----
    let secili: string | null = null;
    let dosyalar: SenaryoListeOgesi[] = [];

    const mesaj = (e: HTMLElement, m: string, sinif: 'iyi' | 'uyari' | 'kotu' | null = null): void => {
      yaz(e, m);
      sinifSec(e, ['iyi', 'uyari', 'kotu'], sinif);
    };

    function dugmeleriGuncelle(): void {
      const g = dugmeler(durum, kip);
      const baglanti = b.bilgi.durum === 'bagli';
      const ucVar = !yok.liste;
      const ayarla = (d: HTMLButtonElement, etkin: boolean, neden: string): void => {
        d.setAttribute('aria-disabled', etkin ? 'false' : 'true');
        d.classList.toggle('kapali', !etkin);
        d.title = etkin ? '' : `Şu an kullanılamaz: ${neden}`;
      };
      ayarla(dogrulaDugme, baglanti && ucVar && secili !== null, !baglanti ? 'sunucuyla bağlantı yok' : !ucVar ? YOK_METNI : 'önce bir dosya seçin');
      const y = g.yukle;
      ayarla(yukleDugme, baglanti && ucVar && secili !== null && y.etkin, !baglanti ? 'sunucuyla bağlantı yok' : !ucVar ? YOK_METNI : secili === null ? 'önce bir dosya seçin' : y.neden);
    }

    // ---- Liste ----
    function listeCiz(): void {
      listeEl.replaceChildren();
      for (const d of dosyalar) {
        const li = el('li', { class: 'senaryo-oge', role: 'option', tabindex: '0', 'data-ad': d.ad, 'aria-selected': d.ad === secili ? 'true' : 'false' }, el('span', { class: 'sabit' }, d.ad), el('small', null, d.boyut_bayt === undefined ? '' : `${d.boyut_bayt} B`));
        const sec = (): void => void dosyaSec(d.ad);
        li.addEventListener('click', sec);
        li.addEventListener('keydown', (e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            sec();
          }
        });
        listeEl.append(li);
      }
    }

    async function listele(): Promise<void> {
      if (yok.liste) return;
      try {
        const y = await http.senaryolar();
        dosyalar = y.senaryolar;
        mesaj(listeIleti, dosyalar.length === 0 ? 'Sunucuda senaryo dosyası yok.' : '');
        listeCiz();
        if (secili === null && dosyalar.length > 0) await dosyaSec(dosyalar[0]!.ad);
      } catch (h) {
        if (ucYok(h)) {
          yok.liste = true;
          mesaj(listeIleti, YOK_METNI, 'uyari');
          mesaj(eylemIleti, '');
        } else mesaj(listeIleti, h instanceof A3Hatasi ? hatadanIleti(h) : String(h), 'kotu');
      }
      dugmeleriGuncelle();
    }

    // ---- Dosya metni ----
    function metniCiz(metin: string, bulgular: A5Raporu['bulgular'] = []): void {
      const onem = satirOnemleri(bulgular);
      metinEl.replaceChildren();
      metin.split('\n').forEach((s, i) => {
        const o = onem.get(i + 1);
        metinEl.append(el('span', { class: `satir${o === undefined ? '' : ' ' + o}`, 'data-satir': String(i + 1) }, el('span', { class: 'no' }, String(i + 1)), s + '\n'));
      });
    }

    let dosyaMetni = '';
    async function dosyaSec(ad: string): Promise<void> {
      secili = ad;
      listeCiz();
      raporOzet.replaceChildren();
      raporTablo.replaceChildren();
      mesaj(eylemIleti, '');
      try {
        const y = await http.senaryoDosyasi(ad);
        if (secili !== ad) return;
        dosyaMetni = y.icerik;
        yaz(metinBaslik, `${ad} · salt okunur`);
        metniCiz(y.icerik);
      } catch (h) {
        dosyaMetni = '';
        metinEl.replaceChildren();
        mesaj(eylemIleti, h instanceof A3Hatasi ? (ucYok(h) ? 'Dosya sunucuda yok ya da sunucu bu ucu sunmuyor.' : hatadanIleti(h)) : String(h), 'kotu');
      }
      dugmeleriGuncelle();
    }

    // ---- Doğrulama raporu ----
    function raporCiz(r: A5Raporu, baslik: string): void {
      raporOzet.replaceChildren(
        el('span', { class: `cip ${r.gecerli ? 'iyi' : 'kotu'}` }, r.gecerli ? 'GEÇERLİ' : 'GEÇERSİZ'),
        el('span', { class: 'senaryo-ozet-metin' }, `${baslik} · ${r.bulgular.filter((x) => x.onem === 'hata').length} hata, ${r.bulgular.filter((x) => x.onem === 'uyari').length} uyarı`),
      );
      if (r.davranis_ozeti) raporOzet.append(el('small', { class: 'sabit', title: 'A5 davranış özeti' }, `sha256:${r.davranis_ozeti.slice(0, 12)}…`));
      raporTablo.replaceChildren();
      if (r.bulgular.length === 0) {
        raporTablo.append(el('div', { class: 'senaryo-ileti' }, 'Bulgu yok.'));
      } else {
        const t = el('table', { class: 'tablo' }, el('thead', null, el('tr', null, ...['Önem', 'Kural', 'Konum (yol · satır)', 'İleti'].map((x) => el('th', null, x)))));
        const g = el('tbody');
        for (const x of r.bulgular) {
          g.append(el('tr', null, el('td', null, el('span', { class: `cip ${x.onem === 'hata' ? 'kotu' : 'uyari'}` }, x.onem === 'hata' ? 'hata' : 'uyarı')), el('td', { class: 'sabit' }, x.kural), el('td', { class: 'sabit' }, bulguKonumu(x)), el('td', null, x.ileti)));
        }
        t.append(g);
        raporTablo.append(t);
      }
      metniCiz(dosyaMetni, r.bulgular);
    }

    dogrulaDugme.addEventListener('click', () => {
      if (dogrulaDugme.getAttribute('aria-disabled') === 'true' || secili === null) return;
      const ad = secili;
      mesaj(eylemIleti, 'doğrulanıyor…');
      void http.senaryoDogrula({ ad }).then(
        (y) => {
          if (secili !== ad) return;
          mesaj(eylemIleti, y.gecerli ? 'Doğrulama geçti (yüklenmedi).' : 'Doğrulama geçmedi; bulgular sağda.', y.gecerli ? 'iyi' : 'kotu');
          raporCiz(y.rapor, ad);
        },
        (h) => mesaj(eylemIleti, h instanceof A3Hatasi ? (ucYok(h) ? YOK_METNI : hatadanIleti(h)) : String(h), 'kotu'),
      );
    });

    yukleDugme.addEventListener('click', () => {
      if (yukleDugme.getAttribute('aria-disabled') === 'true' || secili === null) {
        if (secili !== null) mesaj(eylemIleti, dugmeler(durum, kip).yukle.neden, 'uyari');
        return;
      }
      const ad = secili;
      mesaj(eylemIleti, 'yükleniyor…');
      void komutlar.senaryoYukle({ ad }).then(({ kayit, yanit, hata }) => {
        if (kayit.asama === 'uygulandi' && yanit !== null) {
          mesaj(eylemIleti, `${ad} yüklendi.`, 'iyi');
          raporCiz(yanit.rapor, ad);
          void yukluYenile();
          void onDenetimYenile();
        } else {
          mesaj(eylemIleti, `Yüklenmedi: ${kayit.ileti}`, 'kotu');
          const rapor = hata?.ayrinti?.rapor as A5Raporu | undefined;
          if (rapor !== undefined && Array.isArray(rapor.bulgular)) raporCiz(rapor, ad);
        }
      });
    });

    // ---- Yüklü senaryo ----
    async function yukluYenile(): Promise<void> {
      if (yok.cozumlenmis) return;
      try {
        const { senaryo, ozet } = await http.cozumlenmisSenaryoOzetli();
        yukluOzet.replaceChildren();
        const t = el('table', { class: 'tablo' });
        for (const s of davranisOzeti(senaryo)) t.append(el('tr', null, el('th', null, s.etiket), el('td', null, s.deger)));
        t.append(el('tr', null, el('th', null, 'Davranış özeti'), el('td', { class: 'sabit senaryo-ozet-kod' }, ozet ?? '— (başlık yok)')));
        yukluOzet.append(t);
        yaz(yukluJson, JSON.stringify(senaryo, null, 2));
      } catch (h) {
        yukluJson.textContent = '';
        yukluOzet.replaceChildren();
        if (ucYok(h)) {
          // Yüklü senaryo yok ya da uç yok: yeniden denenmez; `senaryo_yuklendi` olayı ya da bu panelden yükleme tekrar sorar.
          yok.cozumlenmis = true;
          yukluOzet.append(el('div', { class: 'senaryo-ileti uyari' }, `Yüklü senaryo alınamadı: sunucu bu ucu sunmuyor ya da senaryo yüklü değil (404 / 405). ${'Senaryo komut satırından yüklendiyse çalıştırma denetimi yine de çalışır.'}`));
        } else yukluOzet.append(el('div', { class: 'senaryo-ileti kotu' }, h instanceof A3Hatasi ? hatadanIleti(h) : String(h)));
      }
    }

    // ---- Ön denetim ----
    async function onDenetimYenile(): Promise<void> {
      try {
        const r: OnDenetimRaporu = await http.onDenetim();
        odOzet.replaceChildren(
          el('span', { class: `cip ${r.sonuc === 'gecti' ? 'iyi' : 'kotu'}` }, r.sonuc === 'gecti' ? 'GEÇTİ' : 'KALDI'),
          el('span', { class: 'senaryo-ozet-metin' }, `kip ${r.kip}${r.olusturma_utc ? ' · ' + r.olusturma_utc : ''}`),
          el('small', { class: 'sabit' }, r.rapor_kimligi),
        );
        const t = el('table', { class: 'tablo' }, el('thead', null, el('tr', null, ...['Madde', 'Durum', 'Ölçülen', 'Sınır', 'Birim'].map((x) => el('th', null, x)))));
        const g = el('tbody');
        for (const m of r.maddeler) {
          const d = ONDENETIM_DURUM[m.durum] ?? { ad: m.durum, sinif: 'notr' };
          g.append(
            el(
              'tr',
              { title: `${m.ileti ?? ''}${m.dayanak ? ' — ' + m.dayanak : ''}` },
              el('td', { class: 'sabit' }, m.kod),
              el('td', null, el('span', { class: `cip ${d.sinif}` }, d.ad)),
              el('td', { class: 'sayi' }, sayiMetni(m.olculen)),
              el('td', { class: 'sayi' }, sayiMetni(m.sinir)),
              el('td', null, m.birim ?? ''),
            ),
          );
        }
        t.append(g);
        odTablo.replaceChildren(t);
      } catch (h) {
        odOzet.replaceChildren();
        odTablo.replaceChildren(
          el(
            'div',
            { class: `senaryo-ileti${ucYok(h) ? ' uyari' : ' kotu'}` },
            ucYok(h) ? 'Ön denetim raporu yok: çalıştırma henüz başlatılmadı ya da sunucu bu ucu sunmuyor (404). Rapor, başlat komutundan sonra gelir.' : h instanceof A3Hatasi ? hatadanIleti(h) : String(h),
          ),
        );
      }
    }

    // ---- Bağlantı ve olaylar ----
    let ilkYuklendi = false;
    b.durumDinle((bi) => {
      dugmeleriGuncelle();
      if (bi.durum === 'bagli' && !ilkYuklendi) {
        ilkYuklendi = true;
        void listele();
        void yukluYenile();
        void onDenetimYenile();
      }
    });
    b.olaylar((o) => {
      if (o.gecmisten) return;
      if (o.tur === 'senaryo_yuklendi') {
        yok.cozumlenmis = false;
        void yukluYenile();
      }
      if (o.tur === 'on_denetim_sonucu') void onDenetimYenile();
    });
    dugmeleriGuncelle();
  },
  { gecmisOlay: 0 },
);
