// Senaryo akışı: listele → dosyayı oku → doğrula → yükle → başlat; ön denetim raporu;
// sunucu uçları sunmuyorsa (W5-K45) hata kodu ve davranış. Gövdeler şemalara karşı doğrulanır.

import assert from 'node:assert/strict';
import { after, test } from 'node:test';
import { A3Hatasi, A3Http } from '../src/a3/http';
import { ucYok } from '../src/a3/hata';
import { bulguKonumu, davranisOzeti, satirOnemleri } from '../src/paneller/senaryo-ozet';
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

test('senaryo akışı: listele → oku → doğrula → yükle → başlat; ön denetim raporu', async () => {
  const k = await yeni();
  const http = k.baglanti.http;

  const liste = await http.senaryolar();
  semayaUy('senaryo_listesi', liste, 'GET /senaryolar');
  assert.deepEqual(liste.senaryolar.map((s) => s.ad), ['z1.toml', 'dalis-yuzey.toml', 'hatali.toml']);

  const dosya = await http.senaryoDosyasi('hatali.toml');
  semayaUy('yanit_senaryo_dosyasi', dosya, 'GET /senaryolar/{ad}');
  assert.match(dosya.icerik, /kutle = -3\.0/);

  // Geçersiz dosya: rapor yol ve satırla hatayı gösterir; durum değişmez, komut numarası yok
  const kotu = await http.senaryoDogrula({ ad: 'hatali.toml' });
  semayaUy('yanit_senaryo_dogrula', kotu, 'POST /senaryo/dogrula');
  assert.equal(kotu.gecerli, false);
  const bulgu = kotu.rapor.bulgular[0]!;
  assert.equal(bulgu.onem, 'hata');
  assert.equal(bulgu.yol, 'arac.kutle');
  assert.equal(dosya.icerik.split('\n')[bulgu.satir! - 1]!.trim(), 'kutle = -3.0', 'bulgu satırı dosyadaki satırdır');
  assert.match(bulguKonumu(bulgu), /hatali\.toml · arac\.kutle · satır \d+/);
  assert.equal(satirOnemleri(kotu.rapor.bulgular).get(bulgu.satir!), 'hata');
  assert.equal((await k.durum()).durum, 'yuklendi');

  // Uyarılı ama geçerli dosya
  const uyarili = await http.senaryoDogrula({ ad: 'dalis-yuzey.toml' });
  assert.equal(uyarili.gecerli, true);
  assert.deepEqual(uyarili.rapor.bulgular.map((b) => b.onem), ['uyari']);
  assert.match(uyarili.senaryo_ozeti ?? '', /^sha256:[0-9a-f]{64}$/);

  // Geçersiz senaryo yüklenmez: 422 senaryo_gecersiz + A5 raporu ayrıntıda
  const red = await k.komutlar.senaryoYukle({ ad: 'hatali.toml' });
  assert.equal(red.kayit.asama, 'reddedildi');
  assert.equal(red.kayit.hata_kodu, 'senaryo_gecersiz');
  assert.ok(red.hata instanceof A3Hatasi);
  assert.equal((red.hata!.ayrinti!.rapor as { gecerli: boolean }).gecerli, false);
  const ret = k.istekler.filter((i) => i.yol === '/senaryo/yukle').at(-1)!;
  assert.equal(ret.http, 422);
  semayaUy('yanit_ret', ret.yanit, 'senaryo_gecersiz');

  // Geçerli senaryo yüklenir
  await k.senkron();
  const ok = await k.komutlar.senaryoYukle({ ad: 'z1.toml' });
  assert.equal(ok.kayit.asama, 'uygulandi', `${ok.kayit.ileti} (${ok.kayit.hata_kodu})`);
  semayaUy('yanit_senaryo_yukle', ok.yanit, 'POST /senaryo/yukle');
  const yukle = k.istekler.filter((i) => i.yol === '/senaryo/yukle').at(-1)!;
  semayaUy('istek_senaryo_yukle', yukle.govde, 'yükle isteği');
  assert.match(yukle.baslik['ida-istemci'] ?? '', /^\d+$/);
  assert.equal((await k.durum()).durum, 'yuklendi');

  // Yüklü çözümlenmiş senaryo ve davranış özeti başlığı
  const { senaryo, ozet } = await http.cozumlenmisSenaryoOzetli();
  assert.equal(ozet, ok.yanit!.senaryo_ozeti);
  const satirlar = davranisOzeti(senaryo);
  assert.equal(satirlar.find((s) => s.etiket === 'Ad')!.deger, 'z1');
  assert.match(satirlar.find((s) => s.etiket === 'Görev ögeleri')!.deger, /DIVE_TO ×1, WAYPOINT ×\d+, SURFACE ×1/);

  // Başlat ve ön denetim raporu (komut, ekranın gösterdiği revizyonla gider)
  await k.senkron();
  assert.equal((await k.komutlar.gonder('baslat')).asama, 'uygulandi');
  await k.durumBekle('kosuyor');
  const od = await http.onDenetim();
  semayaUy('on_denetim_raporu', od, 'GET /on_denetim');
  assert.equal(od.sonuc, 'gecti');
  assert.ok(od.maddeler.length >= 5);
  for (const m of od.maddeler) assert.ok(['gecti', 'kaldi', 'uyari', 'atlandi'].includes(m.durum));
  // Koşarken senaryo yükleme sunucuda reddedilir (gecersiz_durum), ekran yüklemeyi göstermez
  await k.senkron();
  const koserken = await k.komutlar.senaryoYukle({ ad: 'z1.toml' });
  assert.equal(koserken.kayit.hata_kodu, 'gecersiz_durum');
});

