// Küçük DOM yardımcıları (çatı kullanılmaz).

type Cocuk = Node | string | null | undefined | false;

export function el<K extends keyof HTMLElementTagNameMap>(
  etiket: K,
  ozellik: Record<string, string | boolean | undefined> | null = null,
  ...cocuklar: Cocuk[]
): HTMLElementTagNameMap[K] {
  const e = document.createElement(etiket);
  if (ozellik !== null) {
    for (const [k, v] of Object.entries(ozellik)) {
      if (v === undefined || v === false) continue;
      if (k === 'class') e.className = String(v);
      else e.setAttribute(k, v === true ? '' : v);
    }
  }
  for (const c of cocuklar) {
    if (c === null || c === undefined || c === false) continue;
    e.append(c);
  }
  return e;
}

/** Metni yalnız değiştiyse yazar (gereksiz yeniden yerleşimi önler). */
export function yaz(e: Element, metin: string): void {
  if (e.textContent !== metin) e.textContent = metin;
}

/** Sınıf listesinden yalnız biri etkin olacak biçimde ayarlar. */
export function sinifSec(e: Element, secenekler: readonly string[], secilen: string | null): void {
  for (const s of secenekler) e.classList.toggle(s, s === secilen);
}

export function ozellikYaz(e: Element, ad: string, deger: string): void {
  if (e.getAttribute(ad) !== deger) e.setAttribute(ad, deger);
}

/** Kart: başlık çubuğu + gövde. */
export function kart(baslik: string, ...sag: Cocuk[]): { kok: HTMLElement; bas: HTMLElement; govde: HTMLElement } {
  const bas = el('div', { class: 'kart-bas' }, el('span', { class: 'kart-baslik' }, baslik), el('span', { class: 'bosluk' }), ...sag);
  const govde = el('div', { class: 'kart-govde' });
  const kok = el('section', { class: 'kart' }, bas, govde);
  return { kok, bas, govde };
}

/** Etiket + büyük değer + alt satır. */
export function okuma(etiket: string, birim = ''): { kok: HTMLElement; deger: HTMLElement; alt: HTMLElement; ayarla(d: string, a?: string, sinif?: string | null): void } {
  const sayi = el('span');
  const birimEl = el('span', { class: 'birim' }, birim);
  const deger = el('div', { class: 'deger' }, sayi, birim ? birimEl : null);
  const alt = el('div', { class: 'alt' });
  const kok = el('div', { class: 'okuma' }, el('span', { class: 'etiket' }, etiket), deger, alt);
  return {
    kok,
    deger,
    alt,
    ayarla(d: string, a?: string, sinif: string | null = null) {
      yaz(sayi, d);
      if (a !== undefined) yaz(alt, a);
      sinifSec(sayi, ['yok', 'gecersiz-deger'], d === '—' ? 'yok' : sinif);
    },
  };
}

/** Yerel depolama: her erişim try/catch içinde; yoksa sessizce varsayılan. */
export function depoOku<T>(anahtar: string, varsayilan: T): T {
  try {
    const v = localStorage.getItem('ida.' + anahtar);
    return v === null ? varsayilan : (JSON.parse(v) as T);
  } catch {
    return varsayilan;
  }
}

export function depoYaz(anahtar: string, deger: unknown): void {
  try {
    localStorage.setItem('ida.' + anahtar, JSON.stringify(deger));
  } catch {
    /* yok say */
  }
}

/** Kanvası kapsayıcısına ve aygıt piksel oranına göre boyutlandırır; boyut değiştiyse true. */
export function kanvasBoyutla(k: HTMLCanvasElement): { g: number; y: number; oran: number; degisti: boolean } {
  const r = k.getBoundingClientRect();
  const oran = Math.min(2, window.devicePixelRatio || 1);
  const g = Math.max(1, Math.round(r.width * oran));
  const y = Math.max(1, Math.round(r.height * oran));
  const degisti = k.width !== g || k.height !== y;
  if (degisti) {
    k.width = g;
    k.height = y;
  }
  return { g, y, oran, degisti };
}

/** tema.css değerlerinin kopyası: stil dosyası henüz uygulanmadıysa kullanılır. */
const RENK_YEDEK: Record<string, string> = {
  '--veri-gercek': '#3987e5',
  '--veri-kestirim': '#d95926',
  '--veri-olcum': '#199e70',
  '--veri-4': '#c98500',
  '--veri-5': '#d55181',
  '--veri-6': '#008300',
  '--veri-7': '#9085e9',
  '--veri-8': '#e66767',
  '--veri-hedef': '#d55181',
  '--veri-yol': '#c9d3dc',
  '--metin': '#d9e1e8',
  '--metin-2': '#9eabb8',
  '--metin-3': '#6b7885',
  '--vurgu': '#67e8f9',
};

/** CSS değişkeninin değeri. */
export function renk(ad: string): string {
  const v = getComputedStyle(document.documentElement).getPropertyValue(ad).trim();
  return v !== '' ? v : (RENK_YEDEK[ad] ?? '#9eabb8');
}
