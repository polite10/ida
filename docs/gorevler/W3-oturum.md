# W3-oturum — `ida_link` üst katmanı: oturum, parti ve A1 ↔ A4 dönüştürücü

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). Bağımsız kabul testleri: `ida-test-yazari` (emniyete dokunduğu için). İnceleyen: sol. Dal: `is/w3-oturum`. Ortak kurallar: `docs/gorevler/W3-ortak.md`.

## Amaç

`protocol/ida_link/` altında, tel katmanının (W2: çerçeveleme, kodlama) üstüne, kartın ve kartsız koşucunun **aynı kaynaktan** bağladığı protokol modülünü yazmak: A1 bayt akışını çekirdeğin A4 çağrı girdilerine, çekirdeğin çıktısını A1 çerçevelerine çevirir. Z2'nin (kartta çalışma) ve W4'teki kartsız koşucunun ön koşuludur.

Okunacaklar: A1 sözleşmesinin bütünü (özellikle §3 mesajlar, §4 davranış kuralları: el sıkışma ve oturum, zaman çerçevesi ve parti kapanışı, komutlar ve ertelenmiş komut kuyruğu, olaylar ve onayları, sayaçlar; §5 hata durumları; Ek A alan eşlemesi); A4 sözleşmesinin platform kuralları (çekirdeğin değil platformun uyguladıkları; `core/TASARIM.md` §8 tablosunda "platformun" diye işaretli kurallar); `core/include/ida/port.h`; `protocol/ida_link/BENIOKU.md` ve tel katmanı başlıkları; iskelet §3.2, §4; `docs/gorevler/W1-uyum.md` D-17…D-24; `docs/mimari/06-protokol-ve-hil.md` §3 ve `01-kart-yazilimi.md` §3.2–§3.3 (fikir kaynağı).

## Kapsam

1. **Alım:** bayt bloğu ve alım damgası alır (`rx`), tel katmanıyla çerçeveleri çözer, mesajları oturum durumuna göre işler; alım sayaçları (A1-K07 ve çevresi).
2. **Oturum:** el sıkışma, sürüm ve `dialect_hash` denetimi, oturum kimliği, kopma ve yeniden kurulum; oturum değişiminde temizlik (A1 §4).
3. **Parti:** zaman çerçevesiyle açılan partinin üyelerini toplar, kapanış kuralıyla tek bir çekirdek çağrısının girdisine çevirir (`take`: bu çağrı step mi tick mi, girdi yapıları dolu); yinelenen, eski ve eksik parti durumları; üye taşması.
4. **Komutlar:** daraltıcı komutun mandala alınması ve o ana kadar alınmış, çağrıya girmemiş bütün genişleticilerin atılması (D-17; A1-K63, A1-K66; son 32 kimliğin tutulması; atılan kimliğin nihailiği); ertelenmiş komut kuyruğu; platformun komut sonuçları.
5. **Çıktı:** çekirdek çıktısından aktüatör, durum, telemetri ve olay çerçeveleri (`put_out`); olayların onaylanana kadar aynı kimlikle yinelenmesi (`poll_tx`); kritik ve olağan gönderim sınıfı ayrımı (sınıfı bu modül etiketler, kuyruğu platform tutar).
6. **Arayüz:** platformdan bağımsız C17; bellek ayırmaz; saat, kesme ve seri port bilmez. Platform işlemleri bir işlev tablosuyla verilir (bayt gönder, zaman oku). Kart ve PC aynı kaynağı derler.
7. A1 Ek A'daki eşleme tek yerde, tablo ya da tek dosya olarak durur.

Emniyet ilkesi: kısıtlayan her şey genişletenden önce; kuşkuda genişletici atılır ve reddedilir. Belirsizlikte kısıtlayan okuma seçilir ve `EMNİYET` diye raporlanır. Atış kontrol komutları bu modülde yalnız taşınır ve sıralanır; içerikleri yorumlanmaz.

## Kapsam dışı

Tel katmanının içi (`src/tel.c` ve başlıkları; `W3-tel-hiz` işi aynı anda değiştiriyor, dokunma); kartın seri port sürücüsü ve gönderim kuyruğu; PC tarafındaki köprü, bariyer ve bağlantı emülatörü (W4); çekirdek.

## Dosya sahipliği

Yazılır: `protocol/ida_link/src/` ve `include/` altında **yeni** dosyalar (oturum, parti, komut, çıktı, eşleme), `protocol/ida_link/test/oturum/**`. `protocol/ida_link/CMakeLists.txt`'e yalnız satır eklenir. Bağımsız kabul testleri: `tests/kabul/oturum/**` (test yazarı).

## Kabul

1. **Senaryo testleri** (bayt dizileri altın çerçevelerden ve tel katmanının kodlayıcısından üretilir): temiz el sıkışma; yanlış sürüm ve yanlış `dialect_hash`; tam parti → doğru çağrı girdisi; yinelenen, eski, eksik parti; üye taşması; kopma ve yeniden kurulum.
2. **Komut sırası:** genişletici → daraltıcı sırasıyla gelen komutlarda genişletici çekirdeğe hiç verilmez ve doğru sonuçla reddedilir; atılan kimliğin yinelemesi yeniden işlenmez; daraltıcı her koşulda çekirdeğe ulaşır.
3. **Eşleme:** A1 Ek A'nın her satırı için alan değerinin iki yönde korunduğu bir test.
4. **Çıktı:** olay onaylanana kadar aynı kimlikle yinelenir, onaydan sonra durur; oturum sonunda bekleyenler silinir.
5. **Dayanıklılık:** rastgele bozulmuş bayt akışında (sabit tohum) modül çökmez, bellek dışına yazmaz (`host-asan`), geçersiz girdiden çekirdek çağrısı üretmez.
6. Çekirdekle uçtan uca: modül + gerçek çekirdek iskeleti birlikte derlenir; bir oturum boyunca üretilen çağrı dizisi çekirdekçe `OK` ile kabul edilir.
7. `fw-debug` ve `fw-release` derlenir; kod boyu ve durum yapısının boyu raporlanır.

Bağımsız kabul testleri yalnız A1, A4, `port.h` ve bu belgeden yazılır; kapsamı kabul md.2 ve md.4 ile oturum değişimindeki temizlik.

İş büyüktür; sıra: alım ve oturum → parti → komutlar → çıktı. Her adım kendi testleriyle biter; iş yarıda kalırsa biten adımlar raporlanır.