test('doğrulama komut sayılmaz: komut numarası, istemci başlığı ve durum değişimi yok (A3-K76)', async () => {
  const k = await yeni();
  const once = await k.durum();
  await k.baglanti.http.senaryoDogrula({ icerik: 'kutle = -1\n' });
  const sonra = await k.durum();
  assert.equal(sonra.durum_rev, once.durum_rev);
  const d = k.istekler.find((i) => i.yol === '/senaryo/dogrula')!;
  assert.equal(d.baslik['ida-istemci'], undefined);
});

test('W5-K45: sunucu senaryo uçlarını sunmuyorsa istemci bunu "uç yok" olarak ayırt eder ve çökmez', async () => {
  const k = await yeni();
  // Gerçek sim bugün bu uçları sunmuyor: 404 bulunamadi (kimlik bilgisi doğru)
  const sahte404: typeof fetch = async () =>
    new Response(JSON.stringify({ sonuc: 'ret', komut_no: null, hata: { kod: 'bulunamadi', ileti: 'uç yok' } }), { status: 404, headers: { 'Content-Type': 'application/json' } });
  const http = new A3Http(`http://127.0.0.1:${k.sunucu.port}`, JETON, sahte404);
  for (const cagri of [() => http.senaryolar(), () => http.senaryoDosyasi('z1.toml'), () => http.senaryoDogrula({ ad: 'z1.toml' }), () => http.cozumlenmisSenaryoOzetli(), () => http.onDenetim()]) {
    await assert.rejects(cagri(), (h: unknown) => h instanceof A3Hatasi && ucYok(h));
  }
  // Çalıştırma denetimi senaryo uçlarına bağlı değildir: senaryo "komut satırından yüklü" (sahte sunucu yuklendi'de başlar)
  assert.equal((await k.komutlar.gonder('baslat')).asama, 'uygulandi');
  await kosul(() => k.kanalDurumu() === 'kosuyor', 8000, 'koşuyor');
});

test('sahte sunucu: dosya adı kalıbı (A3-K78) ve olmayan dosya', async () => {
  const k = await yeni();
  await assert.rejects(k.baglanti.http.senaryoDosyasi('../sifre.toml'), (h: A3Hatasi) => h.kod === 'gecersiz_istek' || h.kod === 'bulunamadi');
  await assert.rejects(k.baglanti.http.senaryoDosyasi('yok.toml'), (h: A3Hatasi) => h.kod === 'bulunamadi');
});
