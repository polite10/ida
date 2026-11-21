// Panel 3 — Harita (08 §2.4): NED yerel düzlemde ızgara (Canvas 2B), yer
// gerçeği izi, kart kestirimi izi, yol noktaları, hedef, ölçek ve kuzey;
// yakınlaştırma ve kaydırma. Coğrafi altlık (MapLibre) sonraki dilimdir.
//
// Yol noktaları ve hedefler yüklü çözümlenmiş senaryodan (`GET
// /senaryo/cozumlenmis`, A3-K80; A5 `gorev.ogeler`, `hedefler`) okunur;
// hedeflerin anlık konumu `fizik/durum` gövdelerinden (tür = hedef) gelir.

import { aracNedHizi, euler, sayi, simZamani, yonDerece } from '../ortak/bicim';
import { el, kanvasBoyutla, kart, okuma, renk, yaz } from '../ortak/dom';
import { panelBaslat } from '../ortak/kabuk';

interface YolNoktasi {
  n: number;
  e: number;
  d: number;
  tur: string;
}

interface Hedef {
  ad: string;
  n: number;
  e: number;
  d: number;
}

/** Büyüyen nokta dizisi (n, e çiftleri). */
class Iz {
  veri = new Float64Array(4096);
  boy = 0;
  ekle(n: number, e: number, enAz: number): boolean {
    if (this.boy >= 2) {
      const dn = n - this.veri[this.boy - 2]!;
      const de = e - this.veri[this.boy - 1]!;
      if (dn * dn + de * de < enAz * enAz) return false;
    }
    if (this.boy + 2 > this.veri.length) {
      if (this.veri.length >= 400_000) {
        // ~200 000 nokta üstünde eski yarıyı at (bellek sınırı)
        this.veri.copyWithin(0, this.veri.length / 2);
        this.boy -= this.veri.length / 2;
      } else {
        const v = new Float64Array(this.veri.length * 2);
        v.set(this.veri);
        this.veri = v;
      }
    }
    this.veri[this.boy++] = n;
    this.veri[this.boy++] = e;
    return true;
  }
  temizle(): void {
    this.boy = 0;
  }
  uzunluk(): number {
    let s = 0;
    for (let i = 2; i < this.boy; i += 2) s += Math.hypot(this.veri[i]! - this.veri[i - 2]!, this.veri[i + 1]! - this.veri[i - 1]!);
    return s;
  }
}

function guzelAdim(hamMetre: number): number {
  const us = Math.pow(10, Math.floor(Math.log10(hamMetre)));
  const k = hamMetre / us;
  return (k < 1.5 ? 1 : k < 3.5 ? 2 : k < 7.5 ? 5 : 10) * us;
}

function lejantSvg(tur: 'gercek' | 'kestirim' | 'yol' | 'hedef'): SVGSVGElement {
  const ns = 'http://www.w3.org/2000/svg';
  const s = document.createElementNS(ns, 'svg');
  s.setAttribute('width', '22');
  s.setAttribute('height', '10');
  s.setAttribute('viewBox', '0 0 22 10');
  const p = document.createElementNS(ns, 'path');
  if (tur === 'hedef') {
    p.setAttribute('d', 'M11 1 L15 5 L11 9 L7 5 Z');
    p.setAttribute('fill', 'none');
    p.setAttribute('stroke', renk('--veri-hedef'));
    p.setAttribute('stroke-width', '1.6');
  } else {
    p.setAttribute('d', 'M1 5 H21');
    p.setAttribute('stroke', renk(tur === 'gercek' ? '--veri-gercek' : tur === 'kestirim' ? '--veri-kestirim' : '--veri-yol'));
    p.setAttribute('stroke-width', tur === 'yol' ? '1.2' : '2');
    if (tur !== 'gercek') p.setAttribute('stroke-dasharray', tur === 'yol' ? '2 3' : '5 3');
  }
  s.append(p);
  return s;
}

