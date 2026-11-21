// Panel 4 — Sonar (08 §2.4): Ping360 kutupsal tarama ve Ping1D zaman–menzil
// şeridi (Canvas). Satır zamanı darbe gönderim anıdır (iskelet §3.1; A3-K26
// `gonderim`).
//
// Ping360 menzili A3 §3.4 formülüyle hesaplanır:
//   r_i = c · (2i + 1) · ornek_periyodu · 25 ns / 4,  c = ses_hizi_mps.
// Ping1D yükü (`yuk_ping1d`) profilin menzil penceresini taşımaz; şeritte
// dikey eksen profil örneği (pencere başı → sonu) olarak çizilir, dibe mesafe
// ayrı bir metre ekseninde gösterilir (rapor: Sözleşme çelişkileri).

import { BAYRAK } from '../a3/cerceve';
import { sayi, simZamani, tamsayi } from '../ortak/bicim';
import { menzilHalkalari, ping360Menzil } from './sonar-hesap';
import { el, kanvasBoyutla, kart, okuma, renk, yaz } from '../ortak/dom';
import { panelBaslat } from '../ortak/kabuk';

/** 0–255 şiddet → RGBA (koyu laciverten su yeşili ve sarıya, tek yönlü artan parlaklık). */
function sonarPaleti(): Uint32Array {
  const duraklar: [number, [number, number, number]][] = [
    [0, [5, 8, 12]],
    [0.12, [9, 28, 44]],
    [0.3, [14, 72, 96]],
    [0.5, [26, 132, 136]],
    [0.7, [120, 190, 120]],
    [0.86, [232, 214, 104]],
    [1, [255, 248, 222]],
  ];
  const lut = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    const t = i / 255;
    let j = 0;
    while (j < duraklar.length - 2 && t > duraklar[j + 1]![0]) j++;
    const [t0, c0] = duraklar[j]!;
    const [t1, c1] = duraklar[j + 1]!;
    const f = (t - t0) / (t1 - t0);
    const r = Math.round(c0[0] + (c1[0] - c0[0]) * f);
    const g = Math.round(c0[1] + (c1[1] - c0[1]) * f);
    const b = Math.round(c0[2] + (c1[2] - c0[2]) * f);
    lut[i] = (255 << 24) | (b << 16) | (g << 8) | r; // küçük uçlu RGBA
  }
  return lut;
}

const LUT = sonarPaleti();
const KUTUP_R = 400; // kutupsal görüntünün yarıçapı (piksel, iç arabellek)
const SERIT_SUTUN = 600; // 60 s, 100 ms'lik sütunlar
const SERIT_SATIR = 200;
const SUTUN_NS = 100_000_000;

function paletGradyani(): string {
  const d: string[] = [];
  for (let i = 0; i <= 8; i++) {
    const v = LUT[Math.round((i / 8) * 255)]!;
    d.push(`rgb(${v & 255},${(v >> 8) & 255},${(v >> 16) & 255}) ${(i / 8) * 100}%`);
  }
  return `linear-gradient(90deg, ${d.join(',')})`;
}

