# W3-viz — 3B görselleştirici (dilim 1: Godot sahnesi, A2 istemcisi, sahte A2 kaynağı)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w3-viz`. Ortak kurallar: `docs/gorevler/W3-ortak.md`.

## Amaç

`viz/` altında operatörün 3B penceresini başlatmak: araç su yüzeyinde ve su altında görünür, durumunu A2 sözleşmesinin tel biçimiyle alır. Gerçek A2 sunucusu (sim sunucusunun içinde) henüz yok; bu iş görselleştiriciyi A2'yi konuşan **sahte bir kaynağa** karşı geliştirir, böylece gerçek sunucu geldiğinde görselleştirici değişmeden bağlanır. Kullanıcı sistemi ilk kez 3B görecek; görünüş önemlidir.

Okunacaklar: A2 sözleşmesi §3.1–§3.4 (başlık, ileti türleri, el sıkışma, durum akışı), §4.1–§4.4, §4.7, §6; `sim/include/ida/a2_wire.h` (dokunulmaz); A7 sözleşmesi §3–§4 ve `assets/` (manifest şeması, örnek); iskelet §3.1 (çerçeveler), §5 (İDA-0), §6 (kalite kademeleri); `docs/mimari/07-gorsellestirme.md` §2.1–§2.4, §2.7, §2.10; `docs/olcumler/MP6-godot-kamera.md` ve `tools/olcum/mp6` (Godot ikilisinin bu Mac'te nasıl bulunduğu ve başsız nasıl çalıştırıldığı oradadır); 09 §2.6 (Godot 4.7.2, godot-cpp 10.0.0; sürümler sabit).

## Kapsam (dilim 1)

1. **Godot projesi (`viz/proje/`):** 07 §2.2'deki düğüm ağacı: `Dünya` (NED → Godot dönüşümü burada), `Gövde[id]`, `Su`, `Işıklar`, `Operatör`, `Göstergeler`. Operatör kamerası: serbest ve araç takip kipleri.
2. **Su (sade ve tam kademe):** sade = düz yüzey, basit ışık; tam = dalga yüzeyi (A2'den gelen dalga bileşenleriyle; bu dilimde sahte kaynağın verdiği birkaç bileşen), su altında renk soğurması ve sis. Kademe A2 el sıkışmasındaki yapılandırmadan seçilir. Sade kademe dahili ekran kartında akıcı çalışacak kadar hafif tutulur.
3. **Araç ve hedef modeli:** İDA-0 için BlueROV2 Heavy sınıfı açık çerçeveli gövde (çerçeve, yüzdürücüler, elektronik tüpü ve kubbe, sekiz itici, iki torpido tüpü, sonar başı) ve bir sabit hedef. Modeller basit geometriden bir betikle **üretilir** (glTF; A7 manifestine uygun, `REF` düğümüyle); ikili varlık elle çizilmez, hazır model indirilmez. Üretim betiği depoda, üretilen dosyalar A7'nin kuralına göre depoda ya da derleme çıktısında (A7 hangisini diyorsa).
4. **A2 istemcisi (`viz/eklenti/`, C++ GDExtension):** yerel soket, el sıkışma (sürüm ve manifest özeti), durum akışının çözümü (`a2_wire.h` yapılarıyla), pozların düğümlere uygulanması, iki durum arasında ara değerleme, sahne olayları (gövde doğumu ve ölümü), kopma ve yeniden bağlanma. Dönüşümler 07 §2.3'teki matrislerle; kuaterniyon sırası yalnız eklentide çevrilir.
5. **Sahte A2 kaynağı (`viz/arac/sahte-a2/`, C++):** A2 tel biçimiyle yazılı bir senaryoyu yayınlar: araç yüzeyde yarı batık bekler, dalar, bir tur atar, hedefe yaklaşır. Kinematiktir (fizik değildir; pencerede "sahte veri" etiketi görünür). Yalnız geliştirme aracıdır.
6. **Göstergeler:** sim zamanı, derinlik, kip etiketi; gövde eksenleri ve iz çizgisi açılıp kapanabilir (ayrı görünürlük katmanı).
7. **Çalıştırma:** tek komutla sahte kaynak + Godot penceresi (`viz/calistir.sh`; Windows için eşdeğeri sonraki dilim). Godot ikilisinin yeri ortam değişkeninden; kişisel yol yazılmaz.

## Kapsam dışı

Kamera sensörü hattı ve kare halkası (paylaşımlı bellek); kamera algılama bileşeni; su altı optiğinin sensör doğruluğundaki hâli; torpido ve fırlatma görseli; Windows derlemesi ve ölçümü (kullanıcı makinesinde, sonra); sim sunucusunun A2 tarafı.

## Dosya sahipliği

Yazılır: `viz/**`, `assets/` altında yalnız bu işin ürettiği model ve manifest girdileri. godot-cpp alt modül ya da sabit commit'li getirme olarak eklenir (09 §2.6); `sim/include/ida/a2_wire.h`'ye dokunulmaz. Kök CMake'e yalnız satır eklenir; GDExtension derlemesi varsayılan `host-*` derlemesini yavaşlatmayacak biçimde seçenekle (`IDA_VIZ=ON`) açılır ve CI'da varsayılan kapalıdır.

## Kabul

1. Eklenti bu Mac'te derlenir; proje Godot 4.7.2 ile açılır; `viz/calistir.sh` pencereyi açar ve araç senaryoyu oynatır.
2. A2 çözümü: `a2_wire.h` yapılarından kurulan bayt dizileriyle birim testi (el sıkışma, durum iletisi, bozuk ve kısa ileti reddi); sahte kaynağın ürettiği her ileti istemciden geçer.
3. Çerçeve dönüşümü testi: bilinen NED pozları (kuzeye bakış, 90° sancağa dönüş, 3 m derinlik, 10° baş aşağı) Godot'da beklenen konum ve yönelimi verir (sayısal karşılaştırma).
4. Sahte kaynak durdurulup yeniden başlatılınca görselleştirici çökmeden yeniden bağlanır.
5. Ekran görüntüleri (başsız olmayan pencereden ya da Godot'nun kendi görüntü kaydıyla): yüzeyde, dalışta ve hedefe yakın, sade ve tam kademede; yolları raporda (depoya girmez).
6. Ölçüm (bu Mac; bilgi): sade ve tam kademede kare süresi. Dahili ekran kartlı Windows hedefi için bu sayı geçerli değildir; öyle yazılır.

A2'de uygulanamayan ya da iki biçimde okunan bir yer bulursan makul okumayı uygula ve "Okumalar"a yaz; gerçek sunucu aynı okumayla yazılacak.
