// `a3-kanallar.json` `$defs` şemalarına karşı küçük JSON Schema denetleyicisi (yalnız
// sahte sunucu ve testler; yeni bağımlılık yok). Desteklenen anahtarlar bu dosyadaki
// şemaların kullandıkları kadardır: $ref, type, const, enum, properties, required,
// additionalProperties (false ya da şema), items, minimum, maximum, minLength, maxLength,
// pattern, maxItems, oneOf. `format` ve `description` yok sayılır.

type Sema = Record<string, unknown>;

export interface SemaKatalogu {
  $defs: Record<string, Sema>;
}

function turUyar(tur: string, d: unknown): boolean {
  switch (tur) {
    case 'string':
      return typeof d === 'string';
    case 'integer':
      return typeof d === 'number' && Number.isInteger(d);
    case 'number':
      return typeof d === 'number' && Number.isFinite(d);
    case 'boolean':
      return typeof d === 'boolean';
    case 'null':
      return d === null;
    case 'object':
      return d !== null && typeof d === 'object' && !Array.isArray(d);
    case 'array':
      return Array.isArray(d);
    default:
      return true;
  }
}

/** Hata listesi döndürür; boşsa değer şemaya uyar. */
export function semaDogrula(katalog: SemaKatalogu, sema: Sema, deger: unknown, yol = '$'): string[] {
  const hatalar: string[] = [];
  const ref = sema.$ref;
  if (typeof ref === 'string') {
    const ad = ref.replace('#/$defs/', '');
    const hedef = katalog.$defs[ad];
    if (hedef === undefined) return [`${yol}: şema bulunamadı (${ref})`];
    return semaDogrula(katalog, hedef, deger, yol);
  }
  const tur = sema.type;
  if (tur !== undefined) {
    const turler = Array.isArray(tur) ? (tur as string[]) : [tur as string];
    if (!turler.some((t) => turUyar(t, deger))) return [`${yol}: tür ${turler.join('|')} bekleniyordu`];
  }
  if ('const' in sema && sema.const !== deger) hatalar.push(`${yol}: const ${JSON.stringify(sema.const)} bekleniyordu`);
  if (Array.isArray(sema.enum) && !sema.enum.includes(deger)) hatalar.push(`${yol}: değer enum dışı (${JSON.stringify(deger)})`);
  if (typeof deger === 'number') {
    if (typeof sema.minimum === 'number' && deger < sema.minimum) hatalar.push(`${yol}: ${deger} < ${sema.minimum}`);
    if (typeof sema.maximum === 'number' && deger > sema.maximum) hatalar.push(`${yol}: ${deger} > ${sema.maximum}`);
  }
  if (typeof deger === 'string') {
    if (typeof sema.minLength === 'number' && deger.length < sema.minLength) hatalar.push(`${yol}: çok kısa`);
    if (typeof sema.maxLength === 'number' && deger.length > sema.maxLength) hatalar.push(`${yol}: çok uzun`);
    if (typeof sema.pattern === 'string' && !new RegExp(sema.pattern, 'u').test(deger)) hatalar.push(`${yol}: kalıba uymuyor (${sema.pattern})`);
  }
  if (Array.isArray(deger)) {
    if (typeof sema.maxItems === 'number' && deger.length > sema.maxItems) hatalar.push(`${yol}: çok öğe`);
    if (sema.items !== undefined) deger.forEach((d, i) => hatalar.push(...semaDogrula(katalog, sema.items as Sema, d, `${yol}[${i}]`)));
  }
  if (deger !== null && typeof deger === 'object' && !Array.isArray(deger)) {
    const nesne = deger as Record<string, unknown>;
    const oz = (sema.properties ?? {}) as Record<string, Sema>;
    for (const k of (sema.required as string[] | undefined) ?? []) if (!(k in nesne)) hatalar.push(`${yol}.${k}: zorunlu alan yok`);
    for (const [k, v] of Object.entries(nesne)) {
      const alt = oz[k];
      if (alt !== undefined) hatalar.push(...semaDogrula(katalog, alt, v, `${yol}.${k}`));
      else if (sema.additionalProperties === false) hatalar.push(`${yol}.${k}: beklenmeyen alan`);
      else if (typeof sema.additionalProperties === 'object' && sema.additionalProperties !== null) {
        hatalar.push(...semaDogrula(katalog, sema.additionalProperties as Sema, v, `${yol}.${k}`));
      }
    }
  }
  if (Array.isArray(sema.oneOf)) {
    const uyan = (sema.oneOf as Sema[]).filter((s) => semaDogrula(katalog, s, deger, yol).length === 0).length;
    if (uyan !== 1) hatalar.push(`${yol}: oneOf: ${uyan} seçenek uyuyor (1 bekleniyordu)`);
  }
  return hatalar;
}

export function semaOku(katalogMetni: string): SemaKatalogu {
  return JSON.parse(katalogMetni) as SemaKatalogu;
}