panelBaslat('sonar', ({ baglanti: b, icerik }) => {
  // ---- Yerleşim ----
  const p360Cip = el('span', { class: 'kart-not sabit' }, '—');
  const p360 = kart('Ping360 · kutupsal tarama', p360Cip);
  const p360Kanvas = el('canvas', { 'aria-label': 'Ping360 kutupsal tarama görüntüsü' });
  p360.govde.classList.add('sifir', 'sonar-tuval');
  p360.govde.append(p360Kanvas);

  const p1dCip = el('span', { class: 'kart-not sabit' }, '—');
  const p1d = kart('Ping1D · zaman–menzil şeridi', p1dCip);
  const p1dKanvas = el('canvas', { 'aria-label': 'Ping1D profil şeridi ve dibe mesafe' });
  p1d.govde.classList.add('sifir', 'sonar-tuval');
  p1d.govde.append(p1dKanvas);

  const ok = kart('Okumalar', el('span', { class: 'kart-not' }, 'şiddet ', el('span', { class: 'renk-cubugu', style: `background:${paletGradyani()}` })));
  const oAci = okuma('Ping360 açı', '°');
  const oMenzil = okuma('Menzil', 'm');
  const oHiz = okuma('Satır hızı', 'Hz');
  const oDusen = okuma('Düşen satır');
  const oMesafe = okuma('Ping1D mesafe', 'm');
  const oGuven = okuma('Güven', '%');
  const oFrekans = okuma('Frekans', 'kHz');
  const oGecersiz = okuma('Geçersiz satır');
  oGecersiz.ayarla('0', 'çizilmedi');
  ok.govde.append(el('div', { class: 'okumalar sonar-okumalar' }, oAci.kok, oMenzil.kok, oHiz.kok, oDusen.kok, oMesafe.kok, oGuven.kok, oFrekans.kok, oGecersiz.kok));
  icerik.append(el('div', { class: 'sonar-duzen' }, p360.kok, el('div', { class: 'sonar-sag' }, p1d.kok, ok.kok)));

  // ---- Ping360 arabelleği ----
  const kutup = document.createElement('canvas');
  kutup.width = kutup.height = KUTUP_R * 2;
  const kutupCtx = kutup.getContext('2d')!;
  const kutupGoruntu = kutupCtx.createImageData(KUTUP_R * 2, KUTUP_R * 2);
  const kutupPiksel = new Uint32Array(kutupGoruntu.data.buffer);
  kutupPiksel.fill(LUT[0]!);
  // pencere dışını saydam yap
  for (let y = 0; y < KUTUP_R * 2; y++)
    for (let x = 0; x < KUTUP_R * 2; x++) if ((x - KUTUP_R) ** 2 + (y - KUTUP_R) ** 2 > KUTUP_R * KUTUP_R) kutupPiksel[y * KUTUP_R * 2 + x] = 0;
  kutupCtx.putImageData(kutupGoruntu, 0, 0);

  let menzilM = 0;
  let sonAci: number | null = null;
  let kutupKirli = true;
  let eslem: Int16Array | null = null; // yarıçap pikseli -> örnek dizini
  let eslemAnahtar = '';
  let satirSayisi = 0;
  let dusen = 0;
  let gecersizSatir = 0;
  const satirZamanlari: number[] = [];

  function kutupTemizle(): void {
    for (let i = 0; i < kutupPiksel.length; i++) if (kutupPiksel[i] !== 0) kutupPiksel[i] = LUT[0]!;
    kutupCtx.putImageData(kutupGoruntu, 0, 0);
    kutupKirli = true;
  }

  b.abone('sonar/ping360', (c) => {
    const y = c.yuk;
    const n = y.ornekler.length;
    if (c.baslik.bayraklar & BAYRAK.ONCESINDE_DUSME) dusen++;
    satirSayisi++;
    satirZamanlari.push(performance.now());
    while (satirZamanlari.length > 0 && performance.now() - satirZamanlari[0]! > 2000) satirZamanlari.shift();
    // Geçersiz/sonlu olmayan girdi menzile ve çizime alınmaz; görünür sayılır (A3-K24).
    const rMax = n === y.n_ornek && !c.gecersiz.includes('aci_grad') ? ping360Menzil(y, c.gecersiz) : null;
    if (rMax === null) {
      gecersizSatir++;
      oGecersiz.ayarla(tamsayi(gecersizSatir), 'çizilmedi', 'gecersiz-deger');
      p360Cip.title = `Son geçersiz satır: ${c.gecersiz.join(', ') || 'menzil hesaplanamadı'}`;
      return;
    }
    const periyot = y.ornek_periyodu * 25e-9;
    const anahtar = `${n}:${y.ornek_periyodu}:${y.ses_hizi_mps}`;
    if (anahtar !== eslemAnahtar) {
      if (Math.abs(rMax - menzilM) > 0.01) kutupTemizle();
      eslemAnahtar = anahtar;
      menzilM = rMax;
      eslem = new Int16Array(KUTUP_R);
      for (let p = 0; p < KUTUP_R; p++) {
        const r = ((p + 0.5) / KUTUP_R) * rMax;
        const i = ((4 * r) / (y.ses_hizi_mps * periyot) - 1) / 2; // r_i formülünün tersi
        eslem[p] = Math.max(0, Math.min(n - 1, Math.round(i)));
      }
    }
    // kama genişliği: ardışık açı farkı (grad), yoksa 1
    let adim = sonAci === null ? 1 : (y.aci_grad - sonAci + 400) % 400;
    if (adim === 0 || adim > 20) adim = 1;
    sonAci = y.aci_grad;
    const t0 = ((y.aci_grad - adim / 2) * 2 * Math.PI) / 400;
    const dt = (adim * 2 * Math.PI) / 400;
    const W = KUTUP_R * 2;
    let xmin = W, xmax = 0, ymin = W, ymax = 0;
    for (let p = 0; p < KUTUP_R; p++) {
      const v = LUT[y.ornekler[eslem![p]!]!]!;
      const alt = Math.max(1, Math.ceil(p * dt * 1.5));
      for (let a = 0; a <= alt; a++) {
        const t = t0 + (dt * a) / alt;
        const x = Math.round(KUTUP_R + p * Math.sin(t));
        const yy = Math.round(KUTUP_R - p * Math.cos(t));
        if (x < 0 || yy < 0 || x >= W || yy >= W) continue;
        kutupPiksel[yy * W + x] = v;
        if (x < xmin) xmin = x;
        if (x > xmax) xmax = x;
        if (yy < ymin) ymin = yy;
        if (yy > ymax) ymax = yy;
      }
    }
    if (xmax >= xmin) kutupCtx.putImageData(kutupGoruntu, 0, 0, xmin, ymin, xmax - xmin + 1, ymax - ymin + 1);
    kutupKirli = true;
    yaz(p360Cip, `${simZamani(c.baslik.t_ns)} · ${n} örnek`);
    oAci.ayarla(sayi((y.aci_grad * 360) / 400, 1), `${y.aci_grad} grad`);
    oMenzil.ayarla(sayi(rMax, 1), `c = ${sayi(y.ses_hizi_mps, 0)} m/s`);
    oFrekans.ayarla(tamsayi(y.frekans_khz), `darbe ${tamsayi(y.darbe_suresi_us)} µs`);
    oFrekans.kok.title = `cihaz kazanç kodu ${y.kazanc}`;
    oMenzil.kok.title = `sonar kademesi: ${y.kademe === 2 ? 'tam' : y.kademe === 1 ? 'sade' : 'geçersiz'}`;
  });

  // ---- Ping1D şeridi ----
  const serit = document.createElement('canvas');
  serit.width = SERIT_SUTUN;
  serit.height = SERIT_SATIR;
  const seritCtx = serit.getContext('2d')!;
  const seritGoruntu = seritCtx.createImageData(SERIT_SUTUN, SERIT_SATIR);
  const seritPiksel = new Uint32Array(seritGoruntu.data.buffer);
  seritPiksel.fill(LUT[0]!);
  const mesafeler = new Float32Array(SERIT_SUTUN).fill(Number.NaN);
  let sonSutun = -1; // mutlak sütun numarası (t_ns / 100 ms)
  let seritKirli = true;

  function seritTemizle(): void {
    seritPiksel.fill(LUT[0]!);
    mesafeler.fill(Number.NaN);
    sonSutun = -1;
    seritKirli = true;
  }

  b.abone(
    'sonar/ping1d',
    (c) => {
      const y = c.yuk;
      const sutun = Number(c.baslik.t_ns / BigInt(SUTUN_NS)); // önce böl, sonra çevir
      if (sonSutun >= 0 && sutun < sonSutun - SERIT_SUTUN) seritTemizle();
      if (sutun > sonSutun) {
        // atlanan sütunları boşalt
        for (let s = Math.max(sonSutun + 1, sutun - SERIT_SUTUN + 1); s < sutun; s++) {
          const k = ((s % SERIT_SUTUN) + SERIT_SUTUN) % SERIT_SUTUN;
          for (let r = 0; r < SERIT_SATIR; r++) seritPiksel[r * SERIT_SUTUN + k] = LUT[0]!;
          mesafeler[k] = Number.NaN;
        }
        sonSutun = sutun;
      }
      const k = ((sutun % SERIT_SUTUN) + SERIT_SUTUN) % SERIT_SUTUN;
      const n = y.profil.length;
      for (let r = 0; r < SERIT_SATIR; r++) {
        const i = n === 0 ? -1 : Math.min(n - 1, Math.floor((r / SERIT_SATIR) * n));
        seritPiksel[r * SERIT_SUTUN + k] = i < 0 ? LUT[0]! : LUT[y.profil[i]!]!;
      }
      mesafeler[k] = y.mesafe_m ?? Number.NaN;
      seritKirli = true;
      yaz(p1dCip, `${simZamani(c.baslik.t_ns)} · ${n} örnek`);
      oMesafe.ayarla(sayi(y.mesafe_m, 2), 'dibe');
      oGuven.ayarla(sayi(y.guven * 100, 0));
    },
    25,
  );

  let sonRun = '';
  b.abone('calistirma/durum', (c) => {
    if (c.yuk.run_id !== sonRun) {
      sonRun = c.yuk.run_id;
      kutupTemizle();
      seritTemizle();
      sonAci = null;
    }
  });

  // ---- Çizim ----
  const C = { metin: renk('--metin-2'), metin3: renk('--metin-3'), vurgu: renk('--vurgu'), olcum: renk('--veri-olcum') };

  function p360Ciz(): void {
    const { g, y, oran } = kanvasBoyutla(p360Kanvas);
    const ctx = p360Kanvas.getContext('2d')!;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = '#080b0f';
    ctx.fillRect(0, 0, g, y);
    ctx.setTransform(oran, 0, 0, oran, 0, 0);
    const G = g / oran;
    const Y = y / oran;
    const R = Math.max(40, Math.min(G, Y) / 2 - 30);
    const cx = G / 2;
    const cy = Y / 2 + 6;
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(kutup, cx - R, cy - R, 2 * R, 2 * R);
    // kerteriz çizgileri ve dereceler
    ctx.strokeStyle = 'rgba(160, 190, 210, 0.14)';
    ctx.lineWidth = 1;
    ctx.font = '10.5px ui-monospace, Menlo, Consolas, monospace';
    ctx.fillStyle = C.metin3;
    for (let d = 0; d < 360; d += 30) {
      const t = (d * Math.PI) / 180;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(cx + R * Math.sin(t), cy - R * Math.cos(t));
      ctx.stroke();
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(d === 0 ? 'PRUVA' : `${d}°`, cx + (R + 16) * Math.sin(t), cy - (R + 14) * Math.cos(t));
    }
    // menzil halkaları
    if (menzilM > 0) {
      ctx.textAlign = 'left';
      ctx.textBaseline = 'bottom';
      for (const r of menzilHalkalari(menzilM)) {
        // en çok AZAMI_HALKA halka: çizim işi veriden bağımsız sınırlı
        const pr = (r / menzilM) * R;
        ctx.strokeStyle = 'rgba(160, 190, 210, 0.2)';
        ctx.beginPath();
        ctx.arc(cx, cy, pr, 0, Math.PI * 2);
        ctx.stroke();
        ctx.fillStyle = 'rgba(200, 215, 230, 0.6)';
        ctx.fillText(`${sayi(r, r < 1 ? 1 : 0)} m`, cx + pr * Math.sin(Math.PI / 4) + 3, cy - pr * Math.cos(Math.PI / 4) - 1);
      }
    }
    ctx.strokeStyle = 'rgba(160, 190, 210, 0.35)';
    ctx.beginPath();
    ctx.arc(cx, cy, R, 0, Math.PI * 2);
    ctx.stroke();
    // tarama çizgisi
    if (sonAci !== null) {
      const t = (sonAci * 2 * Math.PI) / 400;
      ctx.strokeStyle = C.vurgu;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(cx + R * Math.sin(t), cy - R * Math.cos(t));
      ctx.stroke();
    }
    // araç simgesi
    ctx.fillStyle = 'rgba(217, 225, 232, 0.9)';
    ctx.beginPath();
    ctx.moveTo(cx, cy - 8);
    ctx.lineTo(cx + 5, cy + 6);
    ctx.lineTo(cx - 5, cy + 6);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = C.metin3;
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillText('gövde çerçevesi · pruva yukarı', 10, 10);
  }

  function p1dCiz(): void {
    const { g, y, oran } = kanvasBoyutla(p1dKanvas);
    const ctx = p1dKanvas.getContext('2d')!;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = '#080b0f';
    ctx.fillRect(0, 0, g, y);
    ctx.setTransform(oran, 0, 0, oran, 0, 0);
    const G = g / oran;
    const Y = y / oran;
    const sol = 54;
    const sag = 12;
    const ust = 12;
    const genislik = Math.max(10, G - sol - sag);
    const seritY = Math.max(40, (Y - ust - 40) * 0.66);
    const mesafeUst = ust + seritY + 18;
    const mesafeY = Math.max(30, Y - mesafeUst - 22);

    seritCtx.putImageData(seritGoruntu, 0, 0);
    // halka arabellek: en yeni sütun sağda
    if (sonSutun >= 0) {
      const k = ((sonSutun % SERIT_SUTUN) + SERIT_SUTUN) % SERIT_SUTUN;
      const solParca = SERIT_SUTUN - (k + 1);
      ctx.imageSmoothingEnabled = false;
      const px = genislik / SERIT_SUTUN;
      if (solParca > 0) ctx.drawImage(serit, k + 1, 0, solParca, SERIT_SATIR, sol, ust, solParca * px, seritY);
      ctx.drawImage(serit, 0, 0, k + 1, SERIT_SATIR, sol + solParca * px, ust, (k + 1) * px, seritY);
    }
    ctx.strokeStyle = 'rgba(160,190,210,0.3)';
    ctx.lineWidth = 1;
    ctx.strokeRect(sol + 0.5, ust + 0.5, genislik, seritY);
    ctx.font = '10.5px ui-monospace, Menlo, Consolas, monospace';
    ctx.fillStyle = C.metin3;
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    ctx.fillText('başı', sol - 6, ust + 6);
    ctx.fillText('sonu', sol - 6, ust + seritY - 6);
    ctx.save();
    ctx.translate(12, ust + seritY / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textAlign = 'center';
    ctx.fillText('profil penceresi', 0, 0);
    ctx.restore();

    // mesafe şeridi (metre ekseni)
    let mn = Infinity;
    let mx = -Infinity;
    for (const m of mesafeler) {
      if (!Number.isFinite(m)) continue;
      mn = Math.min(mn, m);
      mx = Math.max(mx, m);
    }
    ctx.strokeStyle = 'rgba(160,190,210,0.3)';
    ctx.strokeRect(sol + 0.5, mesafeUst + 0.5, genislik, mesafeY);
    if (Number.isFinite(mn) && sonSutun >= 0) {
      const pay = Math.max(0.25, (mx - mn) * 0.15);
      const a = Math.floor((mn - pay) * 2) / 2;
      const bb = Math.ceil((mx + pay) * 2) / 2;
      const Yd = (m: number): number => mesafeUst + ((m - a) / (bb - a)) * mesafeY; // aşağı doğru artan
      ctx.fillStyle = C.metin3;
      ctx.textAlign = 'right';
      ctx.fillText(`${sayi(a, 1)} m`, sol - 6, mesafeUst + 6);
      ctx.fillText(`${sayi(bb, 1)} m`, sol - 6, mesafeUst + mesafeY - 6);
      ctx.strokeStyle = C.olcum;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      let basladi = false;
      const k = ((sonSutun % SERIT_SUTUN) + SERIT_SUTUN) % SERIT_SUTUN;
      for (let i = 0; i < SERIT_SUTUN; i++) {
        const idx = (k + 1 + i) % SERIT_SUTUN;
        const m = mesafeler[idx]!;
        const x = sol + ((i + 0.5) / SERIT_SUTUN) * genislik;
        if (!Number.isFinite(m)) {
          basladi = false;
          continue;
        }
        if (!basladi) ctx.moveTo(x, Yd(m));
        else ctx.lineTo(x, Yd(m));
        basladi = true;
      }
      ctx.stroke();
    }
    ctx.fillStyle = C.metin3;
    ctx.textAlign = 'left';
    ctx.textBaseline = 'bottom';
    ctx.fillText('dibe mesafe', sol + 6, mesafeUst - 3);
    // zaman ekseni
    ctx.textBaseline = 'top';
    for (let s = 0; s <= 60; s += 10) {
      const x = sol + ((60 - s) / 60) * genislik;
      ctx.textAlign = s === 0 ? 'right' : s === 60 ? 'left' : 'center';
      ctx.fillText(s === 0 ? 'şimdi' : `−${s} s`, x, mesafeUst + mesafeY + 5);
    }
  }

  let son = 0;
  const dongu = (t: number): void => {
    requestAnimationFrame(dongu);
    if (document.hidden || t - son < 40) return;
    son = t;
    if (kutupKirli) {
      kutupKirli = false;
      p360Ciz();
    }
    if (seritKirli) {
      seritKirli = false;
      p1dCiz();
    }
  };
  requestAnimationFrame(dongu);
  setInterval(() => {
    const hz = satirZamanlari.length / 2;
    oHiz.ayarla(sayi(hz, 1), `toplam ${tamsayi(satirSayisi)}`);
    oDusen.ayarla(tamsayi(dusen), 'kuyrukta düşme', dusen > 0 ? 'gecersiz-deger' : null);
  }, 500);
  new ResizeObserver(() => {
    kutupKirli = true;
    seritKirli = true;
  }).observe(icerik);
});
