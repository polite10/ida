// İDA-0 sonar kümesi (scenarios/ida0.toml, scenarios/ornek-havuz.toml).
#include "ida/sonar/ayarlar.hpp"

namespace ida::sonar {

SonarKumesiAyari ida0_sonar_kumesi()
{
    SonarKumesiAyari k;
    SonarAyari p1;
    p1.ad = "ping1d0";
    p1.tur = SensorTuru::ping1d;
    p1.kimlik = 6;
    p1.inst = 0;
    p1.profil = "ping1d";
    p1.montaj.konum = {-0.08, 0.08, 0.14};   // [MV] aşağı bakar
    p1.ayar = Ping1dAyar{};
    p1.yapilandirma = Ping1dYapilandirmasi{};
    k.sonarlar.push_back(p1);
    SonarAyari p3;
    p3.ad = "ping360_0";
    p3.tur = SensorTuru::ping360;
    p3.kimlik = 7;
    p3.inst = 0;
    p3.profil = "ping360";
    p3.montaj.konum = {0.12, 0.0, -0.20};    // [MV] tarama düzlemi yatay
    p3.ayar = Ping360Ayar{};
    p3.yapilandirma = Ping360Yapilandirmasi{};
    k.sonarlar.push_back(p3);
    k.ortam.su.sicaklik_c = 20.0;
    k.ortam.su.tuzluluk = 0.0;
    k.ortam.su.yogunluk = 1000.0;
    k.ana_tohum = 1;
    return k;
}

}  // namespace ida::sonar
