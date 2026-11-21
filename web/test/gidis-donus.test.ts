// Kabul 3: sahte sunucunun dünyasının bir tam senaryo döngüsünde ürettiği her
// yük, sunucunun kodlama yoluyla (yukKodla + cerceveKodla) çerçevelenip istemci
// çözücüsünden geçer ve aynı değeri verir (f32 alanlar binary32 yuvarlamasıyla).

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { SahteDunya } from '../arac/sahte-a3/dunya';
import { Cozucu, kanalBul } from '../src/a3/cerceve';
import { YUKLER, type AlanTanimi, type YukAdi } from '../src/a3/katalog';
import { cerceveKodla, iletiBirlestir, yukKodla } from '../src/a3/kodlayici';

function normal(alanlar: readonly AlanTanimi[], nesne: Record<string, unknown>): Record<string, unknown> {
  const o: Record<string, unknown> = {};
  for (const a of alanlar) {
    const v = nesne[a.ad];
    if (a.tip === 'dizi') o[a.ad] = (v as Record<string, unknown>[]).map((x) => normal(a.oge!, x));
    else if (a.tip === 'bayt_dizisi') o[a.ad] = Array.from(v as Uint8Array);
    else if (a.tip === 'f32') o[a.ad] = Array.isArray(v) ? v.map((x) => (x === null ? null : Math.fround(x as number))) : v === null ? null : Math.fround(v as number);
    else if (a.tip === 'u64' || a.tip === 'i64') {
      const b = (x: unknown): unknown => (x === null ? null : BigInt(x as number | bigint));
      o[a.ad] = Array.isArray(v) ? v.map(b) : b(v);
    } else o[a.ad] = v;
  }
  return o;
}

function duz(v: unknown): unknown {
  if (v instanceof Uint8Array) return Array.from(v);
  if (Array.isArray(v)) return v.map(duz);
  if (v !== null && typeof v === 'object') return Object.fromEntries(Object.entries(v).map(([k, x]) => [k, duz(x)]));
  return v;
}

