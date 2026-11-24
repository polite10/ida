// sim/a3 Node uyum testlerinin ortak yardımcıları. web/'e dosya eklemez: panelin A3 istemci
// kitaplığını (web/src/a3) web/node_modules içindeki esbuild ile derleme dizinine paketler.
// Node'un yerleşik TypeScript tip silme özelliği kullanılmaz (Rust tabanlı araç yasağı).

import { existsSync, mkdirSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

export const ATLA = 77;

/** web/src/a3'ü paketler ve modülü döndürür; esbuild yoksa null. */
export async function a3Kitapligi(webDizini, ciktiDizini) {
  const web = resolve(webDizini);
  const cikti = resolve(ciktiDizini);
  const esbuildYolu = join(web, 'node_modules', 'esbuild', 'lib', 'main.js');
  if (!existsSync(esbuildYolu)) return null;
  const m = await import(pathToFileURL(esbuildYolu).href);
  const esbuild = m.default ?? m;
  mkdirSync(cikti, { recursive: true });
  const dosya = join(cikti, 'a3-istemci.mjs');
  await esbuild.build({
    absWorkingDir: web,
    entryPoints: [join(web, 'src', 'a3', 'index.ts')],
    outfile: dosya,
    bundle: true,
    platform: 'node',
    format: 'esm',
    target: 'node22',
    logLevel: 'warning',
  });
  return import(pathToFileURL(dosya).href + `?t=${Date.now()}`);
}

/** Beklenen (C++ json_dok) değerle panel çözücüsünün değerini karşılaştırır. */
export function karsilastir(gercek, beklenen, yol, hatalar) {
  const hata = (n) => hatalar.push(`${yol}: ${n} (gerçek ${fmt(gercek)}, beklenen ${fmt(beklenen)})`);
  if (beklenen === null) {
    if (gercek !== null) hata('yok bekleniyordu');
    return;
  }
  if (typeof beklenen === 'string') {
    if (typeof gercek === 'bigint') {
      if (String(gercek) !== beklenen) hata('64 bit farklı');
    } else if (beklenen === 'NaN') {
      if (!(typeof gercek === 'number' && Number.isNaN(gercek))) hata('NaN bekleniyordu');
    } else if (beklenen === 'Infinity' || beklenen === '-Infinity') {
      if (gercek !== Number(beklenen)) hata('sonsuz bekleniyordu');
    } else if (gercek !== beklenen) {
      hata('metin farklı');
    }
    return;
  }
  if (typeof beklenen === 'number') {
    if (typeof gercek !== 'number' || gercek !== beklenen) hata('sayı farklı');
    return;
  }
  if (Array.isArray(beklenen)) {
    const g = gercek instanceof Uint8Array ? Array.from(gercek) : gercek;
    if (!Array.isArray(g) || g.length !== beklenen.length) {
      hata('dizi boyu farklı');
      return;
    }
    beklenen.forEach((b, i) => karsilastir(g[i], b, `${yol}[${i}]`, hatalar));
    return;
  }
  if (typeof beklenen === 'object') {
    if (gercek === null || typeof gercek !== 'object') {
      hata('nesne bekleniyordu');
      return;
    }
    for (const k of Object.keys(beklenen)) karsilastir(gercek[k], beklenen[k], `${yol}.${k}`, hatalar);
    for (const k of Object.keys(gercek)) if (!(k in beklenen)) hatalar.push(`${yol}.${k}: beklenmeyen alan`);
    return;
  }
  hata('desteklenmeyen tip');
}

function fmt(v) {
  if (typeof v === 'bigint') return `${v}n`;
  if (v instanceof Uint8Array) return `Uint8Array(${v.length})`;
  try {
    return JSON.stringify(v)?.slice(0, 80);
  } catch {
    return String(v);
  }
}

export function bekle(kosul, ms, ne) {
  return new Promise((coz, red) => {
    const son = Date.now() + ms;
    const t = setInterval(() => {
      if (kosul()) {
        clearInterval(t);
        coz();
      } else if (Date.now() > son) {
        clearInterval(t);
        red(new Error(`zaman aşımı: ${ne}`));
      }
    }, 10);
  });
}
