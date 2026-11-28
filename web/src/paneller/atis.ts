// Panel 5 — Atış kontrol ve emniyet durumu (08 §2.4). YALNIZ GÖSTERİM.
//
// Bu panelde atış tetikleyen, emniyet girdisini değiştiren ya da herhangi bir
// komut gönderen denetim YOKTUR (W3-web dilim 1; A3-K99). Gösterilen:
// kartın bildirdiği emniyet zinciri durumu ve engel maskesi (A4 §3.16 adlarıyla),
// sanal emniyet girdilerinin yetkili durumu ile kartın geri okuması yan yana
// (fark işaretli; A3-K97), tüp durumu ve emniyet olayları.

import { YUKLER } from '../a3/katalog';
import { simZamani, sure_ns, tamsayi } from '../ortak/bicim';
import { el, kart, sinifSec, yaz } from '../ortak/dom';
import { girdiGosterimi, tupGosterimi } from '../ortak/emniyet';
import { olayGunlugu } from '../ortak/gunluk';
import { panelBaslat } from '../ortak/kabuk';

/** A4 §3.16 `fc.state` (IDA_FCS_*). */
const ZINCIR: readonly { kod: number; ad: string; aciklama: string; ayri?: boolean }[] = [
  { kod: 0, ad: 'SAFE', aciklama: 'emniyetli; açık izin yok' },
  { kod: 1, ad: 'ARMED', aciklama: 'atış kontrol silahlı; açık izin yok' },
  { kod: 2, ad: 'PERMIT_OPEN', aciklama: 'en az bir tüpte izin açık' },
  { kod: 3, ad: 'FIRE_PENDING', aciklama: 'sonucu beklenen istek var' },
  { kod: 4, ad: 'TEST', aciklama: 'sınama kipi', ayri: true },
  { kod: 5, ad: 'FAULT_LATCHED', aciklama: 'mandallı arıza', ayri: true },
];

/** A4 §3.16 `fc.inhibit_mask` (IDA_FCI_*), bit 0–17; 18–31 ayrılmış. */
const ENGELLER: readonly { ad: string; aciklama: string }[] = [
  { ad: 'NOT_ARMED', aciklama: 'araç silahsız' },
  { ad: 'MODE', aciklama: 'kip izin kümesinde değil' },
  { ad: 'EST', aciklama: 'kestirim düzeyi OK değil' },
  { ad: 'FAILSAFE', aciklama: 'arıza eylemi HOLD ya da üstü' },
  { ad: 'ESTOP', aciklama: 'etkin acil durdurma basılı' },
  { ad: 'PLUG_T0', aciklama: 'tüp f1 fişi çıkık' },
  { ad: 'PLUG_T1', aciklama: 'tüp f2 fişi çıkık' },
  { ad: 'PIN_T0', aciklama: 'tüp f1 pimi takılı' },
  { ad: 'PIN_T1', aciklama: 'tüp f2 pimi takılı' },
  { ad: 'EMPTY_T0', aciklama: 'tüp f1 boş' },
  { ad: 'EMPTY_T1', aciklama: 'tüp f2 boş' },
  { ad: 'EVENTS_OFF', aciklama: 'olay kimliği kullanılamaz' },
  { ad: 'IMU_LAG', aciklama: 'IMU gecikmeli' },
  { ad: 'IMU_STALE', aciklama: 'IMU bayat' },
  { ad: 'SESSION', aciklama: 'A1 oturumu yok' },
  { ad: 'RESULT_PENDING', aciklama: 'sonucu beklenen olay var' },
  { ad: 'FAULT_LATCHED', aciklama: 'mandallı arıza' },
  { ad: 'DET_STALE', aciklama: 'kamera algılama akışı bayat' },
];

type A = { ad: string; degerler?: Record<number, string>; oge?: readonly A[] };
const TUPLER = (YUKLER.yuk_atis_durum as readonly A[]).find((a) => a.ad === 'tupler')!;
const OLAY_SONUCU = TUPLER.oge!.find((a) => a.ad === 'son_olay_sonucu')!.degerler!;
const KAYNAK = (YUKLER.yuk_emniyet_girdiler as readonly A[]).find((a) => a.ad === 'son_degisim_kaynagi')!.degerler!;

