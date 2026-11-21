// Panel 2 — Canlı grafikler (08 §2.4): kanal seçimi, pencere uzunluğu;
// yer gerçeği, kart kestirimi ve sensör ölçümü ayrı renkte.
//
// Zaman ekseni sim zamanıdır (s). Yer gerçeği başlıktaki ölçüm zamanıyla,
// kart kestirimi yükteki `kart_t_ns` (kestirimin ait olduğu ölçüm zamanı) ile,
// sensörler başlıktaki ölçüm/gönderim zamanıyla çizilir (A3-K26).

import uPlot from 'uplot';
import type { Cerceve } from '../a3/cerceve';
import type { KanalAdi, KanalYuku } from '../a3/katalog';
import { dondur, euler, saniye, sayi } from '../ortak/bicim';
import { depoOku, depoYaz, el, kart, renk } from '../ortak/dom';
import { panelBaslat } from '../ortak/kabuk';
import { AZAMI_SANIYE, Tampon } from './grafik-tampon';

type Rol = 'gercek' | 'kestirim' | 'olcum' | `kat${number}`;

interface Kaynak<K extends KanalAdi = KanalAdi> {
  rol: Rol;
  ad: string;
  kanal: K;
  /** Değer(ler) ve zaman (s); null: bu çerçevede değer yok. */
  al: (c: Cerceve<KanalYuku[K]>) => { t: number; v: (number | null)[] } | null;
}

interface Olcu {
  id: string;
  ad: string;
  birim: string;
  grup: string;
  ters?: boolean;
  seriler: { ad: string; rol: Rol }[];
  kaynaklar: Kaynak[];
}

const RENK: Record<string, string> = {
  gercek: '--veri-gercek',
  kestirim: '--veri-kestirim',
  olcum: '--veri-olcum',
};
const KATEGORI = ['--veri-gercek', '--veri-kestirim', '--veri-olcum', '--veri-4', '--veri-5', '--veri-6', '--veri-7', '--veri-8'];

const DER = 180 / Math.PI;
// bigint ns → s: dönüşüm ölçeklemeden sonra (bicim.saniye)
const sn = saniye;

function arac(c: Cerceve<KanalYuku['fizik/durum']>) {
  return c.yuk.govdeler.find((g) => g.tur === 0) ?? null;
}

function k<K extends KanalAdi>(kanal: K, rol: Rol, ad: string, al: Kaynak<K>['al']): Kaynak {
  return { kanal, rol, ad, al } as unknown as Kaynak;
}

function tekli(id: string, ad: string, birim: string, grup: string, kaynaklar: Kaynak[], ters = false): Olcu {
  return { id, ad, birim, grup, ters, seriler: kaynaklar.map((x) => ({ ad: x.ad, rol: x.rol })), kaynaklar };
}

const GERCEK = 'yer gerçeği';
const KESTIRIM = 'kart kestirimi';

