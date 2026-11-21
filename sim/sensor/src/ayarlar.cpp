// İDA-0 varsayılan sensör kümesi: scenarios/ida0.toml (araç), scenarios/ornek-havuz.toml (ortam).
// Ayar değerleri A5 Ek E ürün profilleridir (yapıların üye başlatıcıları).
#include "ida/sensor/ayarlar.hpp"

namespace ida::sensor {

namespace {

SensorAyari sensor(const char* ad, SensorTuru tur, std::uint16_t kimlik, const char* profil,
                   Vek3 konum, SensorAyarGovdesi govde, std::optional<double> hiz = std::nullopt)
{
    SensorAyari a;
    a.ad = ad;
    a.tur = tur;
    a.kimlik = kimlik;
    a.inst = 0;
    a.profil = profil;
    a.hiz_hz = hiz;
    a.montaj.konum = konum;
    a.ayar = std::move(govde);
    return a;
}

}  // namespace

std::vector<SensorAyari> ida0_sensorleri()
{
    // Montajlar [MV] (scenarios/ida0.toml); bütün yönelimler birim (A5-K89).
    return {
        sensor("imu0", SensorTuru::imu, 1, "icm42688p", {0.0, 0.0, -0.03}, ImuAyar{}),
        sensor("mag0", SensorTuru::manyetometre, 2, "rm3100", {0.0, 0.0, -0.03},
               ManyetometreAyar{}, 100.0),
        sensor("baro0", SensorTuru::basinc, 3, "ms5837_30ba", {-0.19, 0.0, -0.03}, BasincAyar{},
               25.0),
        sensor("gnss0", SensorTuru::gnss, 4, "neo_m9n", {-0.05, 0.0, -0.22}, GnssAyar{}, 5.0),
        sensor("dvl0", SensorTuru::dvl, 5, "a50", {0.05, 0.0, 0.14}, DvlAyar{}),
        sensor("ping1d0", SensorTuru::ping1d, 6, "ping1d", {-0.08, 0.08, 0.14}, KapsamDisiAyar{}),
        sensor("ping360_0", SensorTuru::ping360, 7, "ping360", {0.12, 0.0, -0.20},
               KapsamDisiAyar{}),
        sensor("kamera0", SensorTuru::kamera, 8, "kamera_on", {0.21, 0.0, -0.02},
               KapsamDisiAyar{}),
        sensor("batarya0", SensorTuru::batarya_olcer, 9, "psm", {0.0, 0.0, 0.0},
               BataryaOlcerAyar{}, 10.0),
        sensor("sizinti0", SensorTuru::sizinti, 10, "sos_leak", {0.0, 0.0, 0.0}, SizintiAyar{},
               10.0),
        sensor("firlatici_io0", SensorTuru::firlatici_geri_okuma, 11, "firlatici_io",
               {0.0, 0.0, 0.0}, FirlaticiGeriOkumaAyar{}, 10.0),
    };
}

SensorKumesiAyari ida0_sensor_kumesi()
{
    SensorKumesiAyari k;
    k.sensorler = ida0_sensorleri();
    // scenarios/ornek-havuz.toml ortamı (A5 Ek E varsayılanlarıyla).
    k.ortam.yercekimi = 9.82;
    k.ortam.atmosfer_basinci = 101325.0;
    k.ortam.derinlik = 3.0;
    k.ortam.su.yogunluk = 1000.0;
    k.ortam.su.sicaklik_c = 20.0;
    k.ortam.su.ses_hizi = 1482.3;   // saf su 20 °C; tahmin (formül sonar işinde, 05 §2.2.1)
    k.ortam.orijin = OrtamOrijin{0.7155849933176751, 0.5061454830783556, 0.0};
    k.ortam.manyetik_alan = Vek3{2.56e-5, 2.7e-6, 4.03e-5};
    // scenarios/ida0.toml güç ve itici değerleri (kanal sırası t1…t8 = 0…7).
    GucParametreleri& g = k.guc;
    g.v_sabit = 15.6;
    g.aviyonik_guc = 20.0;
    g.gerilim_noktalari = {12.0, 16.0, 20.0};
    g.azami_itki_ileri = {36.3827, 51.4849, 65.7046};
    g.azami_itki_geri = {28.6354, 40.2073, 49.5236};
    g.tam_gaz_akimi = {17.0, 24.0, 32.0};
    g.akim_ussu = 1.5;
    const double s = 0.7071067811865475;
    g.itici_konumu = {Vek3{0.156, 0.111, 0.085},  Vek3{0.156, -0.111, 0.085},
                      Vek3{-0.156, 0.111, 0.085}, Vek3{-0.156, -0.111, 0.085},
                      Vek3{0.12, 0.218, 0.0},     Vek3{0.12, -0.218, 0.0},
                      Vek3{-0.12, 0.218, 0.0},    Vek3{-0.12, -0.218, 0.0}};
    g.itici_yonu = {Vek3{s, -s, 0.0},    Vek3{s, s, 0.0},     Vek3{s, s, 0.0},
                    Vek3{s, -s, 0.0},    Vek3{0.0, 0.0, 1.0}, Vek3{0.0, 0.0, 1.0},
                    Vek3{0.0, 0.0, 1.0}, Vek3{0.0, 0.0, 1.0}};
    k.ana_tohum = 1;
    return k;
}

}  // namespace ida::sensor
