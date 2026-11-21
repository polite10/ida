// Panel 1 — Araç ve sistem durumu (08 §2.4).

import type { Cerceve } from '../a3/cerceve';
import type { OnDenetimRaporu } from '../a3/iletiler';
import { YUKLER, type YukFizikDurum, type YukKartDurum } from '../a3/katalog';
import type { OlayKaydi } from '../a3/olay';
import { OTOPILOT_KIPLERI, aracNedHizi, bayt, buyuk, euler, isaretli, sayi, simZamani, sure_ns, tamsayi, yonDerece } from '../ortak/bicim';
import { el, kart, okuma, sinifSec, yaz } from '../ortak/dom';
import { olayGunlugu } from '../ortak/gunluk';
import { panelBaslat } from '../ortak/kabuk';

const SAGLIK_BITLERI: readonly { ad: string; iyiDeger: 0 | 1; aciklama: string }[] = [
  { ad: 'core_ok', iyiDeger: 1, aciklama: 'çekirdek sağlıklı' },
  { ad: 'data_ok', iyiDeger: 1, aciklama: 'girdi verisi sağlıklı' },
  { ad: 'imu_rejected', iyiDeger: 0, aciklama: 'IMU örneği reddedildi' },
  { ad: 'imu_lag', iyiDeger: 0, aciklama: 'IMU gecikmeli' },
  { ad: 'input_dropped', iyiDeger: 0, aciklama: 'girdi düştü' },
  { ad: 'narrowed', iyiDeger: 0, aciklama: 'daraltma etkin' },
  { ad: 'sample_conflict', iyiDeger: 0, aciklama: 'örnek çakışması' },
  { ad: 'bg_bad_input', iyiDeger: 0, aciklama: 'arka plan girdisi kötü' },
];

function degerler(yuk: keyof typeof YUKLER, alan: string, ust?: string): Readonly<Record<number, string>> {
  type A = { ad: string; degerler?: Record<number, string>; oge?: readonly A[] };
  const liste = YUKLER[yuk] as readonly A[];
  const a = ust !== undefined ? liste.find((x) => x.ad === ust)?.oge?.find((x) => x.ad === alan) : liste.find((x) => x.ad === alan);
  return a?.degerler ?? {};
}
const KUTLE = degerler('yuk_fizik_durum', 'kutle_durumu', 'govdeler');
const ARIZA_SINIF = degerler('yuk_ariza_etkin', 'sinif', 'arizalar');
const KUTLE_AD: Record<string, string> = { iki_torpido: 'İki torpido', bir_torpido: 'Bir torpido', bos: 'Boş', uygulanamaz: '—' };

