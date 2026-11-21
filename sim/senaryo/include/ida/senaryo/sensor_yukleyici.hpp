// İDA senaryo kitaplığı (W4-senaryo, dilim 2): çözümlenmiş senaryodan sensör kümesinin
// kurulum yapıları (`ida::sensor`, W4-sensor).
//
// Bu başlık ve kaynağı yalnız `sim/sensor` depoda varken derlenir (sim/senaryo/CMakeLists.txt;
// hedef `ida_senaryo_sensor`). Alan adları A5 §3.6, §3.8, §3.9 ile bire bir aynıdır.
//
// A5'te olmayan iki girdi çözümlenmiş senaryodan gelmez ve `ida::sensor` varsayılanında kalır:
// gövde içi sıcaklık modeli ve (verilmemişse) ses hızı. Ses hızı A5 `ortam.su.ses_hizi`
// verilmemişse boş bırakılır (formül sonar işindedir; A5 §3.9 "formülden").
#pragma once

#include "ida/senaryo/deger.hpp"
#include "ida/senaryo/fizik_yukleyici.hpp"
#include "ida/sensor/ayarlar.hpp"

#include <optional>
#include <string>

namespace ida::senaryo {

// Çözümlenmiş senaryodan sensör kümesi ayarı. Güç girdisi fizik araç parametrelerinden
// (`ida::sensor::guc_parametreleri`), adım ve kontrol periyodu iskelet §4 sabitleridir.
std::optional<sensor::SensorKumesiAyari> sensor_kur(const Deger& cozumlenmis,
                                                    std::string* hata = nullptr);

}  // namespace ida::senaryo
