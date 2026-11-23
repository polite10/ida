# W4-sim-senaryo — `ida_sim`'in senaryo dosyasından kurulması ve zaman çizelgesi

Tarih: 2026-11-23. Yazan: `ida-uygulayici` (Opus; `sim/sunucu`'nun sahibi). İnceleyen: sol (tek tur; emniyet girdilerinin uygulanmasına dokunan bulgularda ikinci tur). Dal: `is/w4-sim-senaryo` (taban: `is/w4-kopru`'nun birleşik ucu; PR #24–#27 `main`'e girdiyse `origin/main`). Ortak kurallar: `docs/gorevler/W3-ortak.md`, `docs/gorevler/W4-ortak.md`.

## Amaç

`ida_sim` bugün fizik, sensör ve ortamı koddaki İDA-0 varsayılanlarıyla, senaryoyu da koddaki `z1` adımlarıyla kuruyor. Bu iş onu A5 dosyalarına bağlar: `ida_sim --senaryo scenarios/<ad>.toml` çözümleyiciden (`ida_senaryo`) geçen dosyayla kurulur ve dosyadaki zaman çizelgesini yürütür. Kartsız ve kartlı kiplerde aynı senaryo dosyası kullanılır.

Okunacaklar: `docs/sozlesmeler/A5-senaryo.md` §3.7–§3.11, §3.13, §4.6–§4.8, §4.10–§4.11, §4.13 (özellikle A5-K56–K73: görev ve zaman çizelgesi; K62–K65, K71: emniyet atamaları; K82–K85: rastgelelik; K86–K88: kütle durumu ve varsayılan emniyet durumu); iskelet §3.2 (fizik sınırındaki sıra), §4; A1-K85 (emniyet işlemlerinin sırası) ve W4-K3, K4, K5, K12, K15; `sim/senaryo/include/ida/senaryo/**` (`senaryo_coz`, `DogrulanmisSenaryo`, `fizik_kur`, `sensor_kur`); `sim/sunucu/**` ve `sim/kopru/include/**`; `scenarios/ida0.toml`, `scenarios/ornek-havuz.toml`.

## Kapsam

1. **Kurulum:** `--senaryo <dosya>` verilince çözümleyici çalışır; geçersizse doğrulama raporu standart hataya yazılır ve program belgeli bir çıkış koduyla çıkar (hiçbir oturum açılmaz). Fizik `fizik_kur`, sensörler `sensor_kur` ile kurulur (yalnız `DogrulanmisSenaryo` kabul eden arayüzle). `--senaryo` verilmezse bugünkü yerleşik kurulum kalır (Z1/Z2 gerilemesi için). Davranış özeti ve kaynak özetleri CSV başlığına/özet satırına yazılır.
2. **Rastgelelik ve tohum (A5 §3.8, Ek D):** tohum senaryodan; komut satırı tohumu varsa hangisinin geçerli olduğu sözleşmeye göre (belirsizse raporla).
3. **Başlangıç (A5 §3.13) ve kütle durumu (K86):** başlangıç pozu, hızlar, tüplerin dolu/boş durumu; varsayılan emniyet durumu (K87–K88: acil durdurma bırakılmış, fişler takılı değil, pimler takılı).
4. **Zaman çizelgesi (A5 §3.10, K60–K73), bu dilimde desteklenen olay sınıfları:** emniyet girdisi atamaları (acil durdurma, fiş, pim; EMNİYET), operatör komutları (`ARM`, `DISARM`, `SET_MODE`, `GUIDED_SP`), sensör arızaları (sensör kümesinin gövdeli arızaları: ofset, ölçek, gürültü artışı, donma, kayıp, geç teslim, geçersiz bayrak), itici arızası (bugünkü Z1 arızası). Başlangıç ve bitişler fizik adımı sınırına A5'teki kuralla oturur; aynı sınırda bitişler başlangıçlardan önce; emniyet atamaları A1-K85'in sırasına köprü üzerinden girer (kısıtlayan önce; gevşeten revizyon kuralıyla; reddedilen gevşetme `gevsetme_reddedildi` olarak kaydedilir). Desteklenmeyen olay sınıfı çözümleyicide zaten reddedilir (K102); çözümleyicinin kabul edip bu dilimin yürütemediği sınıf kalırsa program başlamadan açık hatayla çıkar (sessizce atlanmaz).
5. **Görev öğeleri (A5-K55–K57):** senaryodaki görev, kartın görev yürütücüsüne A1 üzerinden aynı sırayla ve aynı alan değerleriyle gönderilir; köprüde görev yükleme mesajları yoksa bu madde "yazılamadı: köprüde eksik" diye raporlanır ve görevli senaryo açık hatayla reddedilir (sessiz atlama yok).
6. **`z1` senaryosunun dosya karşılığı:** `scenarios/z1.toml` (yerleşik Z1 ile aynı adımlar). **Kabul ölçütü: aynı tohumla `--senaryo scenarios/z1.toml` koşusu yerleşik `z1` koşusuyla bayt bayt aynı CSV'yi verir**; vermiyorsa fark alan alan açıklanır (ör. dosyada GNSS ve kamera kapalı, kodda açık: bu durumda dosya mı kod mu doğru sorusu mimara raporlanır, sessizce eşitlenmez).
7. **Ses hızı (W4-K1):** `ornek-havuz` + etkin DVL şu an `sensor_kur`'da reddedilir (formül W4-sonar'da yazılıyor); `z1.toml` ses hızını açıkça verir. W4-sonar `main`'e girince bu kısıt kalkacak; şimdi dolanma.

