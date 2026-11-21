// Geliştirme: panelleri izleme kipinde derler, sahte A3 sunucusunu başlatır
// ve kaynakları değişince yeniden başlatır.
//
//   npm run gelistir [-- --port 47100 --kip hil_1x --atla 60]
//
// Sahte sunucu bu oturum boyunca aynı jetonla yeniden başlatılır; böylece açık
// paneller sunucu dönünce kendiliğinden toparlanır (yalnız geliştirme kolaylığı;
// gerçek sim sunucusu her başlatmada yeni jeton üretir, A3-K04).

import * as esbuild from 'esbuild';
import { spawn } from 'node:child_process';
import { randomBytes } from 'node:crypto';
import { watch } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { panelAyari, sahteAyari, sayfalariKopyala } from './derle.mjs';

const kok = dirname(dirname(fileURLToPath(import.meta.url)));
const ekArgumanlar = process.argv.slice(2);
const jeton = randomBytes(32).toString('hex');

let cocuk = null;
function sahteBaslat() {
  if (cocuk !== null) {
    const eski = cocuk;
    cocuk = null;
    eski.once('exit', sahteBaslat);
    eski.kill('SIGTERM');
    return;
  }
  cocuk = spawn(process.execPath, ['.derleme/sahte-a3.mjs', '--statik', 'dist', '--jeton', jeton, ...ekArgumanlar], {
    cwd: kok,
    stdio: 'inherit',
  });
  cocuk.on('exit', (kod, sinyal) => {
    if (sinyal === null && kod !== 0) console.error(`sahte sunucu çıktı (kod ${kod})`);
  });
}

sayfalariKopyala();
const panel = await esbuild.context({
  ...panelAyari({ uretim: false }),
  plugins: [
    {
      name: 'bildir',
      setup(b) {
        b.onEnd((s) => {
          if (s.errors.length === 0) console.log(`[${new Date().toLocaleTimeString('tr-TR')}] paneller derlendi`);
        });
      },
    },
  ],
});
await panel.watch();

const sahte = await esbuild.context({
  ...sahteAyari(),
  plugins: [
    {
      name: 'yeniden-baslat',
      setup(b) {
        b.onEnd((s) => {
          if (s.errors.length === 0) sahteBaslat();
        });
      },
    },
  ],
});
await sahte.watch();

watch(join(kok, 'sayfalar'), { recursive: true }, () => {
  try {
    sayfalariKopyala();
  } catch (h) {
    console.error(h);
  }
});

const kapat = async () => {
  await panel.dispose();
  await sahte.dispose();
  if (cocuk !== null) cocuk.kill('SIGTERM');
  process.exit(0);
};
process.on('SIGINT', kapat);
process.on('SIGTERM', kapat);