const EMNIYET_TURLERI = new Set(['emniyet_girdisi_degisti', 'atis_olayi_sonucu', 'gevsetme_reddedildi', 'emniyet_rev_doydu']);

panelBaslat(
  'atis',
  ({ baglanti: b, icerik }) => {
    icerik.classList.add('atis-izgara');

    const notGosterim = el('span', { class: 'cip notr', title: 'Bu panelde atış tetikleyen ya da emniyet girdisi değiştiren denetim yoktur; acil durdur düğmesi ortak şeritte, girdi isteği Girdiler panelindedir' }, 'Yalnız gösterim');
    const revEl = el('span', { class: 'rev', title: 'Emniyet revizyonu (iskelet §3.2; A3-K117)' }, 'rev —');

    // ---- 1. Zincir ----
    const zincirCip = el('span', { class: 'cip notr' }, '—');
    const bozukCip = el('span', { class: 'cip kotu', hidden: true, title: 'A3-K25: istemci çözücüsünün attığı bozuk çerçeveler' }, '');
    const zincir = kart('Emniyet zinciri · kartın bildirdiği durum', bozukCip, zincirCip, revEl, notGosterim);
    const zincirKutu = el('div', { class: 'zincir' });
    const adimlar = ZINCIR.map((z, i) => {
      const d = el('div', { class: `zincir-adim${z.ayri ? ' ayri' : ''}` }, el('div', { class: 'ad' }, z.ad), el('div', { class: 'aciklama' }, z.aciklama));
      if (z.ayri && !ZINCIR[i - 1]?.ayri) zincirKutu.append(el('div', { class: 'zincir-ayrac' }));
      zincirKutu.append(d);
      return d;
    });
    const zincirAlt = el('div', { class: 'aciklama-satiri' }, '—');
    zincir.govde.append(zincirKutu, zincirAlt);

    // ---- 2. Sanal emniyet girdileri ----
    const farkCip = el('span', { class: 'cip notr' }, '—');
    const girdi = kart('Sanal emniyet girdileri', farkCip);
    const govde = el('tbody');
    const SATIRLAR = [
      { ad: 'Acil durdurma', bit: 0 },
      { ad: 'Silahlandırma fişi · f1', bit: 1 },
      { ad: 'Silahlandırma fişi · f2', bit: 2 },
      { ad: 'Emniyet pimi · f1', bit: 3 },
      { ad: 'Emniyet pimi · f2', bit: 4 },
    ].map((s) => {
      const yetkili = el('td', { class: 'durum-hucre' }, '—');
      const kartD = el('td', { class: 'durum-hucre' }, '—');
      const fark = el('td', null, el('span', { class: 'esit' }, '—'));
      const tr = el('tr', null, el('td', null, s.ad), yetkili, kartD, fark);
      govde.append(tr);
      return { ...s, tr, yetkili, kartD, fark };
    });
    const girdiAlt = el('div', { class: 'aciklama-satiri' }, '—');
    girdi.govde.append(
      el(
        'table',
        { class: 'tablo girdi-tablo' },
        el('thead', null, el('tr', null, el('th', null, 'Girdi'), el('th', null, 'Yetkili · sim'), el('th', null, 'Kart geri okuması'), el('th', null, 'Fark'))),
        govde,
      ),
      girdiAlt,
    );

    // ---- 3. Engel maskesi ----
    const engelCip = el('span', { class: 'cip notr' }, '—');
    const engel = kart('Engel maskesi · A4 §3.16', engelCip);
    const anun = el('div', { class: 'anunsiyator', role: 'list' });
    const lambalar = ENGELLER.map((e, i) => {
      const d = el(
        'div',
        { class: 'anun', role: 'listitem', title: `bit ${i} ${e.ad}: ${e.aciklama}` },
        el('span', { class: 'ad' }, e.ad),
        el('span', { class: 'bit' }, String(i)),
        el('span', { class: 'gizli-metin' }, e.aciklama),
      );
      anun.append(d);
      return d;
    });
    engel.govde.append(anun);

    // ---- 4. Tüpler ----
    const tup = kart('Fırlatıcı tüpleri');
    const tupKutu = el('div', { class: 'tupler' });
    const tupler = [0, 1].map((i) => {
      const dolu = el('span', { class: 'cip notr' }, '—');
      const izin = el('span', null, '—');
      const kalan = el('span', null, '—');
      const sonuc = el('span', null, '—');
      const kok = el(
        'div',
        { class: 'tup' },
        el('div', { class: 'tup-bas' }, el('span', { class: 'tup-ad' }, i === 0 ? 'f1' : 'f2'), dolu),
        el('div', { class: 'satir' }, el('span', null, 'Süreli izin'), izin),
        el('div', { class: 'satir' }, el('span', null, 'Kalan süre'), kalan),
        el('div', { class: 'satir' }, el('span', null, 'Son tek seferlik olay'), sonuc),
      );
      tupKutu.append(kok);
      return { dolu, izin, kalan, sonuc };
    });
    tup.bas.title = "Doluluk ve süreli izin sim'in yetkili durumudur (atis/durum kanalı).";
    tup.bas.append(el('span', { class: 'kart-not' }, 'sim yetkili durumu'));
    tup.govde.append(tupKutu);

    // ---- 5. Emniyet olayları ----
    const gunluk = olayGunlugu(b, 200, { suzgec: (o) => o.onem === 'emniyet' || EMNIYET_TURLERI.has(o.tur) || o.tur === 'ariza_degisti' });
    const olay = kart('Emniyet olayları', gunluk.sayac);
    olay.govde.classList.add('sifir', 'kaydir');
    olay.govde.append(gunluk.liste);

    for (const [k, a] of [
      [zincir, 'zincir'],
      [girdi, 'girdi'],
      [engel, 'engel'],
      [tup, 'tup'],
      [olay, 'olay'],
    ] as const) {
      k.kok.style.gridArea = a;
      icerik.append(k.kok);
    }

    // ---- Veri ----
    b.abone('atis/durum', (c) => {
      const y = c.yuk;
      const etkin = ZINCIR.find((z) => z.kod === y.fc_durumu);
      adimlar.forEach((d, i) => {
        const on = ZINCIR[i]!.kod === y.fc_durumu && !c.gecersiz.includes('fc_durumu');
        d.classList.toggle('etkin', on);
        d.classList.toggle('kotu', on && ZINCIR[i]!.kod === 5);
      });
      const fcGecersiz = y.fc_durumu !== null && (etkin === undefined || c.gecersiz.includes('fc_durumu'));
      yaz(zincirCip, fcGecersiz ? `GEÇERSİZ (${y.fc_durumu})` : etkin ? etkin.ad : 'BİLİNMİYOR');
      sinifSec(zincirCip, ['iyi', 'uyari', 'kotu', 'notr', 'vurgu'], fcGecersiz ? 'kotu' : etkin === undefined ? 'notr' : y.fc_durumu === 0 ? 'iyi' : y.fc_durumu === 5 ? 'kotu' : 'uyari');
      yaz(zincirAlt, `kartın son olay kimliği: ${y.kart_son_olay_kimligi === null ? 'yok' : tamsayi(y.kart_son_olay_kimligi)} · alım zamanı ${simZamani(c.baslik.t_ns)}`);

      const m = y.engel_maskesi;
      let n = 0;
      lambalar.forEach((l, i) => {
        const kurulu = m !== null && ((m >>> i) & 1) === 1;
        if (kurulu) n++;
        l.classList.toggle('kurulu', kurulu);
      });
      yaz(engelCip, m === null ? 'BİLİNMİYOR' : `${n} engel`);
      sinifSec(engelCip, ['uyari', 'notr'], m !== null && n > 0 ? 'uyari' : 'notr');

      const tupListesi = Array.isArray(y.tupler) ? y.tupler : [];
      tupler.forEach((g, i) => {
        const t = tupListesi.find((x) => x.tup === i);
        if (t === undefined) {
          // tüp öğesi yok: önceki gösterim kalmasın
          yaz(g.dolu, 'YOK');
          sinifSec(g.dolu, ['vurgu', 'notr', 'kotu'], 'notr');
          for (const e of [g.izin, g.kalan, g.sonuc]) yaz(e, '—');
          return;
        }
        const dizin = tupListesi.indexOf(t);
        const tg = tupGosterimi(t, (alan) => c.gecersiz.includes(`tupler[${dizin}].${alan}`));
        yaz(g.dolu, tg.dolu.metin);
        sinifSec(g.dolu, ['vurgu', 'notr', 'kotu'], tg.dolu.cip);
        yaz(g.izin, tg.izin);
        yaz(g.kalan, t.izin_kalan_ns === null ? '—' : sure_ns(t.izin_kalan_ns));
        const sonuc = OLAY_SONUCU[t.son_olay_sonucu];
        yaz(g.sonuc, sonuc === undefined || c.gecersiz.includes(`tupler[${dizin}].son_olay_sonucu`) ? 'geçersiz' : sonuc.replaceAll('_', ' '));
      });
    });

    const SINIFLAR = ['kisitlayan', 'gevsek', 'yok', 'gecersiz', 'bilinmiyor'];
    const FARK_METNI: Record<string, string> = { esit: 'eşit', bilinmiyor: 'bilinmiyor', gecersiz: 'geçersiz' };
    b.abone('emniyet/girdiler', (c) => {
      const y = c.yuk;
      const g = girdiGosterimi(y, c.gecersiz);
      g.satirlar.forEach((gs, i) => {
        const s = SATIRLAR[i]!;
        yaz(s.yetkili, gs.yetkili.metin);
        yaz(s.kartD, gs.kart.metin);
        sinifSec(s.yetkili, SINIFLAR, gs.yetkili.sinif);
        sinifSec(s.kartD, SINIFLAR, gs.kart.sinif);
        s.tr.classList.toggle('fark', gs.fark === 'fark' || gs.fark === 'gecersiz');
        s.fark.replaceChildren(
          gs.fark === 'fark'
            ? el('span', { class: 'fark-cip', title: 'Yetkili durum ile kartın bildirdiği etkin değer farklı (A3-K97); iletim gecikmesi süresince olağan olarak kurulabilir' }, 'FARK')
            : gs.fark === 'gecersiz'
              ? el('span', { class: 'fark-cip', title: 'Değer listesi dışı ya da geçersiz alan (A3-K24)' }, 'GEÇERSİZ')
              : el('span', { class: gs.fark === 'esit' ? 'esit' : 'esit bilinmiyor' }, FARK_METNI[gs.fark] ?? gs.fark),
        );
      });
      yaz(farkCip, g.ozet.metin);
      sinifSec(farkCip, ['iyi', 'kotu', 'notr', 'uyari'], g.ozet.sinif);
      yaz(revEl, `rev ${y.emniyet_rev === null ? '—' : tamsayi(y.emniyet_rev)}${y.emniyet_rev_doydu === 1 ? ' · DOYDU' : ''}`);
      yaz(
        girdiAlt,
        `son değişim: ${y.son_degisim_adimi === null ? 'yok' : `adım ${tamsayi(y.son_degisim_adimi)}`} · kaynak ${KAYNAK[y.son_degisim_kaynagi] ?? 'geçersiz'} · yeşil = kısıtlayan yön, sarı = gevşek yön`,
      );
    });

    // A3-K25: bozuk sayılıp atılan çerçeve varsa göster (atılan emniyet çerçevesi ekrandaki son değeri tazelemez).
    let sonBozuk = 0;
    setInterval(() => {
      const n = b.cozucu.sayaclar.istemci_bozuk_cerceve;
      if (n === sonBozuk) return;
      sonBozuk = n;
      bozukCip.hidden = false;
      yaz(bozukCip, `${n} bozuk çerçeve atıldı`);
    }, 500);
  },
  { gecmisOlay: 300 },
);
