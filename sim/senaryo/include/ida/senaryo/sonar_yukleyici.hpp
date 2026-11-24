// İDA senaryo kitaplığı (W4-bagla-sonar): çözümlenmiş senaryodan sonar kümesinin kurulumu
// (`ida::sonar`, W4-sonar).
//
// Bu başlık ve kaynağı yalnız `sim/sonar` depoda varken derlenir (sim/senaryo/CMakeLists.txt;
// hedef `ida_senaryo_sonar`). `sensor_kur` ile aynı kural: girdi yalnız `DogrulanmisSenaryo`dur
// (doğrulanmamış ağaç derleme zamanında verilemez). Alan adları A5 §3.6, §3.9, §3.11, §3.12 ile
// bire bir aynıdır.
//
// Sahne (05 §2.4.1; senaryoda karşılığı olmayan öğe uydurulmaz):
//  - Su yüzeyi z = 0 (sonar modelinin yüzey malzemesi; A7'de su yüzeyi malzemesi yok).
//  - Düz taban z = `ortam.derinlik` (A5-K36: dünya su derinliği; havuzda ve açık suda). Taban
//    malzemesi `ortam.sahne`'nin A7 statik varlığının ilk malzeme atamasıdır (A7 §3.4 akustik
//    alanları), `kalibrasyon.malzeme.<kimlik>.akustik.*` geçersiz kılar (A5-K13, K18). A7 manifesti
//    senaryonun `varliklar.kume`sinden okunur; `varliklar.manifest_ozeti` verilmişse okunan
//    baytların SHA-256'sı ona eşit olmalıdır (A5-K115).
//  - Havuz duvarları ve hedef gövdeleri A7 glTF ağlarından kurulur (sonraki dilim; ağlar depoda
//    yok): bu dilimde sahneye girmez, kurulum uyarısı verilir (sessiz değil).
//
// A5'te olmayan girdiler `ida::sonar` varsayılanında kalır: Ping360 tarama yapılandırması ve Ping1D
// penceresi (W4-K18 (c); A1-K49 1.0.1 notu), ışın sayısı (A5-K78: sade kademede 5 000).
#pragma once

#include "ida/senaryo/senaryo.hpp"
#include "ida/sonar/ayarlar.hpp"
#include "ida/sonar/sahne.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ida::senaryo {

struct SonarKurulumu {
    sonar::SonarKumesiAyari ayar;                 // etkin olmayanlar dahil bütün sonarlar
    std::shared_ptr<sonar::CozumselSahne> sahne;
    std::vector<std::string> uyarilar;
    std::size_t etkin_sayisi = 0;                 // etkin Ping1D + Ping360
};

// `kaynak` senaryoyu çözen kaynakla aynı olmalıdır (A7 manifesti için). Hata: nullopt, `hata`.
// Desteklenmeyen kademe (`kalite.sonar = tam`) hatadır (sonar modeli bu dilimde sade kademededir).
std::optional<SonarKurulumu> sonar_kur(const DogrulanmisSenaryo& cozumlenmis, const DosyaKaynagi& kaynak,
                                       std::string* hata = nullptr);

}  // namespace ida::senaryo
