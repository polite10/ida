// A3 makinece okunur kataloğundan (docs/sozlesmeler/sema/a3-kanallar.json)
// istemci kitaplığının tip ve düzen tanımlarını üretir: src/a3/katalog.ts.
//
// Kullanım:
//   node arac/uret-katalog.mjs            dosyayı yazar
//   node arac/uret-katalog.mjs --denetle  dosya güncel değilse hata koduyla çıkar
//
// Katalog dondurulmuş sözleşmenin parçasıdır; bu betik onu yalnız okur.

import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const kok = dirname(dirname(fileURLToPath(import.meta.url)));
const katalogYolu = join(kok, '..', 'docs', 'sozlesmeler', 'sema', 'a3-kanallar.json');
const hedefYol = join(kok, 'src', 'a3', 'katalog.ts');

const baytlar = readFileSync(katalogYolu);
const ozet = 'sha256:' + createHash('sha256').update(baytlar).digest('hex');
const k = JSON.parse(baytlar.toString('utf8'));

const TAMSAYI = new Set(['u8', 'u16', 'u32', 'u64', 'i32', 'i64']);
const YOK_ETIKETLERI = new Set(['NaN', 'INT64_MIN', '2^64-1', '0xFFFFFFFF', '255', '-1', '0']);

function pascal(ad) {
  return ad
    .split(/[_/]/)
    .filter(Boolean)
    .map((p) => p[0].toUpperCase() + p.slice(1))
    .join('');
}

function js(deger) {
  return JSON.stringify(deger);
}

function yorum(alan) {
  const parca = [];
  if (alan.birim && alan.birim !== '-') parca.push(`[${alan.birim}]`);
  if (alan.anlam) parca.push(alan.anlam);
  if (alan.degerler) {
    parca.push('Değerler: ' + Object.entries(alan.degerler).map(([a, b]) => `${a}=${b}`).join(', ') + '.');
  }
  if (alan.yok_degeri !== undefined) parca.push(`Yok: ${alan.yok_degeri} → null.`);
  return parca.join(' ').replaceAll('*/', '* /');
}

/** Alan tanımını TypeScript nesne değişmezine çevirir. */
function alanDegismezi(alan, girinti) {
  if (!alan.tip || (!TAMSAYI.has(alan.tip) && !['f32', 'f64', 'metin', 'bayt_dizisi', 'dizi'].includes(alan.tip))) {
    throw new Error(`bilinmeyen tel tipi: ${alan.ad} ${alan.tip}`);
  }
  const p = [`ad: ${js(alan.ad)}`, `tip: ${js(alan.tip)}`];
  if (alan.adet !== undefined) p.push(`adet: ${alan.adet}`);
  if (alan.sayac !== undefined) p.push(`sayac: ${js(alan.sayac)}`);
  if (alan.azami !== undefined) p.push(`azami: ${alan.azami}`);
  if (alan.yok_degeri !== undefined) {
    const y = String(alan.yok_degeri);
    if (!YOK_ETIKETLERI.has(y)) throw new Error(`bilinmeyen yok değeri: ${alan.ad} ${y}`);
    p.push(`yok: ${js(y)}`);
  }
  if (alan.aralik !== undefined) {
    const [a, b] = alan.aralik;
    // 2^53 üstü sınırlar (yalnız başlıktaki t_ns) tipin aralığıdır; Number'a güvenle sığmaz.
    if (Number.isSafeInteger(a) || !Number.isInteger(a)) {
      if (Number.isSafeInteger(b) || !Number.isInteger(b)) p.push(`aralik: [${a}, ${b}]`);
    }
  }
  if (alan.degerler !== undefined) {
    const d = Object.entries(alan.degerler).map(([kod, ad]) => `${kod}: ${js(ad)}`).join(', ');
    p.push(`degerler: { ${d} }`);
  }
  p.push(`birim: ${js(alan.birim ?? '-')}`);
  if (alan.oge !== undefined) {
    const ic = alan.oge.map((o) => alanDegismezi(o, girinti + '    ')).join(',\n');
    p.push(`oge: [\n${ic},\n${girinti}  ]`);
  }
  return `${girinti}  { ${p.join(', ')} }`;
}