const OLCULER: Olcu[] = [
  tekli(
    'derinlik',
    'Derinlik',
    'm',
    'Konum ve yönelim',
    [
      k('fizik/durum', 'gercek', GERCEK, (c) => {
        const a = arac(c);
        return a ? { t: sn(c.baslik.t_ns), v: [a.p_n[2]!] } : null;
      }),
      k('kart/durum', 'kestirim', KESTIRIM, (c) => ({ t: sn(c.yuk.kart_t_ns), v: [c.yuk.derinlik_m] })),
      k('sensor/basinc', 'olcum', 'basınç sensörü (ρ = 1000 kg/m³ varsayımı)', (c) => ({
        t: sn(c.baslik.t_ns),
        v: [(c.yuk.basinc_pa - 101325) / (1000 * 9.80665)],
      })),
    ],
    true,
  ),
  tekli('kuzey', 'Kuzey konumu', 'm', 'Konum ve yönelim', [
    k('fizik/durum', 'gercek', GERCEK, (c) => {
      const a = arac(c);
      return a ? { t: sn(c.baslik.t_ns), v: [a.p_n[0]!] } : null;
    }),
    k('kart/durum', 'kestirim', KESTIRIM, (c) => ({ t: sn(c.yuk.kart_t_ns), v: [c.yuk.p_n[0] ?? null] })),
  ]),
  tekli('dogu', 'Doğu konumu', 'm', 'Konum ve yönelim', [
    k('fizik/durum', 'gercek', GERCEK, (c) => {
      const a = arac(c);
      return a ? { t: sn(c.baslik.t_ns), v: [a.p_n[1]!] } : null;
    }),
    k('kart/durum', 'kestirim', KESTIRIM, (c) => ({ t: sn(c.yuk.kart_t_ns), v: [c.yuk.p_n[1] ?? null] })),
  ]),
  tekli('yon', 'Yön (pruva)', '°', 'Konum ve yönelim', [
    k('fizik/durum', 'gercek', GERCEK, (c) => {
      const a = arac(c);
      const e = a ? euler(a.q_nb) : null;
      return e ? { t: sn(c.baslik.t_ns), v: [(((e.yon * DER) % 360) + 360) % 360] } : null;
    }),
    k('kart/durum', 'kestirim', KESTIRIM, (c) => {
      const e = euler(c.yuk.q_nb);
      return { t: sn(c.yuk.kart_t_ns), v: [e ? (((e.yon * DER) % 360) + 360) % 360 : null] };
    }),
  ]),
  tekli('yatis', 'Yatış', '°', 'Konum ve yönelim', [
    k('fizik/durum', 'gercek', GERCEK, (c) => {
      const a = arac(c);
      const e = a ? euler(a.q_nb) : null;
      return e ? { t: sn(c.baslik.t_ns), v: [e.yatis * DER] } : null;
    }),
    k('kart/durum', 'kestirim', KESTIRIM, (c) => {
      const e = euler(c.yuk.q_nb);
      return { t: sn(c.yuk.kart_t_ns), v: [e ? e.yatis * DER : null] };
    }),
  ]),
  tekli('yunuslama', 'Yunuslama', '°', 'Konum ve yönelim', [
    k('fizik/durum', 'gercek', GERCEK, (c) => {
      const a = arac(c);
      const e = a ? euler(a.q_nb) : null;
      return e ? { t: sn(c.baslik.t_ns), v: [e.yunuslama * DER] } : null;
    }),
    k('kart/durum', 'kestirim', KESTIRIM, (c) => {
      const e = euler(c.yuk.q_nb);
      return { t: sn(c.yuk.kart_t_ns), v: [e ? e.yunuslama * DER : null] };
    }),
  ]),
  tekli('yatay_hiz', 'Yatay hız', 'm/s', 'Hız', [
    k('fizik/durum', 'gercek', GERCEK, (c) => {
      const a = arac(c);
      if (!a) return null;
      const v = dondur(a.q_nb, [a.nu[0]!, a.nu[1]!, a.nu[2]!]);
      return { t: sn(c.baslik.t_ns), v: [Math.hypot(v[0], v[1])] };
    }),
    k('kart/durum', 'kestirim', KESTIRIM, (c) => {
      const [n, e] = c.yuk.v_n;
      return { t: sn(c.yuk.kart_t_ns), v: [n != null && e != null ? Math.hypot(n, e) : null] };
    }),
    k('sensor/dvl', 'olcum', 'DVL (dibe göre, sensör çerçevesi)', (c) => {
      const [x, y] = c.yuk.hiz_sensor_mps;
      return { t: sn(c.baslik.t_ns), v: [x != null && y != null ? Math.hypot(x, y) : null] };
    }),
  ]),
  tekli('dikey_hiz', 'Dikey hız (aşağı +)', 'm/s', 'Hız', [
    k('fizik/durum', 'gercek', GERCEK, (c) => {
      const a = arac(c);
      if (!a) return null;
      return { t: sn(c.baslik.t_ns), v: [dondur(a.q_nb, [a.nu[0]!, a.nu[1]!, a.nu[2]!])[2]] };
    }),
    k('kart/durum', 'kestirim', KESTIRIM, (c) => ({ t: sn(c.yuk.kart_t_ns), v: [c.yuk.v_n[2] ?? null] })),
  ]),
  {
    id: 'itici_u',
    ad: 'İtici isteği u',
    birim: '—',
    grup: 'İticiler ve güç',
    seriler: Array.from({ length: 8 }, (_, i) => ({ ad: `T${i + 1}`, rol: `kat${i}` as Rol })),
    kaynaklar: [k('fizik/durum', 'kat0', 'iticiler', (c) => ({ t: sn(c.baslik.t_ns), v: Array.from({ length: 8 }, (_, i) => c.yuk.iticiler[i]?.u ?? null) }))],
  },
  {
    id: 'itici_itki',
    ad: 'İtici itkisi',
    birim: 'N',
    grup: 'İticiler ve güç',
    seriler: Array.from({ length: 8 }, (_, i) => ({ ad: `T${i + 1}`, rol: `kat${i}` as Rol })),
    kaynaklar: [k('fizik/durum', 'kat0', 'iticiler', (c) => ({ t: sn(c.baslik.t_ns), v: Array.from({ length: 8 }, (_, i) => c.yuk.iticiler[i]?.itki_n ?? null) }))],
  },
  tekli('gerilim', 'Bara gerilimi', 'V', 'İticiler ve güç', [
    k('fizik/durum', 'gercek', GERCEK, (c) => ({ t: sn(c.baslik.t_ns), v: [c.yuk.bara_gerilimi_v] })),
    k('sensor/batarya', 'olcum', 'batarya ölçer', (c) => ({ t: sn(c.baslik.t_ns), v: [c.yuk.gerilim_v] })),
  ]),
  tekli('akim', 'Paket akımı', 'A', 'İticiler ve güç', [
    k('fizik/durum', 'gercek', GERCEK, (c) => ({ t: sn(c.baslik.t_ns), v: [c.yuk.paket_akimi_a] })),
    k('sensor/batarya', 'olcum', 'batarya ölçer', (c) => ({ t: sn(c.baslik.t_ns), v: [c.yuk.akim_a] })),
  ]),
  tekli('soc', 'Şarj durumu', '%', 'İticiler ve güç', [
    k('fizik/durum', 'gercek', GERCEK, (c) => ({ t: sn(c.baslik.t_ns), v: [c.yuk.soc * 100] })),
    k('kart/durum', 'kestirim', 'kartın bildirdiği', (c) => ({ t: sn(c.yuk.kart_t_ns), v: [c.yuk.batarya_soc === null ? null : c.yuk.batarya_soc * 100] })),
  ]),
  tekli('ping1d', 'Ping1D dibe mesafe', 'm', 'Sonar', [
    k('sonar/ping1d', 'olcum', 'Ping1D (satır zamanı: darbe gönderimi)', (c) => ({ t: sn(c.baslik.t_ns), v: [c.yuk.mesafe_m] })),
  ]),
  tekli('ping1d_guven', 'Ping1D güven', '0–1', 'Sonar', [
    k('sonar/ping1d', 'olcum', 'Ping1D güven', (c) => ({ t: sn(c.baslik.t_ns), v: [c.yuk.guven] })),
  ]),
];