panelBaslat(
  'harita',
  ({ baglanti: b, icerik }) => {
    const kanvas = el('canvas', { class: 'harita-kanvas', 'aria-label': 'NED yerel düzlem haritası' });
    const kok = el('div', { class: 'harita-kok' }, kanvas);
    icerik.append(kok);

    // ---- Denetimler ----
    const arti = el('button', { class: 'dugme', type: 'button', title: 'Yakınlaştır' }, '+');
    const eksi = el('button', { class: 'dugme', type: 'button', title: 'Uzaklaştır' }, '−');
    const sigdir = el('button', { class: 'dugme', type: 'button', title: 'Senaryoyu ve izi sığdır' }, 'Sığdır');
    const takip = el('button', { class: 'dugme', type: 'button', 'aria-pressed': 'false', title: 'Aracı ortada tut' }, 'Aracı izle');
    const temizle = el('button', { class: 'dugme', type: 'button', title: 'İzleri temizle (yalnız bu pencerede)' }, 'İzleri sil');
    kok.append(
      el(
        'div',
        { class: 'harita-ust' },
        el('div', { class: 'dugme-grubu yuzen' }, arti, eksi),
        el('div', { class: 'yuzen', style: 'display:flex' }, sigdir),
        el('div', { class: 'yuzen', style: 'display:flex' }, takip),
        el('div', { class: 'yuzen', style: 'display:flex' }, temizle),
      ),
    );

    const bilgi = kart('Araç');
    bilgi.kok.classList.add('harita-bilgi', 'yuzen');
    const oKuzey = okuma('Kuzey', 'm');
    const oDogu = okuma('Doğu', 'm');
    const oDerinlik = okuma('Derinlik', 'm');
    const oYon = okuma('Yön', '°');
    const oHiz = okuma('Hız', 'm/s');
    const oFark = okuma('Kestirim farkı', 'm');
    const izNot = el('div', { class: 'kart-not', style: 'margin-top:10px' }, '—');
    bilgi.govde.append(el('div', { class: 'okumalar' }, oKuzey.kok, oDogu.kok, oDerinlik.kok, oYon.kok, oHiz.kok, oFark.kok), izNot);
    kok.append(bilgi.kok);

    const lejant = el(
      'div',
      { class: 'harita-lejant yuzen' },
      el('div', { class: 'lejant-oge' }, lejantSvg('gercek'), 'yer gerçeği izi'),
      el('div', { class: 'lejant-oge' }, lejantSvg('kestirim'), 'kart kestirimi izi'),
      el('div', { class: 'lejant-oge' }, lejantSvg('yol'), 'görev yol noktaları'),
      el('div', { class: 'lejant-oge' }, lejantSvg('hedef'), 'hedef (senaryo)'),
    );
    const imlec = el('div', { class: 'harita-imlec yuzen sabit' }, 'K —  D —');
    kok.append(lejant, imlec);

    // ---- Durum ----
    const gercekIz = new Iz();
    const kestirimIz = new Iz();
    let yolNoktalari: YolNoktasi[] = [];
    let hedefler: Hedef[] = [];
    const canliHedef = new Map<number, { n: number; e: number; d: number }>();
    let arac: { n: number; e: number; d: number; yon: number; hiz: number } | null = null;
    let kestirim: { n: number; e: number; yon: number | null } | null = null;
    let sonRun = '';
    let simT = 0n;

    const gorunum = { n: 7, e: 7, olcek: 14 };
    let takipEt = false;
    let ilkSigdirma = true;
    let kirli = true;

    const C = {
      gercek: renk('--veri-gercek'),
      kestirim: renk('--veri-kestirim'),
      hedef: renk('--veri-hedef'),
      yol: renk('--veri-yol'),
      metin: renk('--metin-2'),
      metin3: renk('--metin-3'),
      vurgu: renk('--vurgu'),
    };

    async function senaryoYukle(): Promise<void> {
      try {
        const s = await b.http.cozumlenmisSenaryo();
        const gorev = s['gorev'] as { ogeler?: Record<string, unknown>[] } | undefined;
        yolNoktalari = (gorev?.ogeler ?? [])
          .filter((o) => o['frame'] === 'LOCAL' && typeof o['north_m'] === 'number' && typeof o['east_m'] === 'number')
          .map((o) => ({ n: o['north_m'] as number, e: o['east_m'] as number, d: Number(o['depth'] ?? 0), tur: String(o['item_type'] ?? '') }));
        const h = (s['hedefler'] ?? {}) as Record<string, { konum?: number[] }>;
        hedefler = Object.entries(h)
          .filter(([, v]) => Array.isArray(v.konum) && v.konum.length === 3)
          .map(([ad, v]) => ({ ad, n: v.konum![0]!, e: v.konum![1]!, d: v.konum![2]! }));
        if (ilkSigdirma) sigdirAyarla();
        kirli = true;
      } catch {
        /* senaryo yoksa yol noktası çizilmez */
      }
    }

    b.durumDinle((bi) => {
      if (bi.durum === 'bagli') void senaryoYukle();
    });

    b.abone('calistirma/durum', (c) => {
      simT = c.baslik.t_ns;
      if (c.yuk.run_id !== sonRun) {
        sonRun = c.yuk.run_id;
        gercekIz.temizle();
        kestirimIz.temizle();
        kirli = true;
        if (sonRun !== '') void senaryoYukle();
      }
    });

    b.abone(
      'fizik/durum',
      (c) => {
        for (const g of c.yuk.govdeler) {
          if (g.tur === 0) {
            const eu = euler(g.q_nb);
            const yon = eu?.yon ?? 0;
            const [vn, ve] = aracNedHizi(g.q_nb, g.nu); // gövde hızı → NED, tam yönelimle
            arac = { n: g.p_n[0]!, e: g.p_n[1]!, d: g.p_n[2]!, yon, hiz: Math.hypot(vn, ve) };
            gercekIz.ekle(arac.n, arac.e, 0.05);
          } else if (g.tur === 2) canliHedef.set(g.govde_id, { n: g.p_n[0]!, e: g.p_n[1]!, d: g.p_n[2]! });
        }
        kirli = true;
      },
      25,
    );

    b.abone(
      'kart/durum',
      (c) => {
        const [n, e] = c.yuk.p_n;
        if (n == null || e == null) {
          kestirim = null;
          return;
        }
        const eu = euler(c.yuk.q_nb);
        kestirim = { n, e, yon: eu?.yon ?? null };
        kestirimIz.ekle(n, e, 0.05);
        kirli = true;
      },
      25,
    );

    // ---- Görünüm ----
    function sigdirAyarla(): void {
      const noktalar: [number, number][] = [];
      for (const y of yolNoktalari) noktalar.push([y.n, y.e]);
      for (const h of hedefler) noktalar.push([h.n, h.e]);
      if (arac) noktalar.push([arac.n, arac.e]);
      for (let i = 0; i < gercekIz.boy; i += 40) noktalar.push([gercekIz.veri[i]!, gercekIz.veri[i + 1]!]);
      if (noktalar.length === 0) return;
      let nMin = Infinity, nMax = -Infinity, eMin = Infinity, eMax = -Infinity;
      for (const [n, e] of noktalar) {
        nMin = Math.min(nMin, n);
        nMax = Math.max(nMax, n);
        eMin = Math.min(eMin, e);
        eMax = Math.max(eMax, e);
      }
      const r = kanvas.getBoundingClientRect();
      const kenar = 70;
      const g = Math.max(120, r.width - 2 * kenar - 270);
      const y = Math.max(120, r.height - 2 * kenar);
      gorunum.olcek = Math.max(0.5, Math.min(200, Math.min(g / Math.max(4, eMax - eMin), y / Math.max(4, nMax - nMin))));
      gorunum.n = (nMin + nMax) / 2;
      // bilgi kartına yer bırakmak için merkezi biraz sağa kaydır
      gorunum.e = (eMin + eMax) / 2 + 135 / gorunum.olcek;
      ilkSigdirma = false;
      kirli = true;
    }

    function yakinlas(carpan: number, px?: number, py?: number): void {
      const r = kanvas.getBoundingClientRect();
      const x = px ?? r.width / 2;
      const y = py ?? r.height / 2;
      const eOnce = gorunum.e + (x - r.width / 2) / gorunum.olcek;
      const nOnce = gorunum.n - (y - r.height / 2) / gorunum.olcek;
      gorunum.olcek = Math.max(0.2, Math.min(400, gorunum.olcek * carpan));
      gorunum.e = eOnce - (x - r.width / 2) / gorunum.olcek;
      gorunum.n = nOnce + (y - r.height / 2) / gorunum.olcek;
      kirli = true;
    }

    arti.addEventListener('click', () => yakinlas(1.4));
    eksi.addEventListener('click', () => yakinlas(1 / 1.4));
    sigdir.addEventListener('click', () => {
      takipEt = false;
      takip.setAttribute('aria-pressed', 'false');
      sigdirAyarla();
    });
    takip.addEventListener('click', () => {
      takipEt = !takipEt;
      takip.setAttribute('aria-pressed', String(takipEt));
      kirli = true;
    });
    temizle.addEventListener('click', () => {
      gercekIz.temizle();
      kestirimIz.temizle();
      kirli = true;
    });

    kanvas.addEventListener(
      'wheel',
      (e) => {
        e.preventDefault();
        const r = kanvas.getBoundingClientRect();
        yakinlas(Math.exp(-e.deltaY * 0.0015), e.clientX - r.left, e.clientY - r.top);
      },
      { passive: false },
    );
    kanvas.addEventListener('dblclick', (e) => {
      const r = kanvas.getBoundingClientRect();
      yakinlas(1.8, e.clientX - r.left, e.clientY - r.top);
    });
    let surukle: { x: number; y: number; n: number; e: number } | null = null;
    kanvas.addEventListener('pointerdown', (e) => {
      kanvas.setPointerCapture(e.pointerId);
      surukle = { x: e.clientX, y: e.clientY, n: gorunum.n, e: gorunum.e };
      kanvas.classList.add('surukleniyor');
    });
    kanvas.addEventListener('pointermove', (e) => {
      const r = kanvas.getBoundingClientRect();
      const px = e.clientX - r.left;
      const py = e.clientY - r.top;
      const ee = gorunum.e + (px - r.width / 2) / gorunum.olcek;
      const nn = gorunum.n - (py - r.height / 2) / gorunum.olcek;
      yaz(imlec, `K ${sayi(nn, 2).padStart(7)}  D ${sayi(ee, 2).padStart(7)}  m`);
      if (surukle) {
        if (takipEt && (Math.abs(e.clientX - surukle.x) > 3 || Math.abs(e.clientY - surukle.y) > 3)) {
          takipEt = false;
          takip.setAttribute('aria-pressed', 'false');
        }
        gorunum.e = surukle.e - (e.clientX - surukle.x) / gorunum.olcek;
        gorunum.n = surukle.n + (e.clientY - surukle.y) / gorunum.olcek;
        kirli = true;
      }
    });
    const birak = (): void => {
      surukle = null;
      kanvas.classList.remove('surukleniyor');
    };
    kanvas.addEventListener('pointerup', birak);
    kanvas.addEventListener('pointercancel', birak);

    // ---- Çizim ----
    function ciz(): void {
      const { g, y, oran } = kanvasBoyutla(kanvas);
      const ctx = kanvas.getContext('2d');
      if (!ctx) return;
      const G = g / oran;
      const Y = y / oran;
      ctx.setTransform(oran, 0, 0, oran, 0, 0);
      if (takipEt && arac) {
        gorunum.n = arac.n;
        gorunum.e = arac.e;
      }
      const s = gorunum.olcek;
      const X = (e: number): number => G / 2 + (e - gorunum.e) * s;
      const Yy = (n: number): number => Y / 2 - (n - gorunum.n) * s;

      // zemin
      const zemin = ctx.createRadialGradient(G / 2, Y / 2, 0, G / 2, Y / 2, Math.max(G, Y) * 0.75);
      zemin.addColorStop(0, '#0b131b');
      zemin.addColorStop(1, '#070b10');
      ctx.fillStyle = zemin;
      ctx.fillRect(0, 0, G, Y);

      // ızgara
      const adim = guzelAdim(90 / s);
      const ince = adim / 5;
      const eSol = gorunum.e - G / 2 / s;
      const eSag = gorunum.e + G / 2 / s;
      const nAlt = gorunum.n - Y / 2 / s;
      const nUst = gorunum.n + Y / 2 / s;
      ctx.lineWidth = 1;
      if (ince * s > 14) {
        ctx.strokeStyle = 'rgba(140, 170, 200, 0.045)';
        ctx.beginPath();
        for (let e = Math.ceil(eSol / ince) * ince; e <= eSag; e += ince) {
          ctx.moveTo(Math.round(X(e)) + 0.5, 0);
          ctx.lineTo(Math.round(X(e)) + 0.5, Y);
        }
        for (let n = Math.ceil(nAlt / ince) * ince; n <= nUst; n += ince) {
          ctx.moveTo(0, Math.round(Yy(n)) + 0.5);
          ctx.lineTo(G, Math.round(Yy(n)) + 0.5);
        }
        ctx.stroke();
      }
      ctx.strokeStyle = 'rgba(140, 170, 200, 0.11)';
      ctx.beginPath();
      for (let e = Math.ceil(eSol / adim) * adim; e <= eSag; e += adim) {
        ctx.moveTo(Math.round(X(e)) + 0.5, 0);
        ctx.lineTo(Math.round(X(e)) + 0.5, Y);
      }
      for (let n = Math.ceil(nAlt / adim) * adim; n <= nUst; n += adim) {
        ctx.moveTo(0, Math.round(Yy(n)) + 0.5);
        ctx.lineTo(G, Math.round(Yy(n)) + 0.5);
      }
      ctx.stroke();

      // eksen etiketleri (ızgara değerleri)
      ctx.font = '10.5px ui-monospace, Menlo, Consolas, monospace';
      ctx.fillStyle = 'rgba(160, 180, 200, 0.45)';
      ctx.textBaseline = 'top';
      ctx.textAlign = 'left';
      for (let e = Math.ceil(eSol / adim) * adim; e <= eSag; e += adim) {
        const x = X(e);
        if (x > 40 && x < G - 40) ctx.fillText(`D ${sayi(e, adim < 1 ? 1 : 0)}`, x + 4, 56);
      }
      ctx.textBaseline = 'bottom';
      for (let n = Math.ceil(nAlt / adim) * adim; n <= nUst; n += adim) {
        const yy = Yy(n);
        if (yy > 70 && yy < Y - 20) ctx.fillText(`K ${sayi(n, adim < 1 ? 1 : 0)}`, 8, yy - 3);
      }

      // orijin
      ctx.strokeStyle = 'rgba(200, 215, 230, 0.35)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(X(0) - 7, Yy(0));
      ctx.lineTo(X(0) + 7, Yy(0));
      ctx.moveTo(X(0), Yy(0) - 7);
      ctx.lineTo(X(0), Yy(0) + 7);
      ctx.stroke();
      ctx.fillStyle = 'rgba(200, 215, 230, 0.45)';
      ctx.textAlign = 'left';
      ctx.textBaseline = 'top';
      ctx.fillText('O', X(0) + 5, Yy(0) + 4);

      // yol noktaları
      if (yolNoktalari.length > 0) {
        ctx.strokeStyle = C.yol;
        ctx.globalAlpha = 0.55;
        ctx.setLineDash([2, 4]);
        ctx.lineWidth = 1.2;
        ctx.beginPath();
        yolNoktalari.forEach((p, i) => (i === 0 ? ctx.moveTo(X(p.e), Yy(p.n)) : ctx.lineTo(X(p.e), Yy(p.n))));
        ctx.stroke();
        ctx.setLineDash([]);
        ctx.globalAlpha = 1;
        yolNoktalari.forEach((p, i) => {
          const x = X(p.e);
          const yy = Yy(p.n);
          ctx.fillStyle = '#0b1218';
          ctx.strokeStyle = C.yol;
          ctx.lineWidth = 1.4;
          ctx.beginPath();
          ctx.arc(x, yy, 9, 0, Math.PI * 2);
          ctx.fill();
          ctx.stroke();
          ctx.fillStyle = C.yol;
          ctx.font = '600 10px ui-monospace, Menlo, Consolas, monospace';
          ctx.textAlign = 'center';
          ctx.textBaseline = 'middle';
          ctx.fillText(String(i + 1), x, yy + 0.5);
          ctx.textAlign = 'left';
          ctx.font = '10.5px ui-monospace, Menlo, Consolas, monospace';
          ctx.fillStyle = C.metin3;
          ctx.textBaseline = 'bottom';
          ctx.fillText(`${sayi(p.d, 1)} m`, x + 11, yy - 9);
        });
      }

      // hedefler (senaryo konumu; canlı gövde konumu varsa onu kullan)
      const hedefCiz = (n: number, e: number, d: number, ad: string): void => {
        const x = X(e);
        const yy = Yy(n);
        ctx.strokeStyle = C.hedef;
        ctx.lineWidth = 1.6;
        ctx.beginPath();
        ctx.moveTo(x, yy - 9);
        ctx.lineTo(x + 9, yy);
        ctx.lineTo(x, yy + 9);
        ctx.lineTo(x - 9, yy);
        ctx.closePath();
        ctx.stroke();
        ctx.beginPath();
        ctx.arc(x, yy, 1.8, 0, Math.PI * 2);
        ctx.fillStyle = C.hedef;
        ctx.fill();
        ctx.fillStyle = C.metin;
        ctx.textBaseline = 'middle';
        ctx.fillText(`${ad} · ${sayi(d, 1)} m`, x + 14, yy);
      };
      if (canliHedef.size > 0) {
        let i = 0;
        for (const [id, h] of canliHedef) hedefCiz(h.n, h.e, h.d, hedefler[i++]?.ad ?? `gövde ${id}`);
      } else for (const h of hedefler) hedefCiz(h.n, h.e, h.d, h.ad);

      // izler
      const izCiz = (iz: Iz, rk: string, kesik: boolean, kalinlik: number): void => {
        if (iz.boy < 4) return;
        ctx.strokeStyle = rk;
        ctx.lineWidth = kalinlik;
        ctx.lineJoin = 'round';
        ctx.lineCap = 'round';
        ctx.setLineDash(kesik ? [6, 4] : []);
        ctx.beginPath();
        const v = iz.veri;
        ctx.moveTo(X(v[1]!), Yy(v[0]!));
        for (let i = 2; i < iz.boy; i += 2) ctx.lineTo(X(v[i + 1]!), Yy(v[i]!));
        ctx.stroke();
        ctx.setLineDash([]);
      };
      izCiz(gercekIz, C.gercek, false, 2);
      izCiz(kestirimIz, C.kestirim, true, 1.5);

      // araç simgeleri
      const aracCiz = (n: number, e: number, yon: number, rk: string, dolu: boolean): void => {
        const x = X(e);
        const yy = Yy(n);
        const boy = Math.max(15, 0.6 * s);
        ctx.save();
        ctx.translate(x, yy);
        ctx.rotate(yon); // ekranda kuzey yukarı, doğu sağ: pruva açısı saat yönünde
        ctx.beginPath();
        ctx.moveTo(0, -boy * 0.62);
        ctx.lineTo(boy * 0.4, boy * 0.42);
        ctx.lineTo(0, boy * 0.22);
        ctx.lineTo(-boy * 0.4, boy * 0.42);
        ctx.closePath();
        if (dolu) {
          ctx.fillStyle = rk;
          ctx.shadowColor = 'rgba(0,0,0,0.6)';
          ctx.shadowBlur = 6;
          ctx.fill();
          ctx.shadowBlur = 0;
          ctx.strokeStyle = 'rgba(255,255,255,0.75)';
          ctx.lineWidth = 1;
          ctx.stroke();
        } else {
          ctx.strokeStyle = rk;
          ctx.lineWidth = 1.6;
          ctx.stroke();
        }
        ctx.restore();
      };
      if (kestirim && arac) {
        ctx.strokeStyle = 'rgba(217, 89, 38, 0.6)';
        ctx.lineWidth = 1;
        ctx.setLineDash([2, 2]);
        ctx.beginPath();
        ctx.moveTo(X(arac.e), Yy(arac.n));
        ctx.lineTo(X(kestirim.e), Yy(kestirim.n));
        ctx.stroke();
        ctx.setLineDash([]);
      }
      if (kestirim) aracCiz(kestirim.n, kestirim.e, kestirim.yon ?? 0, C.kestirim, false);
      if (arac) aracCiz(arac.n, arac.e, arac.yon, C.gercek, true);

      // ölçek çubuğu
      const olcekM = guzelAdim(110 / s);
      const olcekPx = olcekM * s;
      const ox = G - 16 - olcekPx - 46;
      const oy = Y - 58;
      ctx.fillStyle = 'rgba(14,19,25,0.85)';
      ctx.fillRect(ox - 6, oy - 20, olcekPx + 12 + 40, 30);
      ctx.strokeStyle = C.metin;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.moveTo(ox, oy - 4);
      ctx.lineTo(ox, oy);
      ctx.lineTo(ox + olcekPx, oy);
      ctx.lineTo(ox + olcekPx, oy - 4);
      ctx.stroke();
      ctx.fillStyle = C.metin;
      ctx.textAlign = 'left';
      ctx.textBaseline = 'bottom';
      ctx.font = '600 11px ui-monospace, Menlo, Consolas, monospace';
      ctx.fillText(`${sayi(olcekM, olcekM < 1 ? 1 : 0)} m`, ox + olcekPx + 6, oy + 4);

      // kuzey oku
      const kx = G - 300;
      const ky = 34;
      if (kx > 200) {
        ctx.strokeStyle = 'rgba(200,215,230,0.35)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.arc(kx, ky, 16, 0, Math.PI * 2);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(kx, ky - 13);
        ctx.lineTo(kx + 5, ky + 4);
        ctx.lineTo(kx, ky + 1);
        ctx.lineTo(kx - 5, ky + 4);
        ctx.closePath();
        ctx.fillStyle = C.metin;
        ctx.fill();
        ctx.font = '700 10px ui-monospace, Menlo, Consolas, monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'top';
        ctx.fillText('K', kx, ky + 19);
      }
    }

    function bilgiGuncelle(): void {
      if (arac) {
        oKuzey.ayarla(sayi(arac.n, 2), kestirim ? `kart ${sayi(kestirim.n, 2)}` : 'kart —');
        oDogu.ayarla(sayi(arac.e, 2), kestirim ? `kart ${sayi(kestirim.e, 2)}` : 'kart —');
        oDerinlik.ayarla(sayi(arac.d, 2));
        oYon.ayarla(yonDerece(arac.yon), kestirim?.yon != null ? `kart ${yonDerece(kestirim.yon)}` : 'kart —');
        oHiz.ayarla(sayi(arac.hiz, 2));
        oFark.ayarla(kestirim ? sayi(Math.hypot(kestirim.n - arac.n, kestirim.e - arac.e), 2) : '—', 'yatay');
      }
      yaz(izNot, `iz ${sayi(gercekIz.uzunluk(), 1)} m · sim ${simZamani(simT)}`);
    }

    let sonCizim = 0;
    let sonBilgi = 0;
    const dongu = (t: number): void => {
      requestAnimationFrame(dongu);
      if (document.hidden) return;
      if (t - sonBilgi > 250) {
        sonBilgi = t;
        bilgiGuncelle();
      }
      if (!kirli || t - sonCizim < 33) return;
      sonCizim = t;
      kirli = false;
      ciz();
    };
    requestAnimationFrame(dongu);
    new ResizeObserver(() => {
      kirli = true;
    }).observe(kanvas);
  },
  { icerikSinifi: 'tam' },
);
