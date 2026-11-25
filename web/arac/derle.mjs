// Panelleri ve sahte A3 sunucusunu esbuild ile derler.
//
//   node arac/derle.mjs                 üretim derlemesi: dist/ (paneller) + .derleme/sahte-a3.mjs
//   node arac/derle.mjs --yalniz-sahte  yalnız sahte sunucu
//
// Statik yollar A3-K11'e uyar: kök sayfa `/`, geri kalan her şey `/panel/...`.

import * as esbuild from 'esbuild';
import { cpSync, mkdirSync, readdirSync, rmSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const kok = dirname(dirname(fileURLToPath(import.meta.url)));
export const PANELLER = ['index', 'durum', 'grafik', 'harita', 'sonar', 'atis', 'senaryo'];

/** Panel derleme ayarı (gelistir.mjs de kullanır). */
export function panelAyari({ uretim }) {
  return {
    absWorkingDir: kok,
    entryPoints: [
      ...PANELLER.map((p) => ({ in: `src/paneller/${p}.ts`, out: `panel/${p}` })),
      { in: 'src/ortak/stil.css', out: 'panel/varlik/tema' },
    ],
    outdir: 'dist',
    bundle: true,
    format: 'esm',
    splitting: true,
    chunkNames: 'panel/parca/[name]-[hash]',
    target: ['es2022', 'chrome110', 'edge110', 'safari16'],
    minify: uretim,
    sourcemap: uretim ? 'linked' : 'inline',
    legalComments: 'linked',
    logLevel: 'warning',
    metafile: true,
  };
}

export function sahteAyari() {
  return {
    absWorkingDir: kok,
    entryPoints: ['arac/sahte-a3/ana.ts'],
    outfile: '.derleme/sahte-a3.mjs',
    bundle: true,
    platform: 'node',
    format: 'esm',
    target: 'node22',
    packages: 'external',
    sourcemap: 'inline',
    logLevel: 'warning',
  };
}

export function sayfalariKopyala() {
  const kaynak = join(kok, 'sayfalar');
  mkdirSync(join(kok, 'dist', 'panel', 'varlik'), { recursive: true });
  cpSync(join(kaynak, 'index.html'), join(kok, 'dist', 'index.html'));
  for (const f of readdirSync(join(kaynak, 'panel'))) {
    cpSync(join(kaynak, 'panel', f), join(kok, 'dist', 'panel', f), { recursive: true });
  }
}

async function ana() {
  const yalnizSahte = process.argv.includes('--yalniz-sahte');
  await esbuild.build(sahteAyari());
  if (yalnizSahte) return;
  rmSync(join(kok, 'dist'), { recursive: true, force: true });
  const sonuc = await esbuild.build(panelAyari({ uretim: true }));
  sayfalariKopyala();
  let toplam = 0;
  for (const [ad, c] of Object.entries(sonuc.metafile.outputs)) {
    if (ad.endsWith('.map')) continue;
    toplam += c.bytes;
  }
  console.log(`dist/ yazıldı: ${Object.keys(sonuc.metafile.outputs).length} dosya, ${(toplam / 1024).toFixed(1)} KiB (kaynak haritaları hariç)`);
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  await ana();
}