## Kapsam dışı

Bağlantı emülatörü profilleri (gecikme/kayıp), kalite kademelerinin uygulanması, hedefler ve hedef hareketi, torpido ve fırlatma fiziği, sonar ve kamera, A3'ten senaryo yükleme, kayıt (A6). `sim/senaryo`, `sim/sensor`, `sim/fizik`, `core/`, `protocol/` değiştirilmez (eksik görürsen en küçük değişikliği dosya:satır ile öner).

## Dosya sahipliği

Yazılır: `sim/sunucu/**`, `sim/kopru/**` (yalnız zaman çizelgesi için gereken arayüz eklemeleri), `tests/butunleme/**`, `scenarios/z1.toml` (yeni dosya; var olan `scenarios/` dosyaları değiştirilmez).

## Kabul

1. Madde 6'daki bayt eşitliği (SIL, süreç içi ve `ida_sil` ile).
2. Geçersiz senaryo: oturum açılmaz, rapor ve çıkış kodu; doğrulanmamış bir ağacın kuruluma ulaşamadığı (derleme zamanı kısıtı korunur).
3. Zaman çizelgesi: her desteklenen sınıf için adım sınırı testi (başlangıç/bitiş tam beklenen fizik adımında); aynı sınırda bitiş-başlangıç sırası.
4. **Emniyet (EMNİYET):** senaryodan acil durdurma bas/bırak: itki sıfır ve W4-K3 (eski komut dönmez); senaryo "pim çıkar" + aynı sınırda kısıtlayan atama → gevşetme reddi ve kaydı (A5-K62–K63'ün yükleme anında reddedilmeyen, yürütmede karşılaşılan biçimleri); emniyet örneğinin karta W4-K12'ye uygun gitmesi (bağımsız kabul testleri `kabul.kopru.*` değişmeden geçer).
5. Belirlenimcilik: aynı senaryo ve tohumla iki koşu aynı CSV; farklı tohum farklı CSV; A5-K53 (otopilot bloğunu değiştirmek fizik izini değiştirmez).
6. Z1 yerleşik koşusu ve bütün var olan testler değişmeden geçer; 60 s kartsız HIL koşusunda zamanlama bozulmaz (kaçırma 0).
7. `host-debug` + `host-release`; W3-K14 taraması; MSVC kalıpları.
