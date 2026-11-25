// Sahte A3 sunucusunun komut satırı girişi (yalnız geliştirme).
//
//   node .derleme/sahte-a3.mjs [--port 47100] [--statik dist] [--kip sil|hil_1x]
//                              [--atla <s>] [--tohum <n>] [--jeton <64 hex>]
//                              [--emniyet olagan|bilinmiyor|gecersiz] [--elle]
//
// `--elle`: dünya kendiliğinden başlamaz ve bitince yeniden başlamaz; çalıştırma yalnız panelden
// (A3 komutlarıyla: başlat, duraklat, adımla, …) ilerler.
//
// `--emniyet bilinmiyor`: kart geri okuması ve emniyet zinciri durumu bilinmiyor (255).
// `--emniyet gecersiz`: emniyet kanallarında değer listesi dışı değerler (gösterim sınaması).
//
// `--jeton` yalnız geliştirme içindir (sunucu yeniden başlatılınca açık
// panellerin aynı jetonla toparlanması için); gerçek sim sunucusu her
// başlatmada yeni jeton üretir (A3-K04).

import { existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { parseArgs } from 'node:util';
import { sahteSunucuBaslat } from './sunucu';

const { values } = parseArgs({
  options: {
    port: { type: 'string', default: '47100' },
    statik: { type: 'string' },
    katalog: { type: 'string' },
    kip: { type: 'string', default: 'sil' },
    atla: { type: 'string' },
    tohum: { type: 'string' },
    jeton: { type: 'string' },
    emniyet: { type: 'string', default: 'olagan' },
    elle: { type: 'boolean', default: false },
  },
});

const kip = values.kip === 'hil_1x' ? 'hil_1x' : 'sil';
const emniyetKipi = values.emniyet === 'bilinmiyor' || values.emniyet === 'gecersiz' ? values.emniyet : 'olagan';
const statik = values.statik ?? (existsSync(resolve('dist')) ? resolve('dist') : null);
const katalogYolu = values.katalog ?? resolve('..', 'docs', 'sozlesmeler', 'sema', 'a3-kanallar.json');

const sunucu = await sahteSunucuBaslat({
  port: Number(values.port),
  statik,
  katalogYolu,
  kip,
  emniyetKipi,
  elle: values.elle === true,
  ...(values.atla !== undefined ? { atla: Number(values.atla) } : {}),
  ...(values.tohum !== undefined ? { tohum: Number(values.tohum) } : {}),
  ...(values.jeton !== undefined ? { jeton: values.jeton } : {}),
});

process.stdout.write(
  `İDA sahte A3 sunucusu — SAHTE VERİ (kinematik senaryo, fizik değil); kip ${kip}${values.elle === true ? '; elle yönetim' : ''}${emniyetKipi === 'olagan' ? '' : `; emniyet ${emniyetKipi}`}\n` +
    `Açılış adresi: ${sunucu.acilisAdresi}\n`,
);

let kapaniyor = false;
const kapat = (): void => {
  if (kapaniyor) return;
  kapaniyor = true;
  void sunucu.kapat().then(() => process.exit(0));
};
process.on('SIGINT', kapat);
process.on('SIGTERM', kapat);
