// W5-web-komut: komut katmanı (kabul, ret, süre aşımı, kopma, olay kaçması) ve çalıştırma
// zinciri. İstek ve yanıt gövdeleri a3-kanallar.json şemalarına karşı doğrulanır; durum
// iddiaları sunucunun bildirdiği değerlerle (GET /durum ve kanal) yapılır (W5-K44).

import assert from 'node:assert/strict';
import { after, test } from 'node:test';
import { A3Http } from '../src/a3/http';
import { KomutYoneticisi, type KomutKaydi } from '../src/a3/komut';
import { JETON, kosul, kur, semayaUy, type Kurulum } from './yardimci-komut';

const acik: Kurulum[] = [];
after(async () => {
  for (const k of acik) await k.kapat();
});
async function yeni(ayar: Parameters<typeof kur>[0] = {}): Promise<Kurulum> {
  const k = await kur(ayar);
  acik.push(k);
  return k;
}
const komutIstekleri = (k: Kurulum): Kurulum['istekler'] => k.istekler.filter((i) => i.yontem === 'POST' && i.yol.startsWith('/calistirma/'));

test('kabul: başlat → duraklat → adımla → sürdür → hız → durdur; her adımda sunucunun bildirdiği durum', async () => {
  const k = await yeni();
  const d0 = await k.durum();
  assert.equal(d0.durum, 'yuklendi');
  assert.equal(d0.run_id, null);

  const b = await k.komutlar.gonder('baslat');
  assert.equal(b.asama, 'uygulandi', b.ileti);
  await k.durumBekle('kosuyor');
  const dKos = await k.durum();
  assert.equal(dKos.durum, 'kosuyor');
  assert.match(dKos.run_id ?? '', /^\d{8}-\d{6}-[0-9a-f]{6}$/);

  const dur = await k.komutlar.gonder('duraklat');
  assert.equal(dur.asama, 'uygulandi', dur.ileti);
  await k.durumBekle('duraklatildi');
  const dDur = await k.durum();
  assert.equal(dDur.durum, 'duraklatildi');
  assert.equal(dDur.run_id, dKos.run_id);

  // adımla: sim zamanı ilerler, çalıştırma yeniden duraklatılır
  await kosul(() => Number(k.baglanti.son('calistirma/durum')?.yuk.durum_rev) === dDur.durum_rev, 3000, 'kanal durum_rev güncel');
  const t0 = dDur.sim_t_ns;
  const adim = await k.komutlar.gonder('adimla', { periyot_sayisi: 20 });
  assert.equal(adim.asama, 'uygulandi', adim.ileti);
  await kosul(() => Number(k.baglanti.son('calistirma/durum')?.yuk.durum_rev) > dDur.durum_rev + 1, 3000, 'adımlama bitti');
  const dAdim = await k.durum();
  assert.equal(dAdim.durum, 'duraklatildi');
  assert.ok(dAdim.sim_t_ns > t0, 'adımlama sim zamanını ilerletti');

  await kosul(() => Number(k.baglanti.son('calistirma/durum')?.yuk.durum_rev) === dAdim.durum_rev, 3000, 'kanal durum_rev güncel');
  const sur = await k.komutlar.gonder('surdur');
  assert.equal(sur.asama, 'uygulandi', sur.ileti);
  await k.durumBekle('kosuyor');
  assert.equal((await k.durum()).durum, 'kosuyor');

  // kanal, sunucunun son durum_rev'ine yetişsin (komut ekranın gösterdiği revizyonla gider)
  const sonRev = (await k.durum()).durum_rev;
  await kosul(() => Number(k.baglanti.son('calistirma/durum')?.yuk.durum_rev) === sonRev, 3000, 'kanal durum_rev güncel');
  const hiz = await k.komutlar.gonder('hiz', { carpan: 4 });
  assert.equal(hiz.asama, 'uygulandi', hiz.ileti);
  await kosul(() => k.baglanti.son('calistirma/durum')?.yuk.hedef_carpan === 4, 3000, 'hedef_carpan 4 (sunucudan)');

  const sonRev2 = (await k.durum()).durum_rev;
  await kosul(() => Number(k.baglanti.son('calistirma/durum')?.yuk.durum_rev) === sonRev2, 3000, 'kanal durum_rev güncel');
  const dur2 = await k.komutlar.gonder('durdur');
  assert.equal(dur2.asama, 'uygulandi', dur2.ileti);
  await k.durumBekle('durduruldu');
  assert.equal((await k.durum()).durum, 'durduruldu');

  // Gövdeler sözleşme şemalarına uyar; Ida-Istemci başlığı her komutta var
  const ist = komutIstekleri(k);
  assert.deepEqual(ist.map((i) => i.yol), ['/calistirma/baslat', '/calistirma/duraklat', '/calistirma/adimla', '/calistirma/surdur', '/calistirma/hiz', '/calistirma/durdur']);
  const sema: Record<string, string> = { baslat: 'istek_baslat', duraklat: 'istek_duraklat', adimla: 'istek_adimla', surdur: 'istek_calistirma', hiz: 'istek_hiz', durdur: 'istek_calistirma' };
  for (const i of ist) {
    const ad = i.yol.split('/').pop()!;
    semayaUy(sema[ad]!, i.govde, `${ad} isteği`);
    semayaUy('yanit_kuyrukta', i.yanit, `${ad} yanıtı`);
    assert.equal(i.http, 202);
    assert.match(i.baslik['ida-istemci'] ?? '', /^\d+$/, `${ad}: Ida-Istemci başlığı`);
    assert.equal(i.baslik['content-type'], 'application/json');
    assert.match(i.baslik.authorization ?? '', new RegExp(`^Bearer ${JETON}$`));
  }
  // Revizyon alanları: başlatta run_id null, sonrakilerde çalıştırmanın kimliği
  assert.equal((ist[0]!.govde as { beklenen_run_id: unknown }).beklenen_run_id, null);
  for (const i of ist.slice(1)) assert.equal((i.govde as { beklenen_run_id: unknown }).beklenen_run_id, dKos.run_id);
  // Her komut ayrı istek kimliği taşır
  assert.equal(new Set(ist.map((i) => (i.govde as { istek_kimligi: string }).istek_kimligi)).size, ist.length);
});

