// Testleri esbuild ile JavaScript'e çevirir ve Node'un yerleşik test
// koşucusuyla çalıştırır. Node'un yerleşik TypeScript tip silme özelliği
// kullanılmaz (Rust tabanlı araç yasağı; W3-web "Araçlar").

import * as esbuild from 'esbuild';
import { spawnSync } from 'node:child_process';
import { readdirSync, rmSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const kok = dirname(dirname(fileURLToPath(import.meta.url)));
const cikti = join(kok, '.derleme-test');
rmSync(cikti, { recursive: true, force: true });

const testler = readdirSync(join(kok, 'test')).filter((f) => f.endsWith('.test.ts'));
await esbuild.build({
  absWorkingDir: kok,
  entryPoints: testler.map((f) => ({ in: `test/${f}`, out: f.replace(/\.ts$/, '') })),
  outdir: cikti,
  outExtension: { '.js': '.mjs' },
  bundle: true,
  platform: 'node',
  format: 'esm',
  target: 'node22',
  packages: 'external',
  sourcemap: 'inline',
  logLevel: 'warning',
});

const dosyalar = readdirSync(cikti)
  .filter((f) => f.endsWith('.test.mjs'))
  .map((f) => join(cikti, f));
// --test-force-exit: testler bittikten sonra Node'un HTTP istemcisinin canlı tuttuğu
// soketleri beklememek için (kitaplığın kendi zamanlayıcıları kapat() ile temizlenir).
const sonuc = spawnSync(process.execPath, ['--test', '--test-force-exit', '--test-reporter=spec', ...dosyalar], {
  cwd: kok,
  stdio: 'inherit',
});
process.exit(sonuc.status ?? 1);
