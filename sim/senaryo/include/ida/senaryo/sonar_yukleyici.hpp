// İDA senaryo kitaplığı (W4-bagla-sonar, W4-sonar-sahne): çözümlenmiş senaryodan sonar kümesinin kurulumu
// (`ida::sonar`, W4-sonar).
//
// Bu başlık ve kaynağı yalnız `sim/sonar` depoda varken derlenir (sim/senaryo/CMakeLists.txt;
// hedef `ida_senaryo_sonar`). `sensor_kur` ile aynı kural: girdi yalnız `DogrulanmisSenaryo`dur
// (doğrulanmamış ağaç derleme zamanında verilemez). Alan adları A5 §3.6, §3.9, §3.11, §3.12 ile
// bire bir aynıdır.
//
// Sahne (05 §2.4.1; senaryoda karşılığı olmayan öğe uydurulmaz; W4-sonar-sahne): `sonar::AgSahnesi`.
//  - Su yüzeyi z = 0 (sonar modelinin yüzey malzemesi; A7'de su yüzeyi malzemesi yok), gövde 1.
//  - `ortam.sahne`'nin A7 statik varlığı `ortam.sahne_poz` pozunda (gövde 2): taban, duvarlar ve
//    öteki yapılar varlığın ağlarındandır (A7-K27: `akustik[]` doluysa SON_*, boşsa COL_* kaynak
//    ağları birebir; A7-K56: sonar tabanı geometriden alır). Malzeme üçgen başına (A7-K28), A7 §3.4
//    akustik alanları; `kalibrasyon.malzeme.<kimlik>.akustik.*` geçersiz kılar (A5-K13, K18).
//  - `hedefler.<ad>`: varlığın ağı `konum`/`yonelim` pozunda, gövde `hedef_govde_kimligi(kimlik)`;
//    yalnız akustik yansıtıcı cisim. `hareket.tur` sabit, sabit_hiz ve sinus sahnede uygulanır
//    (A5 §3.10 formülleri); rastgele_yuruyus ve `yonelim_rastgele = true` A5 Ek D akışı ister, bu
//    dilimde açık hatayla reddedilir.
//  - A7 manifesti senaryonun `varliklar.kume`sinden yeniden okunur; baytların SHA-256'sı doğrulama
//    raporundaki kaynak özetine (koşulsuz) ve verilmişse `varliklar.manifest_ozeti`'ne eşit, beyan
//    edilmişse `varliklar.kume_surumu` manifestteki değere eşit olmalıdır (A5-K115; W4-bagla-sonar
//    sol #3). Varlıklar `a7_varlik_yukle` ile okunur; yüklenemeyen varlık A7'nin hata koduyla açık
//    hatadır (A7-K06 `OZET_UYUSMAZ`, dosya yoksa `EKSIK_VARLIK`; A7-K51). Yer tutucu özetli uygunluk
//    örneği (`assets/ornek`) bu yüzden sonar açıkken yüklenemez.
//
// A5'te olmayan girdiler `ida::sonar` varsayılanında kalır: Ping360 tarama yapılandırması ve Ping1D
// penceresi (W4-K18 (c); A1-K49 1.0.1 notu), ışın sayısı (A5-K78: sade kademede 5 000).
#pragma once

#include "ida/senaryo/senaryo.hpp"
#include "ida/sonar/ag_sahnesi.hpp"
#include "ida/sonar/ayarlar.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ida::senaryo {

// Sonar sahnesinin gövde kimlikleri.
inline constexpr std::uint32_t kSuYuzeyiGovdesi = 1;
inline constexpr std::uint32_t kStatikGovde = 2;
inline constexpr std::uint32_t hedef_govde_kimligi(std::uint16_t hedef_kimligi) { return 1000u + hedef_kimligi; }

struct SonarKurulumu {
    sonar::SonarKumesiAyari ayar;                 // etkin olmayanlar dahil bütün sonarlar
    std::shared_ptr<sonar::AgSahnesi> sahne;      // etkin sonar yoksa boş
    std::vector<std::pair<std::string, std::uint32_t>> hedef_govdeleri;   // hedefler.<ad> → gövde
    std::vector<std::string> uyarilar;
    std::size_t etkin_sayisi = 0;                 // etkin Ping1D + Ping360
};

// `kaynak` senaryoyu çözen kaynakla, `rapor` o çözümün doğrulama raporuyla aynı olmalıdır (A7
// manifesti için). Hata: nullopt, `hata`. Desteklenmeyen kademe (`kalite.sonar = tam`) hatadır
// (sonar modeli bu dilimde sade kademededir).
std::optional<SonarKurulumu> sonar_kur(const DogrulanmisSenaryo& cozumlenmis, const DosyaKaynagi& kaynak,
                                       const DogrulamaRaporu& rapor, std::string* hata = nullptr);

}  // namespace ida::senaryo