test('W5-K44: komut kuyrukta iken ekranın durumu değişmez; "kuyrukta" ve "uygulandı" ayrı gösterilir', async () => {
  const k = await yeni();
  k.sunucu.test.komutlariTut(true);
  const asamalar: string[] = [];
  k.komutlar.dinle((liste) => {
    const son = liste[0];
    if (son !== undefined && asamalar.at(-1) !== son.asama) asamalar.push(son.asama);
  });
  const bekleyen = k.komutlar.gonder('baslat');
  await kosul(() => k.komutlar.kayitlar()[0]?.asama === 'kuyrukta', 3000, 'kuyrukta');
  await k.senkron();
  assert.equal(k.komutlar.kayitlar()[0]!.asama, 'kuyrukta', 'sunucu uygulamadan "uygulandı" gösterilmez');
  assert.equal(k.kanalDurumu(), 'yuklendi', 'komut gitti ama sunucu durumu değiştirmedi: ekran yuklendi der');
  k.sunucu.test.komutlariTut(false);
  const sonuc = await bekleyen;
  assert.equal(sonuc.asama, 'uygulandi');
  await k.durumBekle('hazirlaniyor');
  assert.deepEqual(asamalar.filter((a, i, d) => d.indexOf(a) === i), ['gonderiliyor', 'kuyrukta', 'uygulandi']);
});

test('ret: eşzamanlı ret (409 gecersiz_durum) görünür ve Türkçe; yanıt yanit_ret şemasına uyar', async () => {
  const k = await yeni();
  const r = await k.komutlar.gonder('duraklat'); // yuklendi durumunda geçersiz (Tablo 4.7a)
  assert.equal(r.asama, 'reddedildi');
  assert.equal(r.hata_kodu, 'gecersiz_durum');
  assert.match(r.ileti, /durumunda geçerli değil/);
  assert.equal(r.gonderildi_mi, true);
  assert.ok(r.komut_no !== null && r.komut_no >= 1, 'ret yanıtı komut_no taşır (A3-K52)');
  const i = komutIstekleri(k).at(-1)!;
  assert.equal(i.http, 409);
  semayaUy('yanit_ret', i.yanit, 'ret yanıtı');
  // Sunucu durumu değişmedi
  assert.equal((await k.durum()).durum, 'yuklendi');
});