function tsTipi(alan, ogeAdi) {
  if (TAMSAYI.has(alan.tip) || alan.tip === 'f32' || alan.tip === 'f64') {
    // 64 bit tamsayılar kayıpsız: bigint (2^53 üstü Number'a sığmaz)
    const t = alan.tip === 'u64' || alan.tip === 'i64' ? 'bigint' : 'number';
    if (alan.adet !== undefined) return alan.yok_degeri !== undefined ? `(${t} | null)[]` : `${t}[]`;
    return alan.yok_degeri !== undefined ? `${t} | null` : t;
  }
  if (alan.tip === 'metin') return 'string';
  if (alan.tip === 'bayt_dizisi') return 'Uint8Array';
  if (alan.tip === 'dizi') return `${ogeAdi}[]`;
  throw new Error(alan.tip);
}

function arayuzMetni(ad, alanlar, onEk) {
  const satirlar = [];
  for (const alan of alanlar) {
    const ogeAdi = onEk + pascal(alan.ad) + 'Oge';
    satirlar.push(`  /** ${yorum(alan)} */\n  ${alan.ad}: ${tsTipi(alan, ogeAdi)};`);
  }
  return `export interface ${ad} {\n${satirlar.join('\n')}\n}\n`;
}

const out = [];
out.push(`// ÜRETİLDİ — elle değiştirme. Üreten: web/arac/uret-katalog.mjs
// Kaynak: docs/sozlesmeler/sema/a3-kanallar.json (A3 ${k.x_ida_sozlesme.surum}, dondurulmuş)
// Kaynak özeti: ${ozet}
/* eslint-disable */

export const A3_SURUMU = ${js(k.x_ida_sozlesme.surum)};
export const WS_ALT_PROTOKOLU = ${js(k.x_ida_sozlesme.ws_alt_protokolu)};
export const HTTP_TABAN_YOLU = ${js(k.x_ida_sozlesme.http_taban_yolu)};
/** İstemcinin derlendiği katalog dosyasının özeti (A3-K16 biçimi). */
export const KATALOG_OZETI = ${js(ozet)};
export const BASLIK_BAYT = ${k.x_ida_cerceve_basligi_bayt};

export type TelTipi = 'u8' | 'u16' | 'u32' | 'u64' | 'i32' | 'i64' | 'f32' | 'f64' | 'metin' | 'bayt_dizisi' | 'dizi';
export type YokEtiketi = 'NaN' | 'INT64_MIN' | '2^64-1' | '0xFFFFFFFF' | '255' | '-1' | '0';

export interface AlanTanimi {
  readonly ad: string;
  readonly tip: TelTipi;
  readonly adet?: number;
  readonly sayac?: 'u8' | 'u16' | 'u32';
  readonly azami?: number;
  readonly yok?: YokEtiketi;
  readonly aralik?: readonly [number, number];
  readonly degerler?: Readonly<Record<number, string>>;
  readonly birim: string;
  readonly oge?: readonly AlanTanimi[];
}

export type KanalSinifi = 'durum' | 'dizi' | 'olay';
export type ZamanAnlami = 'sim' | 'olcum' | 'alim' | 'gonderim';

export interface KanalTanimi {
  readonly kanal_id: number;
  readonly ad: string;
  readonly kanal_surumu: number;
  readonly yuk: YukAdi;
  readonly sinif: KanalSinifi;
  readonly azami_hz: number | null;
  readonly varsayilan_hz: number | null;
  readonly kuyruk_siniri: number | null;
  readonly zaman_anlami: ZamanAnlami;
  readonly anahtar: readonly string[];
  readonly zorunlu_abonelik: boolean;
  readonly yer_gercegi: boolean;
}
`);

// Yük düzenleri
const yukler = k.x_ida_yukler;
out.push('/** Yük düzenleri: alanlar tel sırasıyla (A3 §3.4). */');
out.push('export const YUKLER = {');
for (const [ad, alanlar] of Object.entries(yukler)) {
  out.push(`  ${ad}: [\n${alanlar.map((a) => alanDegismezi(a, '  ')).join(',\n')},\n  ],`);
}
out.push('} as const satisfies Record<string, readonly AlanTanimi[]>;\n');
out.push('export type YukAdi = keyof typeof YUKLER;\n');

// Tipler
const ekArayuzler = [];
const anaArayuzler = [];
for (const [ad, alanlar] of Object.entries(yukler)) {
  const tip = pascal(ad);
  const satirlar = [];
  for (const alan of alanlar) {
    const ogeAdi = tip + pascal(alan.ad) + 'Oge';
    if (alan.tip === 'dizi') ekArayuzler.push(arayuzMetni(ogeAdi, alan.oge, tip + pascal(alan.ad)));
    satirlar.push(`  /** ${yorum(alan)} */\n  ${alan.ad}: ${tsTipi(alan, ogeAdi)};`);
  }
  anaArayuzler.push(`export interface ${tip} {\n${satirlar.join('\n')}\n}\n`);
}
out.push(...ekArayuzler, ...anaArayuzler);