panelBaslat(
  'durum',
  ({ baglanti: b, icerik }) => {
    icerik.classList.add('durum-izgara');

    // ---- Araç ----
    const kipCip = el('span', { class: 'cip notr' }, '—');
    const armCip = el('span', { class: 'cip notr', title: 'Otopilot arm durumu (motor silahlandırması); atış kontrol silahlandırması değildir' }, '—');
    const arac = kart('Araç', kipCip, armCip);
    const derinlik = okuma('Derinlik', 'm');
    const yon = okuma('Yön', '°');
    const hiz = okuma('Yatay hız', 'm/s');
    const dikey = okuma('Dikey hız', 'm/s');
    const yatis = okuma('Yatış / yunuslama', '°');
    const hata = okuma('Konum farkı', 'm');
    const kutle = okuma('Kütle durumu');
    const kestirim = okuma('Kestirim');
    arac.govde.append(
      el('div', { class: 'okumalar' }, derinlik.kok, yon.kok, hiz.kok, dikey.kok, yatis.kok, hata.kok, kutle.kok, kestirim.kok),
      el(
        'div',
        { class: 'aciklama-satiri' },
        el('span', { class: 'seri-imi gercek' }),
        'yer gerçeği',
        el('span', { class: 'seri-imi kestirim', style: 'margin-left:12px' }),
        'kart kestirimi (alt satır)',
      ),
    );

    // ---- Güç ve iticiler ----
    const guc = kart('Güç ve iticiler');
    const gerilim = okuma('Bara gerilimi', 'V');
    const akim = okuma('Paket akımı', 'A');
    const soc = okuma('Şarj', '%');
    const iticiKutu = el('div', { class: 'iticiler', role: 'img', 'aria-label': 'Sekiz iticinin istek ve itki değerleri' });
    const iticiler = Array.from({ length: 8 }, (_, i) => {
      const istek = el('div', { class: 'itici-istek' });
      const itki = el('div', { class: 'itici-itki' });
      const deger = el('div', { class: 'itici-deger' });
      const kok = el(
        'div',
        { class: 'itici' },
        el('div', { class: 'itici-cubuk' }, el('div', { class: 'itici-sifir' }), istek, itki),
        deger,
        el('div', { class: 'itici-ad' }, `T${i + 1}`),
      );
      iticiKutu.append(kok);
      return { kok, istek, itki, deger };
    });
    guc.govde.append(
      el('div', { class: 'okumalar' }, gerilim.kok, akim.kok, soc.kok),
      iticiKutu,
      el(
        'div',
        { class: 'aciklama-satiri' },
        el('span', { class: 'lejant-kutu istek' }),
        'istek u ∈ [−1, 1]',
        el('span', { class: 'lejant-kutu itki', style: 'margin-left:12px' }),
        'üretilen itki (azami itkiye oranla)',
      ),
    );

    // ---- Kart sağlığı ----
    const saglik = kart('Kart sağlığı');
    const ledler = SAGLIK_BITLERI.map((s) => {
      const led = el('span', { class: 'led' });
      const kok = el('div', { class: 'saglik-bit', title: s.aciklama }, led, el('span', { class: 'sabit' }, s.ad));
      return { led, kok, s };
    });
    const adim = okuma('Adım süresi', 'µs');
    const acilis = okuma('Açılış sayacı');
    const cekirdekNot = el('div', { class: 'kart-not', style: 'margin-top:10px;white-space:normal' }, 'Çekirdek sayaçları bekleniyor…');
    saglik.govde.append(el('div', { class: 'saglik-izgara' }, ...ledler.map((l) => l.kok)), el('div', { class: 'okumalar', style: 'margin-top:12px' }, adim.kok, acilis.kok), cekirdekNot);

    // ---- Gerçek zaman ve bağlantı ----
    const gz = kart('Gerçek zaman ve bağlantı');
    const gzTablo = el('table', { class: 'tablo' });
    const satirlar = new Map<string, HTMLElement>();
    const satir = (ad: string, aciklama: string): void => {
      const d = el('td', { class: 'sayi' }, '—');
      satirlar.set(ad, d);
      gzTablo.append(el('tr', { title: aciklama }, el('td', null, ad), d));
    };
    satir('Kaçırılan adım', 'gecikmesi en az bir fizik adımı olan adım (04 §2.11)');
    satir('Son tarih ihlali', 'adımın son tarihinden geç bitmesi');
    satir('Azami adım gecikmesi', 'en büyük fizik adımı gecikmesi');
    satir('Kaçırılan parti', 'A1 Tablo 3.11a');
    satir('Bariyer bekleme', 'A1 Tablo 3.11a');
    satir('A1 oturumu', 'baglanti/durum');
    satir('Emülatör kuyruğu (araç→yer)', 'bağlantı emülatörü');
    satir('Bu bağlantı: ezilen / düşen', 'A3-K40, K41: yavaş istemcide sunucunun ezdiği durum ve düşürdüğü dizi çerçeveleri');
    satir('İstemci: bozuk / geçersiz alan', 'A3-K24, K25: istemci çözücüsünün sayaçları');
    satir('Yeniden bağlanma', 'bu pencerenin yeniden bağlanma sayısı');
    gz.govde.append(gzTablo);
    gz.govde.classList.add('sifir');

    // ---- Etkin arızalar ----
    const arizaSayi = el('span', { class: 'cip notr' }, '0');
    const ariza = kart('Etkin arızalar', arizaSayi);
    const arizaGovde = el('tbody');
    ariza.govde.classList.add('sifir', 'kaydir');
    ariza.govde.append(
      el('table', { class: 'tablo' }, el('thead', null, el('tr', null, el('th', null, 'Kimlik'), el('th', null, 'Sınıf'), el('th', null, 'Tür'), el('th', null, 'Hedef'))), arizaGovde),
    );

    // ---- Ön denetim ----
    const odCip = el('span', { class: 'cip notr' }, '—');
    const od = kart('Ön denetim', odCip);
    const odListe = el('div', { class: 'od-liste' });
    od.govde.append(odListe);

    // ---- Olay günlüğü ----
    const gunluk = olayGunlugu(b, 300);
    const olay = kart('Olay günlüğü', gunluk.sayac);
    olay.govde.classList.add('sifir', 'kaydir');
    olay.govde.append(gunluk.liste);

    for (const [k, a] of [
      [arac, 'arac'],
      [guc, 'guc'],
      [saglik, 'saglik'],
      [gz, 'gz'],
      [ariza, 'ariza'],
      [od, 'od'],
      [olay, 'olay'],
    ] as const) {
      k.kok.style.gridArea = a;
      icerik.append(k.kok);
    }

    // ---- Veri ----
    let sonFizik: YukFizikDurum | null = null;
    let sonKart: Cerceve<YukKartDurum> | null = null;

    const aracGuncelle = (): void => {
      const f = sonFizik?.govdeler.find((g) => g.tur === 0) ?? null;
      const k = sonKart?.yuk ?? null;
      const eg = f ? euler(f.q_nb) : null;
      const ek = k ? euler(k.q_nb) : null;
      derinlik.ayarla(f ? sayi(f.p_n[2]!, 2) : '—', `kart ${sayi(k?.derinlik_m, 2)}`);
      yon.ayarla(eg ? yonDerece(eg.yon) : '—', `kart ${ek ? yonDerece(ek.yon) : '—'}`);
      if (f) {
        // gövde hızı → NED (tam yönelimle); kartın v_n'i zaten NED'dir
        const [vn, ve, vd] = aracNedHizi(f.q_nb, f.nu);
        hiz.ayarla(sayi(Math.hypot(vn, ve), 2), `kart ${k && k.v_n[0] != null && k.v_n[1] != null ? sayi(Math.hypot(k.v_n[0], k.v_n[1]), 2) : '—'}`);
        dikey.ayarla(isaretli(vd, 2), `kart ${isaretli(k?.v_n[2], 2)}`);
        yatis.ayarla(eg ? `${sayi((eg.yatis * 180) / Math.PI, 1)} / ${sayi((eg.yunuslama * 180) / Math.PI, 1)}` : '—', ek ? `kart ${sayi((ek.yatis * 180) / Math.PI, 1)} / ${sayi((ek.yunuslama * 180) / Math.PI, 1)}` : 'kart —');
        const ad = KUTLE[f.kutle_durumu] ?? String(f.kutle_durumu);
        kutle.ayarla(KUTLE_AD[ad] ?? ad, 'yer gerçeği');
        if (k && k.p_n[0] != null && k.p_n[1] != null && k.p_n[2] != null) {
          const d = Math.hypot(k.p_n[0] - f.p_n[0]!, k.p_n[1] - f.p_n[1]!, k.p_n[2] - f.p_n[2]!);
          hata.ayarla(sayi(d, 2), 'kestirim − gerçek, 3B', d > 1 ? 'gecersiz-deger' : null);
        } else hata.ayarla('—', 'kestirim yok');
      }
      if (k) {
        const kip = OTOPILOT_KIPLERI[k.kip ?? -1];
        yaz(kipCip, k.kip === null ? 'KİP ?' : `KİP ${kip ?? k.kip}`);
        sinifSec(kipCip, ['vurgu', 'notr'], k.kip !== null && k.kip >= 2 ? 'vurgu' : 'notr');
        yaz(armCip, k.silahli === 1 ? 'SİLAHLI' : k.silahli === 0 ? 'SİLAHSIZ' : 'ARM ?');
        sinifSec(armCip, ['uyari', 'notr'], k.silahli === 1 ? 'uyari' : 'notr');
        kestirim.ayarla(k.kestirim_gecerli === 1 ? 'Geçerli' : k.kestirim_gecerli === 0 ? 'Geçersiz' : 'Bilinmiyor', `kart t ${simZamani(k.kart_t_ns)}`, k.kestirim_gecerli === 1 ? null : 'gecersiz-deger');
      }
    };

    b.abone(
      'fizik/durum',
      (c) => {
        sonFizik = c.yuk;
        const y = c.yuk;
        gerilim.ayarla(sayi(y.bara_gerilimi_v, 2));
        akim.ayarla(sayi(y.paket_akimi_a, 1));
        soc.ayarla(sayi(y.soc * 100, 1), `kart ${sayi(sonKart?.yuk.batarya_soc != null ? sonKart.yuk.batarya_soc * 100 : null, 1)} %`);
        y.iticiler.forEach((t, i) => {
          const g = iticiler[i];
          if (!g) return;
          const u = Math.max(-1, Math.min(1, t.u));
          const oran = Math.max(-1, Math.min(1, t.itki_n / 50)); // T200 sınıfı azami itki ≈ 50 N (tahmin; A5'te)
          g.istek.style.cssText = u >= 0 ? `bottom:50%;height:${u * 50}%` : `top:50%;height:${-u * 50}%`;
          g.itki.style.cssText = oran >= 0 ? `bottom:50%;height:${oran * 50}%` : `top:50%;height:${-oran * 50}%`;
          yaz(g.deger, `${sayi(t.itki_n, 0)} N`);
          g.kok.title = `T${i + 1}: istek u = ${sayi(t.u, 2)}, itki ${sayi(t.itki_n, 1)} N, akım ${sayi(t.akim_a, 1)} A`;
          const zayif = Math.abs(u) > 0.25 && Math.abs(oran) < Math.abs(u) * 0.6;
          g.kok.classList.toggle('zayif', zayif);
        });
        aracGuncelle();
      },
      10,
    );

    b.abone(
      'kart/durum',
      (c) => {
        sonKart = c;
        const y = c.yuk;
        ledler.forEach((l, i) => {
          const bit = (y.saglik >> i) & 1;
          sinifSec(l.led, ['iyi', 'kotu', 'uyari'], bit === l.s.iyiDeger ? (l.s.iyiDeger === 1 ? 'iyi' : null) : i < 2 ? 'kotu' : 'uyari');
        });
        adim.ayarla(y.adim_suresi_us === null ? '—' : tamsayi(y.adim_suresi_us), 'tanısal');
        acilis.ayarla(y.acilis_sayaci === 0 ? '—' : tamsayi(y.acilis_sayaci));
      },
      10,
    );

    b.abone('kart/cekirdek_sayaclari', (c) => {
      const sifirOlmayan = Object.entries(c.yuk).filter(([k, v]) => k !== 'acilis_sayaci' && k !== 'saglik' && typeof v === 'number' && v > 0);
      yaz(cekirdekNot, sifirOlmayan.length === 0 ? '22 çekirdek sayacının hepsi sıfır.' : 'Sıfır olmayan: ' + sifirOlmayan.map(([k, v]) => `${k} ${v}`).join(', '));
    });

    b.abone('sayac/gercek_zaman', (c) => {
      const y = c.yuk;
      yaz(satirlar.get('Kaçırılan adım')!, tamsayi(y.kacirilan_adim));
      yaz(satirlar.get('Son tarih ihlali')!, tamsayi(y.adim_son_tarih_ihlali));
      yaz(satirlar.get('Azami adım gecikmesi')!, sure_ns(y.azami_adim_gecikmesi_ns));
      yaz(satirlar.get('Kaçırılan parti')!, tamsayi(y.parti_kacirildi));
      yaz(satirlar.get('Bariyer bekleme')!, tamsayi(y.bariyer_bekleme));
    });

    const OTURUM = degerler('yuk_baglanti_durum', 'a1_oturum');
    b.abone(
      'baglanti/durum',
      (c) => {
        const y = c.yuk;
        yaz(satirlar.get('A1 oturumu')!, y.a1_oturum === 255 ? 'uygulanamaz (SIL)' : `${buyuk(OTURUM[y.a1_oturum] ?? String(y.a1_oturum))} · oturum ${y.sess}`);
        const e = y.emulator.find((x) => x.yon === 1);
        yaz(satirlar.get('Emülatör kuyruğu (araç→yer)')!, e ? bayt(e.kuyruk_bayt) : '—');
        yaz(satirlar.get('Bu bağlantı: ezilen / düşen')!, `${tamsayi(y.bu_baglanti_ezilen_durum)} / ${tamsayi(y.bu_baglanti_dusen_dizi)}`);
      },
      2,
    );

    setInterval(() => {
      const s = b.cozucu.sayaclar;
      yaz(satirlar.get('İstemci: bozuk / geçersiz alan')!, `${tamsayi(s.istemci_bozuk_cerceve)} / ${tamsayi(s.istemci_gecersiz_alan)}`);
      yaz(satirlar.get('Yeniden bağlanma')!, tamsayi(b.bilgi.yenidenBaglanma));
    }, 1000);

    b.abone('ariza/etkin', (c) => {
      const liste = c.yuk.arizalar;
      yaz(arizaSayi, String(liste.length));
      sinifSec(arizaSayi, ['uyari', 'notr'], liste.length > 0 ? 'uyari' : 'notr');
      arizaGovde.replaceChildren(
        ...(liste.length === 0
          ? [el('tr', null, el('td', { colspan: '4', class: 'yok' }, 'Etkin arıza yok'))]
          : liste.map((f) =>
              el(
                'tr',
                {
                  title: `kaynak ${f.kaynak === 0 ? 'senaryo' : 'elle'} · başlangıç adımı ${f.baslangic_adimi}${f.bitis_adimi === null ? ' · süresiz' : ` · bitiş adımı ${f.bitis_adimi}`}`,
                },
                el('td', { class: 'sabit' }, f.fault_id),
                el('td', null, ARIZA_SINIF[f.sinif] ?? String(f.sinif)),
                el('td', { class: 'sabit' }, f.tur),
                el('td', { class: 'sabit' }, f.hedef),
              ),
            )),
      );
    });

    // Ön denetim raporu: ilk açılışta ve her `on_denetim_sonucu` olayında HTTP'den alınır (A3-K62).
    const onDenetimYukle = async (): Promise<void> => {
      try {
        const r: OnDenetimRaporu = await b.http.onDenetim();
        yaz(odCip, r.sonuc === 'gecti' ? 'GEÇTİ' : 'KALDI');
        sinifSec(odCip, ['iyi', 'kotu', 'notr'], r.sonuc === 'gecti' ? 'iyi' : 'kotu');
        const renk: Record<string, string> = { gecti: 'iyi', kaldi: 'kotu', uyari: 'uyari', atlandi: 'notr' };
        const ad: Record<string, string> = { gecti: 'geçti', kaldi: 'kaldı', uyari: 'uyarı', atlandi: 'atlandı' };
        odListe.replaceChildren(
          ...r.maddeler.map((m) =>
            el(
              'div',
              { class: 'od-madde', title: `${m.ileti ?? ''}${m.dayanak ? ' — ' + m.dayanak : ''}` },
              el('span', { class: `led ${renk[m.durum] === 'notr' ? '' : renk[m.durum] ?? ''}` }),
              el('span', { class: 'sabit' }, m.kod),
              el('span', { class: 'od-durum' }, ad[m.durum] ?? m.durum),
            ),
          ),
        );
        od.bas.title = `Rapor ${r.rapor_kimligi} · ${r.kip}`;
      } catch {
        yaz(odCip, 'YOK');
        sinifSec(odCip, ['iyi', 'kotu', 'notr'], 'notr');
      }
    };
    b.durumDinle((bi) => {
      if (bi.durum === 'bagli') void onDenetimYukle();
    });
    b.olaylar((o: OlayKaydi) => {
      if (o.tur === 'on_denetim_sonucu' && !o.gecmisten) void onDenetimYukle();
    });
  },
  { gecmisOlay: 200 },
);
