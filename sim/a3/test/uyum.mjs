// W4-a3 kabul 2: C++ kodlayıcının ürettiği çerçeveler panelin çözücüsüyle (web/src/a3/cerceve.ts)
// aynı değerleri verir. Kullanım: node uyum.mjs <ida_a3_test> <web dizini> <çıktı dizini>
// Çıkış: 0 geçti, 1 kaldı, 77 atlandı (web/node_modules yok).

import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { ATLA, a3Kitapligi, karsilastir } from './ortak.mjs';

const [ikili, web, cikti] = process.argv.slice(2);
const a3 = await a3Kitapligi(web, cikti);
if (a3 === null) {
  console.log('ATLANDI: web/node_modules/esbuild yok (web/ dizininde npm ci)');
  process.exit(ATLA);
}
const dosya = join(cikti, 'uyum_ornekleri.json');
const r = spawnSync(ikili, ['uyum_ornekleri', dosya], { stdio: 'inherit' });
if (r.status !== 0) {
  console.log('örnekler üretilemedi');
  process.exit(1);
}
const { ornekler } = JSON.parse(readFileSync(dosya, 'utf8'));
const hatalar = [];
const cozucu = new a3.Cozucu();
const kanallar = new Set();
for (const o of ornekler) {
  const bayt = Uint8Array.from(Buffer.from(o.ileti_hex, 'hex'));
  const c = cozucu.ileti(bayt);
  if (c.length !== 1) {
    hatalar.push(`${o.kanal}: ${c.length} çerçeve çözüldü (1 bekleniyordu)`);
    continue;
  }
  const k = c[0];
  kanallar.add(k.kanal.ad);
  if (k.kanal.ad !== o.kanal) hatalar.push(`${o.kanal}: kanal adı ${k.kanal.ad}`);
  if (String(k.baslik.t_ns) !== o.baslik.t_ns) hatalar.push(`${o.kanal}: t_ns`);
  for (const a of ['sira', 'kanal_id', 'kanal_surumu', 'bayraklar']) {
    if (k.baslik[a] !== o.baslik[a]) hatalar.push(`${o.kanal}: başlık ${a} ${k.baslik[a]} ≠ ${o.baslik[a]}`);
  }
  if (k.baslik.yuk_uzunlugu !== bayt.length - a3.BASLIK_BAYT) hatalar.push(`${o.kanal}: yuk_uzunlugu`);
  karsilastir(k.yuk, o.beklenen, o.kanal, hatalar);
}
const s = cozucu.sayaclar;
if (s.istemci_bozuk_cerceve !== 0 || s.istemci_bilinmeyen_kanal !== 0 || s.istemci_kisa_kalan !== 0) {
  hatalar.push(`çözücü sayaçları: ${JSON.stringify(s)}`);
}
for (const h of hatalar.slice(0, 40)) console.log('  KALDI', h);
console.log(`uyum_web_cozucu: ${ornekler.length} kanal (${[...kanallar].join(', ')}); ${hatalar.length === 0 ? 'GEÇTİ' : 'KALDI'}`);
process.exit(hatalar.length === 0 && ornekler.length === 15 ? 0 : 1);