for (const kip of ['sil', 'hil_1x'] as const) {
  test(`sahte dünya (${kip}): tam döngüde üretilen her çerçeve çözücüden aynı değerle geçer`, () => {
    const d = new SahteDunya({ kip, tohum: 7 });
    const cozucu = new Cozucu();
    const kanallar = new Map<string, number>();
    let olaySira = 0;
    let olaySayisi = 0;
    let yeniCalistirma = 0;
    let sonRun = '';
    // ~185 s duvar saati: bir koşu (~150 s), bitiş, sıfırlama ve yeni koşunun başı
    for (let i = 0; i < 9250; i++) {
      const uretim = d.ilerle(20);
      const cerceveler: Uint8Array[] = [];
      const beklenen: unknown[] = [];
      for (const u of uretim) {
        const k = kanalBul(u.kanal)!;
        const yuk = yukKodla(k.yuk, u.yuk as never);
        cerceveler.push(cerceveKodla(k, { t_ns: u.t_ns, sira: (kanallar.get(u.kanal) ?? 0) + 1 }, yuk));
        beklenen.push({ kanal: u.kanal, t_ns: u.t_ns, yuk: normal(YUKLER[k.yuk as YukAdi], u.yuk as unknown as Record<string, unknown>) });
        kanallar.set(u.kanal, (kanallar.get(u.kanal) ?? 0) + 1);
      }
      for (const o of d.olayKuyrugu.splice(0)) {
        const yukNesne = { tur: o.tur, onem: o.onem, kaynak: o.kaynak, adim_no: o.adim_no, komut_no: 0, istemci_id: 0, ayrinti: JSON.stringify(o.ayrinti) };
        cerceveler.push(cerceveKodla(kanalBul('olay')!, { t_ns: d.t_ns, sira: ++olaySira }, yukKodla('yuk_olay', yukNesne)));
        beklenen.push({ kanal: 'olay', t_ns: d.t_ns, yuk: { ...yukNesne, adim_no: o.adim_no === null ? null : BigInt(o.adim_no) } });
        olaySayisi++;
        if (o.tur === 1 && o.ayrinti['sonraki'] === 'kosuyor') yeniCalistirma++;
      }
      if (cerceveler.length === 0) continue;
      const cozulen = cozucu.ileti(iletiBirlestir(cerceveler));
      assert.equal(cozulen.length, beklenen.length);
      cozulen.forEach((c, j) => {
        const b = beklenen[j] as { kanal: string; t_ns: number; yuk: unknown };
        assert.equal(c.kanal.ad, b.kanal);
        assert.equal(c.baslik.t_ns, BigInt(b.t_ns));
        assert.deepEqual(duz(c.yuk), duz(b.yuk), `${b.kanal} t=${b.t_ns}`);
        assert.deepEqual(c.gecersiz, [], `${b.kanal}: geçersiz alan ${c.gecersiz.join(',')}`);
      });
      if (d.run_id !== '' && d.run_id !== sonRun) sonRun = d.run_id;
    }
    assert.equal(cozucu.sayaclar.istemci_bozuk_cerceve, 0);
    assert.equal(cozucu.sayaclar.istemci_gecersiz_alan, 0);
    assert.ok(yeniCalistirma >= 2, 'senaryo bitip yeniden başlamalı');
    assert.ok(olaySayisi > 10);
    const beklenenKanallar = [
      'calistirma/durum',
      'sayac/gercek_zaman',
      'fizik/durum',
      'kart/durum',
      'yer_istasyonu/durum',
      'atis/durum',
      'emniyet/girdiler',
      'ariza/etkin',
      'sensor/imu',
      'sensor/manyetometre',
      'sensor/basinc',
      'sensor/gnss',
      'sensor/dvl',
      'sensor/batarya',
      'sensor/sizinti',
      'sensor/firlatici',
      'sonar/ping360',
      'sonar/ping1d',
      'kart/cekirdek_sayaclari',
      'kayit/durum',
      'surec/durum',
      'baglanti/durum',
      ...(kip === 'hil_1x' ? ['kart/baglanti_sayaclari'] : []),
    ];
    for (const k of beklenenKanallar) assert.ok((kanallar.get(k) ?? 0) > 0, `${k} üretilmedi`);
  });
}

test('senaryo zaman çizelgesi: silahsız başlar, 3 m dalar, itici arızası, emniyet olayları', () => {
  const d = new SahteDunya({ tohum: 3 });
  const turler = new Set<string>();
  let enDerin = 0;
  let ilkSilahli: number | null = null;
  let ariza = false;
  for (let i = 0; i < 8000 && d.durum !== 'bitti'; i++) {
    for (const u of d.ilerle(20)) {
      if (u.kanal === 'fizik/durum') {
        const y = u.yuk as { govdeler: { tur: number; p_n: number[] }[] };
        enDerin = Math.max(enDerin, y.govdeler.find((g) => g.tur === 0)!.p_n[2]!);
      }
      if (u.kanal === 'kart/durum' && ilkSilahli === null && (u.yuk as { silahli: number }).silahli === 1) ilkSilahli = u.t_ns;
      if (u.kanal === 'ariza/etkin' && (u.yuk as { arizalar: { fault_id: string }[] }).arizalar.some((f) => f.fault_id === 'itici_zayiflama')) ariza = true;
    }
    for (const o of d.olayKuyrugu.splice(0)) turler.add(String(o.tur));
  }
  assert.equal(d.durum, 'bitti');
  assert.ok(ilkSilahli !== null && ilkSilahli >= 3e9, 'araç silahsız başlamalı');
  assert.ok(enDerin > 2.9 && enDerin < 3.4, `en derin ${enDerin}`);
  assert.ok(ariza, 'itici arızası etkin olmalı');
  for (const t of ['1', '6', '7', '14', '21']) assert.ok(turler.has(t), `olay türü ${t} yok`);
});