test('ret: eski durum revizyonu (durum_rev_uyusmuyor) ve eski çalıştırma kimliği (beklenen_durum_uyusmuyor) ayrıntılarıyla', async () => {
  const k = await yeni();
  const gercek = k.komutlar.beklenen.bind(k.komutlar);
  k.komutlar.beklenen = () => ({ run_id: null, durum_rev: 999_999 });
  const r1 = await k.komutlar.gonder('baslat');
  assert.equal(r1.asama, 'reddedildi');
  assert.equal(r1.hata_kodu, 'durum_rev_uyusmuyor');
  assert.match(r1.ileti, /güncel revizyon: \d+/);
  k.komutlar.beklenen = () => ({ run_id: '20200101-000000-abcdef', durum_rev: gercek()!.durum_rev });
  const r2 = await k.komutlar.gonder('baslat');
  assert.equal(r2.hata_kodu, 'beklenen_durum_uyusmuyor');
  k.komutlar.beklenen = gercek;
  for (const i of komutIstekleri(k)) semayaUy('yanit_ret', i.yanit, 'ret');
  assert.equal((await k.durum()).durum, 'yuklendi', 'reddedilen komut durumu değiştirmedi');
});

test('ret: 202 sonrası reddedilen komut komut_sonucu olayıyla "reddedildi" olur (yarışan iki komut)', async () => {
  const k = await yeni();
  assert.equal((await k.komutlar.gonder('baslat')).asama, 'uygulandi');
  await k.durumBekle('kosuyor');
  await k.senkron();
  k.sunucu.test.komutlariTut(true);
  const a = k.komutlar.gonder('duraklat');
  await kosul(() => k.komutlar.kayitlar()[0]?.asama === 'kuyrukta', 3000, 'ilk duraklat kuyrukta');
  // Aynı revizyonu gören ikinci komut: girişte kabul edilir, uygulama sınırında reddedilir (A3-K54, K61)
  const yedek = k.komutlar.kayitlar()[0]!;
  const b2 = k.komutlar.gonder('duraklat');
  await kosul(() => k.komutlar.kayitlar().filter((x) => x.asama === 'kuyrukta').length === 2, 3000, 'ikinci duraklat da kuyrukta');
  k.sunucu.test.komutlariTut(false);
  const [ra, rb] = await Promise.all([a, b2]);
  assert.equal(ra.asama, 'uygulandi');
  assert.equal(rb.asama, 'reddedildi');
  assert.equal(rb.hata_kodu, 'durum_rev_uyusmuyor');
  assert.ok(yedek === ra);
  assert.equal((await k.durum()).durum, 'duraklatildi');
});

test('süre aşımı: sonuç gelmezse "bilinmiyor"; geç gelen komut_sonucu kaydı düzeltir', async () => {
  const k = await yeni({ komut: { sureMs: 400, baslatSureMs: 400, olayBeklemeMs: 100, yoklamaMs: 60 } });
  k.sunucu.test.komutlariTut(true);
  const t0 = Date.now();
  const r = await k.komutlar.gonder('baslat');
  assert.equal(r.asama, 'bilinmiyor');
  assert.equal(r.hata_kodu, 'zaman_asimi');
  assert.match(r.ileti, /bilinmiyor/);
  assert.ok(Date.now() - t0 >= 380 && Date.now() - t0 < 3000, 'süre sınırına uyuldu');
  assert.equal(k.kanalDurumu(), 'yuklendi', 'bilinmiyor iken ekran durumu uydurulmaz');
  k.sunucu.test.komutlariTut(false);
  await kosul(() => r.asama === 'uygulandi', 3000, 'geç gelen sonuç kaydı düzeltir');
  await k.durumBekle('hazirlaniyor');
});

test('olay kaçarsa GET /komutlar/{komut_no} ile sonuç bulunur (A3-K60)', async () => {
  const k = await yeni({ komut: { sureMs: 3000, olayBeklemeMs: 80, yoklamaMs: 50 } });
  // Olayları hiç iletmeyen bağlantı vekili: yalnız durum çerçevesi ve HTTP çalışır
  const vekil = {
    http: k.baglanti.http,
    bilgi: k.baglanti.bilgi,
    son: k.baglanti.son.bind(k.baglanti),
    olaylar: () => () => undefined,
    durumDinle: () => () => undefined,
  };
  const y = new KomutYoneticisi(vekil as never, { sureMs: 3000, olayBeklemeMs: 80, yoklamaMs: 50 });
  const r = await y.gonder('baslat');
  assert.equal(r.asama, 'uygulandi', r.ileti);
  const sorgu = k.istekler.filter((i) => i.yontem === 'GET' && i.yol.startsWith('/komutlar/'));
  assert.ok(sorgu.length >= 1, 'sonuç HTTP ile soruldu');
  semayaUy('yanit_komut', sorgu.at(-1)!.yanit, 'GET /komutlar');
  y.kapat();
});

