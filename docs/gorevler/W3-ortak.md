# W3 — ortak kurallar ve hedef

Tarih: 2026-11-21. Yazan: mimar. Bu belge W3'ün bütün işlerinde geçerlidir; her işin kendi belgesi (`W3-<ad>.md`) yalnız kapsamı, dosya sahipliğini ve kabulü ekler. İş belgesi ile bu belge çelişirse iş belgesi geçerlidir.

## Hedef: ilk çalışan zincir

W3'ün işleri birbirini beklemeden paralel yürür (K20) ve tek bir hedefe hizmet eder:

- **Z1 (kartsız):** simüle edilen İDA-0, bilgisayarda koşan çekirdekle silahsız hâlden kalkar, 3 m'ye dalar, derinliği ve yönü tutar; bir itici zayıflatıldığında toparlar.
- **Z2 (kartlı):** aynı senaryo, çekirdek STM32U5A9J-DK üzerinde koşarken, tek kablo üzerinden (ST-LINK sanal seri portu, 3 000 000 baud, 200 Hz) çalışır.

Her iş, kapsamındaki özellikleri bu iki hedefe en erken katkı verecek sırayla yazar; iş belgesindeki "dilim 1" bu sıranın ilk parçasıdır. Dilim 1 bitince rapor verilir; sonraki dilim mimarın yanıtıyla başlar.

## Sözleşmeler ve belirsizlik (K20)

- A1–A8 sözleşmeleri 1.0 olarak dondurulmuştur (`docs/sozlesmeler/`). Sözleşme metnine, `protocol/ida.xml`, `protocol/ayna.json`, `protocol/komutlar.json`, `core/include/ida/port.h`, `core/include/ida/algilama_kaydi.h` ve `sim/include/ida/a2_wire.h` dosyalarına dokunulmaz.
- Sözleşme iki biçimde okunabiliyorsa **makul okumayı seç, uygula ve raporun "Okumalar" başlığına yaz** (kural numarası, seçilen okuma, tek cümle gerekçe). Mimara sorup bekleme.
- İstisna, emniyet: araç silahlandırması (arm), atış izni, acil durdurma, emniyet daraltması ve kısıtlayan/genişleten komut sırasıyla ilgili bir belirsizlikte **kısıtlayan (daha güvenli) okumayı** uygula ve okumayı `EMNİYET` diye işaretle.
- Sözleşmenin yanlış ya da uygulanamaz olduğunu düşündüğün yerde sözleşmeye uy; uyamıyorsan o parçayı yazma ve "Sözleşme çelişkileri" başlığında raporla. Sözleşmede olmayan mesaj, alan, ret nedeni ya da durum uydurulmaz.
- Mimari bölümleri (`docs/mimari/01–09`) taslaktır ve fikir kaynağıdır; iskelet (`00-iskelet.md`, 0.12) ve sözleşmelerle çeliştikleri yerde geçersizdir.

## Mimar kararları (W3 sırasında; 2026-11-21)

İş belgelerindeki "emniyet kısıtı" ifadesi iki bağımsız rapora göre iki biçimde okundu; aşağıdaki kararlar iş belgelerinin önüne geçer.

- **W3-K1 Acil durdurma ve araç.** Çekirdekte sanal emniyet girdilerinin (acil durdurma, fiş, pim) etkin değeri yalnız atış kontrol izinlerini kapatır (A4-K116: "acil durdurmada iki tüp"; engel biti `ESTOP`). Araç silahlandırmasının (`ARM`) ön koşulu değildir ve itki kapısında yer almaz: A4 §3.17'nin `ARM` satırı kapalı listedir. İticilerin acil durdurmada kesilmesi sim sunucusundaki bağımsız engeldir (iskelet §5). Emniyet gevşetmesi W4'te kalır; Z1 ve Z2 ona bağlı değildir.
- **W3-K2 "Emniyet kısıtı" tanımı.** W3 belgelerinde bu ifade, çağrıda uygulanan daraltmayı anlatır (A4-K44'ün daraltıcı girdileri: `DISARM`, `SESSION_END`, daraltıcı kip isteği, arıza eyleminin daraltması). Daraltma uygulanan çağrıda genişletici etkisizdir; sanal emniyet girdileri bu tanıma girmez.
- **W3-K3 `ARM` ön koşulları.** Yalnız A4 §3.17'deki satır: olay kimliği kullanılabilir, parametreler geçerli ve kayıt meşgul değil, kestirim düzeyi yeterli, arıza eylemi yok, arm reddi gerektiren arıza kaydı yok. Kestirim düzeyi `OK` ya da `DEGRADED` yeterlidir (havuzda GNSS yoktur; konumu gerektiren kipler kendi denetimini `SET_MODE`'da yapar); `INIT`, `BAD`, `FAULT`'ta ret. Bağlantı durumu ayrı bir ön koşul değildir; bağlantı kaybı bir arıza eylemi üretiyorsa ret o yoldan gelir. Yalnız uyarı düzeyindeki (`WARN`) bir arıza, kaydı "arm reddi gerektirir" diye tanımlı değilse arm'ı engellemez; sızıntı ve düşük batarya arm'ı engeller.
- **W3-K4 İtki kapısı.** İtki isteği şu durumlarda sıfırdır: silahsız; kestirim `INIT`, `BAD` ya da `FAULT`; arıza eylemi itkisiz düzeyde (edilgen yüzeye çıkış ve üstü); geçersiz ya da kapalı istek; sayısal çökme. Acil durdurma bu listede değildir (W3-K1).
- **W3-K5 Kip ve kestirim bileşenleri.** Kiplerin ön koşulu ve kip × arıza matrisi, kestirimin genel düzeyine değil kipin gerektirdiği bileşenlerin geçerliliğine bakar (`ida_kes_gecerlilik`: tutum, yön, derinlik, dikey hız; sonraki dilimde yatay hız ve konum). Derinlik ve yön tutma kipleri tutum, yön ve derinlik geçerliyken sürer; kestirim düzeyinin `DEGRADED` olması (havuzda GNSS yok, yüzeyde DVL kilidi yok) bu kipleri tek başına sonlandırmaz ve yüzeye çıkış eylemi başlatmaz. Konum hedefli kipler (`GUIDED`, `AUTO`) yatay geçerlilik ister. 02 §4.3'teki "EST_DEGRADED → 30 s sonra yüzeye çık" satırı yalnız konum gerektiren kiplere uygulanır.
- **W3-K6 Parametre sayısı testi.** `core/test/test_param.c` toplam parametre sayısını sabitlemez (W3-kontrol işinde düzeltildi); parametre ekleyen işler bu testi kendi dallarında kırık görür, birleştirmede düzelir.
- **Açık nokta (karar değil):** acil durdurma etkinken sim iticileri keser ama çekirdek itki istemeyi sürdürür; denetleyicinin bu sürede birikmesi Z1'de gözlenirse A4'ün küçük sürüm artışında ele alınır.