const PENCERELER = [10, 30, 60, 120, 300];

function zamanEtiketi(s: number): string {
  const t = Math.max(0, s);
  const d = Math.floor(t / 60);
  const sn2 = t - d * 60;
  return `${d}:${sn2.toFixed(0).padStart(2, '0')}`;
}

panelBaslat('grafik', ({ baglanti: b, icerik }) => {
  const secili = new Set<string>(depoOku<string[]>('grafik.secim', ['derinlik', 'yon', 'yatay_hiz']));
  let pencere = depoOku<number>('grafik.pencere', 60);
  if (!PENCERELER.includes(pencere)) pencere = 60;

  // ---- Seçici ----
  const secici = kart('Büyüklükler');
  secici.kok.classList.add('secici');
  const gruplar = new Map<string, HTMLElement>();
  for (const o of OLCULER) {
    let g = gruplar.get(o.grup);
    if (!g) {
      g = el('div', { class: 'secici-grup' }, el('span', { class: 'etiket' }, o.grup));
      gruplar.set(o.grup, g);
      secici.govde.append(g);
    }
    const kutu = el('input', { type: 'checkbox', checked: secili.has(o.id) });
    kutu.addEventListener('change', () => {
      if (kutu.checked) secili.add(o.id);
      else secili.delete(o.id);
      depoYaz('grafik.secim', [...secili]);
      yerlestir();
    });
    g.append(el('label', { class: 'secim' }, kutu, el('span', null, o.ad), el('span', { class: 'birim' }, o.birim)));
  }

  const pencereGrup = el('div', { class: 'dugme-grubu', role: 'group', 'aria-label': 'Pencere uzunluğu' });
  const pencereDugmeleri = PENCERELER.map((p) => {
    const d = el('button', { class: 'dugme', type: 'button', 'aria-pressed': String(p === pencere) }, p < 60 ? `${p} s` : `${p / 60} dk`);
    d.addEventListener('click', () => {
      pencere = p;
      depoYaz('grafik.pencere', p);
      pencereDugmeleri.forEach((x, i) => x.setAttribute('aria-pressed', String(PENCERELER[i] === p)));
      kirli = true;
    });
    pencereGrup.append(d);
    return d;
  });
  const durdurDugme = el('button', { class: 'dugme', type: 'button', 'aria-pressed': 'false', title: 'Çizimi dondurur; veri toplanmaya devam eder' }, 'Dondur');
  let donuk = false;
  durdurDugme.addEventListener('click', () => {
    donuk = !donuk;
    durdurDugme.setAttribute('aria-pressed', String(donuk));
    kirli = true;
  });

  const grafikler = el('div', { class: 'grafikler' });
  const sag = el(
    'div',
    { style: 'display:flex;flex-direction:column;gap:12px;min-height:0;min-width:0' },
    el(
      'div',
      { style: 'display:flex;align-items:center;gap:10px;flex-wrap:wrap' },
      el('span', { class: 'etiket' }, 'Pencere'),
      pencereGrup,
      durdurDugme,
      el('span', { style: 'flex:1' }),
      el('span', { class: 'kart-not' }, el('span', { class: 'seri-imi gercek' }), 'yer gerçeği  ', el('span', { class: 'seri-imi kestirim' }), 'kart kestirimi  ', el('span', { class: 'seri-imi olcum' }), 'sensör ölçümü'),
    ),
    grafikler,
  );
  icerik.append(el('div', { class: 'grafik-duzen' }, secici.kok, sag));

  // ---- Tamponlar ve abonelikler ----
  const tamponlar = new Map<string, Tampon>(OLCULER.map((o) => [o.id, new Tampon(o.seriler.length)]));
  let sonX = 0;
  let sonRun = '';
  let kirli = true;

  b.abone('calistirma/durum', (c) => {
    if (c.yuk.run_id !== sonRun) {
      sonRun = c.yuk.run_id;
      for (const t of tamponlar.values()) t.temizle();
      sonX = 0;
      kirli = true;
    }
  });

  const kanallar = new Map<KanalAdi, { o: Olcu; kaynak: Kaynak; sutun: number }[]>();
  for (const o of OLCULER) {
    o.kaynaklar.forEach((kay, i) => {
      const liste = kanallar.get(kay.kanal) ?? [];
      liste.push({ o, kaynak: kay, sutun: o.kaynaklar.length === 1 ? 0 : i });
      kanallar.set(kay.kanal, liste);
    });
  }
  const HZ: Partial<Record<KanalAdi, 25 | 10>> = { 'fizik/durum': 25, 'kart/durum': 25 };
  for (const [kanal, liste] of kanallar) {
    b.abone(
      kanal,
      (c) => {
        for (const { o, kaynak, sutun } of liste) {
          const r = kaynak.al(c as never);
          if (r === null || !Number.isFinite(r.t)) continue;
          tamponlar.get(o.id)!.ekle(r.t, sutun, r.v);
          if (r.t > sonX) sonX = r.t;
        }
        kirli = true;
      },
      HZ[kanal] ?? 10,
    );
  }

  // ---- Grafikler ----
  interface Cizim {
    o: Olcu;
    kok: HTMLElement;
    alan: HTMLElement;
    u: uPlot;
    sira: number[];
  }
  let cizimler: Cizim[] = [];

  const eksenRengi = renk('--metin-3');
  const izgaraRengi = 'rgba(160, 180, 200, 0.07)';

  function grafikKur(o: Olcu, yukseklik: number): Cizim {
    const kk = kart(o.ad, el('span', { class: 'kart-not sabit' }, o.birim));
    kk.kok.classList.add('grafik-kart');
    const alan = el('div', { class: 'grafik-alani' });
    kk.govde.append(alan);
    const lejantYeri = el('div', { class: 'lejant-yeri' });
    kk.bas.insertBefore(lejantYeri, kk.bas.children[1] ?? null);
    grafikler.append(kk.kok);
    const genislik = Math.max(200, grafikler.clientWidth - 16);
    // Yer gerçeği başvuru çizgisidir: en üstte çizilsin diye rol serileri ters sırada verilir.
    const sira = o.seriler.some((s) => s.rol.startsWith('kat')) ? o.seriler.map((_, i) => i) : o.seriler.map((_, i) => i).reverse();
    const seriler: uPlot.Series[] = [
      { label: 'sim zamanı', value: (_u, v) => (v == null ? '—' : sayi(v, 2) + ' s') },
      ...sira.map((i) => o.seriler[i]!).map((s) => {
        const rk = s.rol.startsWith('kat') ? KATEGORI[Number(s.rol.slice(3))]! : RENK[s.rol]!;
        return {
          label: s.ad,
          stroke: renk(rk),
          width: s.rol === 'olcum' ? 1.25 : s.rol === 'gercek' ? 2 : 1.5,
          spanGaps: true,
          points: { show: false },
          value: (_u: uPlot, v: number | null) => (v == null ? '—' : sayi(v, 2)),
          ...(s.rol === 'kestirim' ? { dash: [6, 4] } : {}),
        } satisfies uPlot.Series;
      }),
    ];
    const u = new uPlot(
      {
        width: genislik,
        height: yukseklik,
        series: seriler,
        scales: { x: { time: false }, y: o.ters ? { dir: -1 } : {} },
        legend: { show: true, live: true, mount: (_u, e) => lejantYeri.append(e) },
        cursor: { drag: { x: false, y: false }, points: { size: 6 } },
        axes: [
          {
            stroke: eksenRengi,
            grid: { stroke: izgaraRengi, width: 1 },
            ticks: { stroke: izgaraRengi, width: 1, size: 4 },
            size: 28,
            values: (_u, v) => v.map((x) => zamanEtiketi(x)),
            font: '11px ui-monospace, Menlo, Consolas, monospace',
          },
          {
            stroke: eksenRengi,
            grid: { stroke: izgaraRengi, width: 1 },
            ticks: { stroke: izgaraRengi, width: 1, size: 4 },
            size: 52,
            values: (_u, v, _e, _a, artis) => {
              const b = artis >= 1 ? 0 : Math.min(4, Math.ceil(-Math.log10(artis) - 1e-9));
              return v.map((x) => sayi(x, b));
            },
            font: '11px ui-monospace, Menlo, Consolas, monospace',
          },
        ],
      },
      [[], ...o.seriler.map(() => [])] as uPlot.AlignedData,
      alan,
    );
    return { o, kok: kk.kok, alan, u, sira };
  }

  function yerlestir(): void {
    for (const c of cizimler) {
      c.u.destroy();
      c.kok.remove();
    }
    cizimler = [];
    grafikler.replaceChildren();
    const secilen = OLCULER.filter((o) => secili.has(o.id));
    if (secilen.length === 0) {
      grafikler.append(el('div', { class: 'grafik-bos' }, 'Soldan en az bir büyüklük seçin.'));
      return;
    }
    cizimler = secilen.map((o) => grafikKur(o, 100));
    boyutla();
    kirli = true;
  }

  /** Her grafiği kartının ayrılan alanına sığdırır (lejant alanın içindedir). */
  function boyutla(): void {
    for (const c of cizimler) {
      const r = c.alan.getBoundingClientRect();
      const lej = c.u.root.querySelector<HTMLElement>('.u-legend');
      const lh = lej ? lej.offsetHeight : 0;
      const g = Math.max(160, Math.floor(r.width));
      const y = Math.max(70, Math.floor(r.height - lh - 2));
      if (c.u.width !== g || c.u.height !== y) c.u.setSize({ width: g, height: y });
    }
  }

  function ciz(): void {
    const ust = sonX;
    const alt = ust - pencere;
    for (const c of cizimler) {
      const t = tamponlar.get(c.o.id)!;
      c.u.batch(() => {
        c.u.setData([t.x, ...c.sira.map((i) => t.y[i]!)] as unknown as uPlot.AlignedData, false);
        c.u.setScale('x', { min: alt, max: Math.max(ust, alt + 1) });
      });
    }
    for (const t of tamponlar.values()) t.kirp(sonX - AZAMI_SANIYE);
  }

  // Çizim ~12 kare/s ile sınırlı: düşük donanımda işlemci yükünü kısar.
  let sonCizim = 0;
  const dongu = (zaman: number): void => {
    requestAnimationFrame(dongu);
    if (!kirli || donuk || document.hidden || zaman - sonCizim < 80) return;
    sonCizim = zaman;
    kirli = false;
    ciz();
  };
  requestAnimationFrame(dongu);

  let boyutZamanlayici: ReturnType<typeof setTimeout> | null = null;
  new ResizeObserver(() => {
    if (boyutZamanlayici !== null) clearTimeout(boyutZamanlayici);
    boyutZamanlayici = setTimeout(() => {
      boyutla();
      kirli = true;
    }, 60);
  }).observe(grafikler);
  yerlestir();
});