out.push(`/** Kodlayıcı girdisi: 64 bit alanlarda güvenli tamsayı (number) da kabul edilir. */
export type KodlamaGirdisi<T> = T extends bigint
  ? bigint | number
  : T extends Uint8Array
    ? Uint8Array
    : T extends readonly (infer U)[]
      ? KodlamaGirdisi<U>[]
      : T extends object
        ? { [K in keyof T]: KodlamaGirdisi<T[K]> }
        : T;
`);
out.push('/** Yük adından çözülmüş yük tipine. */');
out.push('export interface YukTipleri {');
for (const ad of Object.keys(yukler)) out.push(`  ${ad}: ${pascal(ad)};`);
out.push('}\n');

// Kanallar
out.push('/** Kanal kataloğu (A3 §3.3). */');
out.push('export const KANALLAR = [');
for (const c of k.x_ida_kanallar) {
  out.push(
    `  { kanal_id: ${c.kanal_id}, ad: ${js(c.ad)}, kanal_surumu: ${c.kanal_surumu}, yuk: ${js(c.yuk)}, sinif: ${js(c.sinif)}, ` +
      `azami_hz: ${js(c.azami_hz)}, varsayilan_hz: ${js(c.varsayilan_hz)}, kuyruk_siniri: ${js(c.kuyruk_siniri)}, ` +
      `zaman_anlami: ${js(c.zaman_anlami)}, anahtar: ${js(c.anahtar)}, zorunlu_abonelik: ${js(c.zorunlu_abonelik)}, yer_gercegi: ${js(c.yer_gercegi)} },`,
  );
}
out.push('] as const satisfies readonly KanalTanimi[];\n');
out.push('export type KanalAdi = (typeof KANALLAR)[number][\'ad\'];\n');
out.push('/** Kanal adından yük tipine. */');
out.push('export interface KanalYuku {');
for (const c of k.x_ida_kanallar) out.push(`  ${js(c.ad)}: ${pascal(c.yuk)};`);
out.push('}\n');

// Olay türleri, geçerlilik nedenleri, hata kodları
out.push('/** Olay türleri (A3 §3.5). */');
out.push('export const OLAY_TURLERI = [');
for (const o of k.x_ida_olay_turleri) {
  out.push(`  { kod: ${o.kod}, ad: ${js(o.ad)}, varsayilan_onem: ${js(o.varsayilan_onem)} },`);
}
out.push('] as const;\n');
out.push('/** Geçerlilik neden bitleri (A3 Tablo 4.8). */');
out.push('export const GECERLILIK_NEDENLERI: Readonly<Record<number, string>> = {');
for (const [bit, ad] of Object.entries(k.x_ida_gecerlilik_nedenleri)) out.push(`  ${bit}: ${js(ad)},`);
out.push('};\n');
out.push('/** Hata kodları (A3 §3.9). */');
out.push('export const HATA_KODLARI = [');
for (const h of k.x_ida_hata_kodlari) out.push(`  { kod: ${js(h.kod)}, http: ${h.http} },`);
out.push('] as const;\n');
out.push('export type HataKodu = (typeof HATA_KODLARI)[number][\'kod\'];\n');
out.push('/** WebSocket kapanış kodları (A3 §3.7). */');
out.push('export const WS_KAPANIS_KODLARI: Readonly<Record<number, string>> = {');
for (const [kod, anlam] of Object.entries(k.x_ida_ws_kapanis_kodlari)) {
  out.push(`  ${kod}: ${js(typeof anlam === 'string' ? anlam : JSON.stringify(anlam))},`);
}
out.push('};\n');

const metin = out.join('\n');

if (process.argv.includes('--denetle')) {
  const var_ = existsSync(hedefYol) ? readFileSync(hedefYol, 'utf8') : '';
  if (var_ !== metin) {
    console.error('src/a3/katalog.ts güncel değil: `npm run katalog` çalıştırın.');
    process.exit(1);
  }
  console.log('katalog.ts güncel (' + ozet + ')');
} else {
  writeFileSync(hedefYol, metin);
  console.log('yazıldı: src/a3/katalog.ts (' + ozet + ')');
}