## Dosya sahipliği

- Yalnız iş belgesinde sana verilen dizinlere yaz. Başka işin dizinine dokunma; gereksinimini raporla.
- Paylaşılan dosyalarda (kök `CMakeLists.txt`, `tests/CMakeLists.txt`, `vcpkg.json`, `core/CMakeLists.txt`, `core/test/CMakeLists.txt`) yalnız **satır ekle**, var olan satırı değiştirme; eklediğin satırları raporda listele. Aynı anda başka işler de satır ekliyor; birleştirmeyi kayıt agent'ı yapar.
- Kendi çalışma ağacında çalış; commit atma, dal gönderme, PR açma (kayıt agent'ı yapar).

## Dil ve derleme

- Çekirdek ve `protocol/ida_link`: ISO C17, uzantı yok, bellek ayırma ve özyineleme yok, değişebilir genel durum yok, `float`; aynı kaynak `host-*` ve `fw-*` ön ayarlarında derlenir.
- PC tarafı: C++20, `CXX_EXTENSIONS OFF`; fizik `double`.
- Uyarı hatadır. Kod üç derleyicide derlenmelidir: Apple Clang, MSVC (`/W4 /WX /analyze`), arm-none-eabi-gcc. MSVC'de önceki işleri düşüren uyarılar: C4702 (ulaşılamayan kod), C4310 ve C4244 (daraltan dönüşüm; açık dönüştür), C4204/C4221 (sabit olmayan toplu başlatıcı), **C4456/C4457 (iç kapsamda aynı adlı değişken)**, C6011 (NULL denetimsiz erişim), C4100 (kullanılmayan parametre). Yerelde `-Wshadow -Wconversion -Wdouble-promotion` ile derle; Windows sonucu CI'dan gelir.
- Rust ve Rust tabanlı araç kullanılmaz. Yeni bağımlılık ancak iş belgesi izin veriyorsa, açık sürümle sabitlenerek eklenir.

## Test ve doğrulama

- Birim testlerini uygulayıcı yazar (C: Unity; C++: depodaki düzen; CTest). `host-debug` ve `host-asan` geçer; çekirdek ve protokol işlerinde `fw-debug` ve `fw-release` derlenir.
- Bağımsız kabul testi (`ida-test-yazari`) yalnız emniyete dokunan işlerde yazılır; iş belgesi belirtir.
- Bir iş tek sol incelemesi ve tek düzeltme turu görür. İkinci tur yalnız emniyete dokunan bulgularda açılır.
- Ölçülmemiş süre, yük ya da doğruluk rakamı gerçek gibi yazılmaz; ölçtüysen makineyi ve komutu, tahminse "tahmin" olduğunu yaz.

## Yasaklar

Başka agent başlatma; commit; kişisel makine yolu; ardıl kart, başka mikrodenetleyici ya da gerçek sensör önerisi; yarışma kuralı; ikinci kablo ya da USB HS yolu. Atış kontrol içeriği emniyet zinciri ve arayüzlerle sınırlıdır: hedefleme çözümü, önleme hesabı, atış ölçütü yazılmaz; torpido güçlendirilmez. Ortamın reddettiği bir komutu başka yoldan çalıştırma; raporla.

## Kart

Kart tektir ve yalnız `W3-kart` işi kullanır. Seri uç ilk `/dev/cu.usbmodem*` aygıtıdır (kart 2026-11-21'de başka USB portuna takıldı; ad değişti, betikler ucu kendisi bulur).

## Rapor (kısa; başlıklar sabit)

1. **Yazılanlar:** dosyalar ve tek satırlık içerik.
2. **Doğrulama:** çalıştırılan komutlar ve sonuç özeti (geçen/kalan test sayısı, derlenen ön ayarlar).
3. **Doğrulanmayanlar:** koşulmayan test, denenmeyen derleyici, tahmin rakamlar.
4. **Okumalar:** seçilen sözleşme okumaları (`EMNİYET` işaretlileri ayrı).
5. **Sözleşme çelişkileri:** varsa.
6. **Paylaşılan dosyalara eklenen satırlar.**
7. **Sonraki dilim için öneri:** en çok beş madde.