test('bağlantı koptuğunda bekleyen komutun sonucu "bilinmiyor"; toparlanınca gerçek sonuç gelir', async () => {
  const k = await yeni({ komut: { sureMs: 10_000, olayBeklemeMs: 5000 } });
  k.sunucu.test.komutlariTut(true);
  const bekleyen = k.komutlar.gonder('baslat');
  await kosul(() => k.komutlar.kayitlar()[0]?.asama === 'kuyrukta', 3000, 'kuyrukta');
  k.sunucu.baglantilariKopar();
  const r = await bekleyen;
  assert.equal(r.asama, 'bilinmiyor');
  assert.equal(r.hata_kodu, 'baglanti_koptu');
  assert.match(r.ileti, /Bağlantı koptu/);
  // Bağlantı yokken yeni komut gönderilmez
  await kosul(() => k.baglanti.bilgi.durum !== 'bagli', 2000, 'kopuk');
  const yeniKomut = await k.komutlar.gonder('sifirla');
  assert.equal(yeniKomut.gonderildi_mi, false);
  assert.equal(yeniKomut.hata_kodu, 'durum_yok');
  // Sunucu komutu şimdi uygular; toparlanan bağlantı sonucu öğrenir ve kaydı düzeltir
  k.sunucu.test.komutlariTut(false);
  await kosul(() => k.baglanti.bilgi.durum === 'bagli', 5000, 'yeniden bağlandı');
  await kosul(() => r.asama === 'uygulandi', 5000, 'geç sonuç kaydı düzeltti');
});

test('ağ hatası: istek sunucuya ulaşmış olabilir → "bilinmiyor" (ağ hatası), ret sayılmaz', async () => {
  let hata = false;
  const k = await yeni({ fetchHatasi: () => hata });
  hata = true;
  const r = await k.komutlar.gonder('baslat');
  hata = false;
  assert.equal(r.asama, 'bilinmiyor');
  assert.equal(r.hata_kodu, 'ag_hatasi');
  assert.match(r.ileti, /bilinmiyor/);
});

test('çift tıklama: yanıt beklenirken aynı komut ikinci kez gönderilmez', async () => {
  const k = await yeni();
  const a = k.komutlar.gonder('baslat');
  const b = k.komutlar.gonder('baslat');
  const [ra, rb] = await Promise.all([a, b]);
  assert.equal(rb.gonderildi_mi, false);
  assert.equal(rb.hata_kodu, 'gonderiliyor');
  assert.equal(ra.asama, 'uygulandi');
  assert.equal(komutIstekleri(k).length, 1);
});

test('HIL kipi: duraklat onaysız onay_gerekli; adımla ve hız kipte_gecersiz (Tablo 4.7b)', async () => {
  const k = await yeni({ kip: 'hil_1x' });
  assert.equal((await k.komutlar.gonder('baslat')).asama, 'uygulandi');
  await k.durumBekle('kosuyor');
  await k.senkron();
  const r = await k.komutlar.gonder('duraklat');
  assert.equal(r.hata_kodu, 'onay_gerekli');
  const h = await k.komutlar.gonder('hiz', { carpan: 2 });
  assert.equal(h.hata_kodu, 'kipte_gecersiz');
  const onayli = await k.komutlar.gonder('duraklat', { gecersizligi_onayla: true });
  assert.equal(onayli.asama, 'uygulandi', onayli.ileti);
  semayaUy('istek_duraklat', komutIstekleri(k).at(-1)!.govde, 'onaylı duraklat');
});

test('hız: 0 ile 0,01 arası değer sunucuda 422 gecersiz_deger olarak görünür', async () => {
  const k = await yeni();
  await k.komutlar.gonder('baslat');
  await k.durumBekle('kosuyor');
  await k.senkron();
  const r = await k.komutlar.gonder('hiz', { carpan: 0.005 });
  assert.equal(r.asama, 'reddedildi');
  assert.equal(r.hata_kodu, 'gecersiz_deger');
  assert.match(r.ileti, /aralığın dışında/);
});

test('istemci kaydı: Ida-Istemci olmadan komut 400; katman kayıtlı kimliği kullanır', async () => {
  const k = await yeni();
  const ham = await fetch(`http://127.0.0.1:${k.sunucu.port}/api/v1/calistirma/baslat`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${JETON}`, 'Content-Type': 'application/json' },
    body: JSON.stringify({ istek_kimligi: 'abcdefgh1', beklenen_run_id: null, beklenen_durum_rev: 1 }),
  });
  assert.equal(ham.status, 400);
  const http = new A3Http(`http://127.0.0.1:${k.sunucu.port}`, JETON);
  await assert.rejects(http.calistirma('duraklat', { run_id: null, durum_rev: 1 }), (h: { kod: string }) => h.kod === 'gecersiz_durum');
});

export type { KomutKaydi };
