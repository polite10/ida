// W4-sim-senaryo bütünleme testleri: `ida_sim`'in A5 senaryo dosyasından kurulması ve zaman
// çizelgesi. Senaryolar depodaki scenarios/z1.toml'dan bellekte türetilir (BellekKaynagi);
// depo kökü derleme zamanında verilir (IDA_DEPO_KOKU).
//
//   senaryo_z1_esitligi [ida_sil]  madde 6: dosya koşusu = yerleşik Z1'in GUIDED_SP'siz biçimi
//                                  (bayt bayt; süreç içi ve ida_sil); yerleşik Z1'den fark raporu
//   senaryo_gecersiz               geçersiz ve yürütülemez senaryo: oturum açılmaz; derleme kısıtı
//   senaryo_zaman_cizelgesi        başlangıç/bitiş sınırları, aynı sınırda bitiş-başlangıç, koşul
//   senaryo_emniyet                EMNİYET: acil durdurma bas/bırak (W4-K3), gevşetme reddi ve
//                                  atama çelişkisi (A5-K63)
//   senaryo_belirlenim             aynı senaryo/tohum aynı CSV; farklı tohum farklı; A5-K53
//   senaryo_gorev                  görev yüklemesi (A5-K57; A1-K69)
//   senaryo_hil_pty                HIL 1x, sözde uçbirim, 60 s (zamanlama)
#include "ida/port.h"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#ifndef IDA_DEPO_KOKU
#define IDA_DEPO_KOKU "."
#endif

namespace {

using namespace ida;

int g_hata = 0;

void dogrula(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata;
    }
}

// Derleme zamanı kısıtı (kabul 2): doğrulanmamış bir ağaç kuruluma ulaşamaz.
static_assert(!std::is_constructible_v<senaryo::DogrulanmisSenaryo, senaryo::Deger, std::string, std::string>,
              "DogrulanmisSenaryo yalnız çözümleyiciden");
static_assert(!std::is_default_constructible_v<senaryo::DogrulanmisSenaryo>, "DogrulanmisSenaryo varsayılan kurulamaz");

std::string z1_metni()
{
    senaryo::DiskKaynagi d(IDA_DEPO_KOKU);
    return d.oku("scenarios/z1.toml").value_or("");
}

void degistir(std::string& s, const std::string& a, const std::string& b)
{
    const std::size_t i = s.find(a);
    if (i == std::string::npos) {
        std::printf("  (sınama hatası: metinde yok: %s)\n", a.c_str());
        ++g_hata;
        return;
    }
    s.replace(i, a.size(), b);
}

// z1.toml'dan bellekte bir senaryo (yol scenarios/<ad>.toml; araç dosyası ve manifest diskten).
std::shared_ptr<senaryo::BellekKaynagi> kaynak(const std::string& metin, const std::string& ad = "t")
{
    senaryo::DiskKaynagi d(IDA_DEPO_KOKU);
    auto b = std::make_shared<senaryo::BellekKaynagi>();
    for (const char* y : {"scenarios/ida0.toml", "assets/ornek/manifest.json"}) {
        b->dosyalar[y] = d.oku(y).value_or("");
    }
    b->dosyalar["scenarios/" + ad + ".toml"] = metin;
    return b;
}

sunucu::Ayar dosya_ayari(const std::shared_ptr<senaryo::BellekKaynagi>& k, const std::string& ad = "t")
{
    sunucu::Ayar a;
    a.gunluk = 0;
    a.senaryo_dosyasi = "scenarios/" + ad + ".toml";
    a.senaryo_kaynagi = k;
    return a;
}

std::vector<std::string> satirlar(const std::string& csv)
{
    std::vector<std::string> v;
    std::istringstream is(csv);
    std::string s;
    while (std::getline(is, s)) {
        v.push_back(s);
    }
    return v;
}

std::vector<std::string> alanlar(const std::string& s)
{
    std::vector<std::string> v;
    std::istringstream is(s);
    std::string a;
    while (std::getline(is, a, ',')) {
        v.push_back(a);
    }
    return v;
}

// İki CSV'nin ilk farkı: satır, zaman ve farklı sütunların adları.
std::string ilk_fark(const std::string& x, const std::string& y)
{
    const auto a = satirlar(x);
    const auto b = satirlar(y);
    const auto baslik = a.empty() ? std::vector<std::string>{} : alanlar(a[0]);
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        if (a[i] == b[i]) {
            continue;
        }
        const auto fa = alanlar(a[i]);
        const auto fb = alanlar(b[i]);
        std::string s = "satır " + std::to_string(i) + " (t = " + (fa.empty() ? "?" : fa[0]) + " s); farklı sütunlar:";
        std::size_t n = 0;
        for (std::size_t j = 0; j < std::min(fa.size(), fb.size()); ++j) {
            if (fa[j] != fb[j]) {
                if (n < 12) {
                    s += " " + (j < baslik.size() ? baslik[j] : std::to_string(j));
                }
                ++n;
            }
        }
        return s + (n > 12 ? " … (" + std::to_string(n) + ")" : "");
    }
    return a.size() == b.size() ? "" : "satır sayısı farklı";
}

int test_z1_esitligi(const char* sil_yolu)
{
    // Yerleşik Z1 (GUIDED_SP'li) ve GUIDED_SP'siz biçimi.
    sunucu::Ayar y;
    y.gunluk = 0;
    const sunucu::Sonuc ya = sunucu::kos(y);
    y.z1.guided_sp = false;
    const sunucu::Sonuc yb = sunucu::kos(y);
    // Dosya (diskteki scenarios/z1.toml).
    sunucu::Ayar d;
    d.gunluk = 0;
    d.senaryo_dosyasi = "scenarios/z1.toml";
    d.depo_koku = IDA_DEPO_KOKU;
    const sunucu::Sonuc dc = sunucu::kos(d);
    dogrula(ya.tamam && yb.tamam && dc.tamam, "üç koşu tamamlandı " + dc.hata);
    std::printf("  z1.toml davranış özeti %s\n", dc.davranis_ozeti.c_str());
    dogrula(!dc.csv.empty() && dc.csv == yb.csv,
            "dosya (süreç içi) = yerleşik Z1'in GUIDED_SP'siz biçimi, bayt bayt (" + std::to_string(dc.csv.size()) +
                " bayt)" + (dc.csv == yb.csv ? "" : "; ilk fark: " + ilk_fark(yb.csv, dc.csv)));
    const std::string f = ilk_fark(ya.csv, dc.csv);
    std::printf("  RAPOR: yerleşik Z1 ile dosya koşusu %s%s\n", f.empty() ? "aynı" : "farklı; ilk fark ", f.c_str());
    dogrula(!f.empty(), "yerleşik Z1'den fark yalnız GUIDED_SP'den (A5'te karşılığı yok; mimara raporlandı)");
    if (sil_yolu != nullptr) {
        sunucu::Ayar t = d;
        t.karsi = sunucu::Karsi::tcp;
        t.karsi_verildi = true;
        t.sil_yolu = sil_yolu;
        const sunucu::Sonuc tc = sunucu::kos(t);
        dogrula(tc.tamam && tc.csv == yb.csv,
                "dosya (iki süreç, ida_sil) = yerleşik GUIDED_SP'siz, bayt bayt" +
                    (tc.csv == yb.csv ? std::string() : "; ilk fark: " + ilk_fark(yb.csv, tc.csv)) + " " + tc.hata);
    }
    return 0;
}

int test_gecersiz()
{
    // Geçersiz (aşama 2): negatif süre. Oturum açılmaz, rapor JSON'u döner.
    std::string m = z1_metni();
    degistir(m, "sure_ns = 90_000_000_000", "sure_ns = -5");
    sunucu::Sonuc s = sunucu::kos(dosya_ayari(kaynak(m)));
    dogrula(!s.tamam && s.hata_turu == sunucu::HataTuru::senaryo_gecersiz, "geçersiz senaryo reddedildi: " + s.hata);
    dogrula(s.dogrulama_raporu.find("\"gecerli\":false") != std::string::npos, "doğrulama raporu (A5 §3.14)");
    dogrula(s.sayac.parti_gonderilen == 0u && s.sayac.oturum_kurulan == 0u, "oturum açılmadı");
    // A5-K62: aynı sınırda zamanlı kısıtlayan + gevşeten atama yüklemede reddedilir.
    m = z1_metni();
    m += "\n[[olaylar]]\nkimlik = \"bas\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 5_000_000_000\n"
         "girdi = \"acil_durdurma\"\ndeger = true\n"
         "\n[[olaylar]]\nkimlik = \"pim\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 5_000_000_000\n"
         "girdi = \"pim\"\nhedef = \"f1\"\ndeger = false\n";
    s = sunucu::kos(dosya_ayari(kaynak(m)));
    dogrula(s.hata_turu == sunucu::HataTuru::senaryo_gecersiz, "A5-K62: zamanlı çelişen emniyet atamaları reddedildi");
    // Yürütülemez sınıf: geçerli ama bu dilimde yok (sistem/kart_sifirlama).
    m = z1_metni();
    m += "\n[[olaylar]]\nkimlik = \"sifirla\"\nsinif = \"sistem\"\nzaman_ns = 5_000_000_000\ntur = \"kart_sifirlama\"\n";
    s = sunucu::kos(dosya_ayari(kaynak(m)));
    dogrula(s.hata_turu == sunucu::HataTuru::senaryo_yurutulemez && s.sayac.parti_gonderilen == 0u,
            "yürütülemeyen sınıf başlamadan açık hata: " + s.hata);
    // Koşulun görev öğesi büyüklüğü yürütülemez.
    m = z1_metni();
    m += "\n[[olaylar]]\nkimlik = \"kg\"\nsinif = \"gcs_komutu\"\nkomut = \"DISARM\"\n"
         "kosul = { buyukluk = \"gorev_ogesi\", karsilastirma = \">=\", deger = 1.0 }\n";
    s = sunucu::kos(dosya_ayari(kaynak(m)));
    dogrula(s.hata_turu == sunucu::HataTuru::senaryo_yurutulemez, "kosul.buyukluk = gorev_ogesi yürütülemez: " + s.hata);
    // EMNİYET (sol #1): yerleşik plana özgü seçenekler dosyayla birlikte başlamadan reddedilir;
    // oturum açılmaz, ileti seçeneği adlandırır.
    {
        sunucu::Ayar b = dosya_ayari(kaynak(z1_metni()));
        b.acil_durdurma_s = 12.0;
        sunucu::Sonuc r = sunucu::kos(b);
        dogrula(r.hata_turu == sunucu::HataTuru::kullanim && r.hata.find("--acil ") == 0u &&
                    r.sayac.parti_gonderilen == 0u,
                "--acil + senaryo: başlamadan kullanım hatası: " + r.hata);
        b = dosya_ayari(kaynak(z1_metni()));
        b.acil_kalkar_s = 18.0;
        r = sunucu::kos(b);
        dogrula(r.hata_turu == sunucu::HataTuru::kullanim && r.hata.find("--acil-kalk") == 0u,
                "--acil-kalk + senaryo: kullanım hatası");
        // Dar denetim: verilmiş ama değeri negatif ya da NaN (değerden bağımsız ret).
        b = dosya_ayari(kaynak(z1_metni()));
        b.acil_verildi = true;
        b.acil_durdurma_s = -1.0;
        r = sunucu::kos(b);
        dogrula(r.hata_turu == sunucu::HataTuru::kullanim && r.hata.find("--acil ") == 0u,
                "--acil (son değer −1) + senaryo: kullanım hatası");
        b = dosya_ayari(kaynak(z1_metni()));
        b.acil_kalkar_s = std::numeric_limits<double>::quiet_NaN();
        r = sunucu::kos(b);
        dogrula(r.hata_turu == sunucu::HataTuru::kullanim && r.hata.find("--acil-kalk") == 0u,
                "--acil-kalk NaN (bayraksız) + senaryo: kullanım hatası");
        // Yerleşik koşuda geçersiz değerler de açık hata.
        sunucu::Ayar y;
        y.gunluk = 0;
        y.acil_durdurma_s = std::numeric_limits<double>::quiet_NaN();
        r = sunucu::kos(y);
        dogrula(r.hata_turu == sunucu::HataTuru::kullanim && r.sayac.parti_gonderilen == 0u,
                "yerleşik: --acil NaN kullanım hatası: " + r.hata);
        y = sunucu::Ayar{};
        y.gunluk = 0;
        y.acil_verildi = true;
        y.acil_durdurma_s = -1.0;
        r = sunucu::kos(y);
        dogrula(r.hata_turu == sunucu::HataTuru::kullanim, "yerleşik: --acil −1 kullanım hatası");
        y = sunucu::Ayar{};
        y.gunluk = 0;
        y.acil_durdurma_s = 12.0;
        y.acil_kalkar_s = 10.0;
        r = sunucu::kos(y);
        dogrula(r.hata_turu == sunucu::HataTuru::kullanim, "yerleşik: --acil-kalk ≤ --acil kullanım hatası");
        b = dosya_ayari(kaynak(z1_metni()));
        b.ariza_verildi = true;
        r = sunucu::kos(b);
        dogrula(r.hata_turu == sunucu::HataTuru::kullanim && r.hata.find("--ariza") == 0u,
                "--ariza + senaryo: kullanım hatası");
    }
    // Komut satırı: --tohum dosyayla verilemez.
    sunucu::Ayar a = dosya_ayari(kaynak(z1_metni()));
    a.tohum_verildi = true;
    s = sunucu::kos(a);
    dogrula(s.hata_turu == sunucu::HataTuru::kullanim, "--tohum + senaryo: kullanım hatası");
    return 0;
}

// sol #4: geçerli senaryonun doğrulama uyarıları (gevşek başlangıç, gevşeten olay; A5-K71)
// kaynak ve alan bilgisiyle Sonuc'a taşınır; rapor değiştirilmeden saklanır.
int test_dogrulama_uyarilari()
{
    std::string m = z1_metni();
    degistir(m, "sure_ns = 90_000_000_000", "sure_ns = 1_000_000_000");
    m += "\n[emniyet_girdileri]\nfis = { f1 = true, f2 = false }\n"
         "\n[[olaylar]]\nkimlik = \"pimcik\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 500_000_000\n"
         "girdi = \"pim\"\nhedef = \"f2\"\ndeger = false\n";
    const sunucu::Sonuc s = sunucu::kos(dosya_ayari(kaynak(m)));
    dogrula(s.tamam, "koşu " + s.hata);
    bool fis = false, olay = false;
    for (const auto& b : s.dogrulama_bulgulari) {
        std::printf("  bulgu %s %s %s %s: %s\n", b.onem.c_str(), b.kural.c_str(), b.dosya.c_str(), b.yol.c_str(),
                    b.ileti.c_str());
        fis = fis || (b.kural == "A5-K71" && b.onem == "uyari" && b.yol.find("emniyet_girdileri.fis.f1") != std::string::npos &&
                      b.dosya == "scenarios/t.toml");
        olay = olay || (b.kural == "A5-K71" && b.onem == "uyari" && b.yol.find("olaylar") != std::string::npos);
    }
    dogrula(fis, "A5-K71 uyarısı: gevşek başlangıç (fiş takılı), dosya ve alanıyla");
    dogrula(olay, "A5-K71 uyarısı: gevşeten zaman çizelgesi ataması");
    dogrula(s.dogrulama_raporu.find("\"gecerli\":true") != std::string::npos, "geçerli senaryonun raporu saklandı");
    return 0;
}

int test_zaman_cizelgesi()
{
    std::string m = z1_metni();
    degistir(m, "sure_ns = 90_000_000_000", "sure_ns = 24_000_000_000");
    // İtici: A (t2 × 0,5) 10,000000001 s'de başlar, 5 s sürer → 15,00125 s sınırında biter; B
    // (t2 × 0,25) aynı sınırda başlar: o sınırda yalnız B (önce bitişler).
    m += "\n[[olaylar]]\nkimlik = \"itA\"\nsinif = \"aktuator\"\nzaman_ns = 10_000_000_001\ntur = \"itki_carpani\"\n"
         "hedef = \"t2\"\nsure_ns = 5_000_000_000\nsiddet = { carpan = 0.5 }\n"
         "\n[[olaylar]]\nkimlik = \"itB\"\nsinif = \"aktuator\"\nzaman_ns = 15_000_000_001\ntur = \"itki_carpani\"\n"
         "hedef = \"t2\"\nsiddet = { carpan = 0.25 }\n"
         // Sensör: basınç ofseti 20,000000001 s'de başlar, 1 s sürer.
         "\n[[olaylar]]\nkimlik = \"bofs\"\nsinif = \"sensor\"\nzaman_ns = 20_000_000_001\ntur = \"ofset\"\n"
         "hedef = \"baro0\"\nsure_ns = 1_000_000_000\nsiddet = { ofset = 50.0 }\nkanal_maskesi = 4294967295\n"
         // Koşul: ilk fizik adımından sonra sağlanır (derinlik ≥ −10 m) → 1. sınırda başlar (A5-K63);
         // dosyadaki Z1 dalmaz (GUIDED_SP A5'te yok), derinliğe bağlı anlamlı bir eşik yoktur.
         "\n[[olaylar]]\nkimlik = \"kosullu\"\nsinif = \"emniyet_girdisi\"\ngirdi = \"pim\"\nhedef = \"f2\"\ndeger = true\n"
         "kosul = { buyukluk = \"derinlik\", karsilastirma = \">=\", deger = -10.0 }\n"
         // Hiç sağlanmayan koşul: uygulanmadi / kosul_saglanmadi.
         "\n[[olaylar]]\nkimlik = \"hic\"\nsinif = \"emniyet_girdisi\"\ngirdi = \"pim\"\nhedef = \"f2\"\ndeger = true\n"
         "kosul = { buyukluk = \"derinlik\", karsilastirma = \">\", deger = 50.0 }\n";
    sunucu::Ayar a = dosya_ayari(kaynak(m));
    std::vector<sunucu::Ayar::SinirGozlemi> g;
    g.reserve(20000);
    a.sinir_kancasi = [&](const sunucu::Ayar::SinirGozlemi& x) { g.push_back(x); };
    std::int64_t ofs_ilk = -1, ofs_son = -1, ofs_disi = 0;
    a.olcum_kancasi = [&](const sensor::Olcum& o) {
        if (o.hdr.tur != sensor::SensorTuru::basinc) {
            return;
        }
        const bool ariza = o.hdr.ariza_maskesi != 0u;
        const bool pencere = o.hdr.t_meas_ns >= 20'001'250'000 && o.hdr.t_meas_ns < 21'001'250'000;
        if (ariza && ofs_ilk < 0) {
            ofs_ilk = o.hdr.t_meas_ns;
        }
        if (ariza) {
            ofs_son = o.hdr.t_meas_ns;
        }
        ofs_disi += ariza != pencere ? 1 : 0;
    };
    const sunucu::Sonuc s = sunucu::kos(a);
    dogrula(s.tamam, "koşu " + s.hata);
    std::size_t hata_a = 0;
    for (const auto& x : g) {
        const double beklenen = x.b < 8001u ? 1.0 : (x.b < 12001u ? 0.5 : 0.25);
        hata_a += x.itici_carpani[1] != beklenen ? 1u : 0u;
    }
    dogrula(!g.empty() && hata_a == 0u,
            "itici: A 8001. sınırda başlar, 12001'de biter; B 12001'de başlar (aynı sınırda önce bitiş) (" +
                std::to_string(hata_a) + " hatalı sınır)");
    dogrula(ofs_ilk >= 20'001'250'000 && ofs_son < 21'001'250'000 && ofs_disi == 0,
            "sensör ofseti yalnız [20,00125; 21,00125) s ölçümlerinde (ilk " + std::to_string(ofs_ilk) + ", son " +
                std::to_string(ofs_son) + ")");
    // Koşul: ilk sağlandığı sınır (durumu derinlik ≥ 1 olan ilk sınır, b ≥ 1).
    std::int64_t beklenen_t = -1;
    for (const auto& x : g) {
        if (x.b >= 1u && x.derinlik >= -10.0) {
            beklenen_t = static_cast<std::int64_t>(x.b) * 1'250'000;
            break;
        }
    }
    std::int64_t gercek_t = -2;
    for (const auto& o : s.olay_sonuclari) {
        if (o.kimlik == "kosullu") {
            gercek_t = o.t_ns;
        }
    }
    dogrula(beklenen_t == 1'250'000 && gercek_t == beklenen_t,
            "koşullu olay koşulun ilk sağlandığı adımı izleyen sınırda (" + std::to_string(gercek_t) + " ns)");
    bool hic = false;
    for (const auto& o : s.olay_sonuclari) {
        hic = hic || (o.kimlik == "hic" && o.sonuc == "uygulanmadi" && o.neden == "kosul_saglanmadi");
    }
    dogrula(hic, "sağlanmayan koşul: uygulanmadi / kosul_saglanmadi");
    // Zamanlı olayların sonuç anları: t ≥ zaman_ns olan ilk sınır (A5-K62).
    bool anlar = true;
    for (const auto& o : s.olay_sonuclari) {
        if (o.kimlik == "itA") {
            anlar = anlar && o.t_ns == 10'001'250'000;
        }
        if (o.kimlik == "arm") {
            anlar = anlar && o.t_ns == 3'000'000'000;
        }
    }
    dogrula(anlar, "zamanlı olayların uygulama sınırları");

    // sol #2: koşullu sensör arızası (baro0 ofseti, 1 s) koşulun sağlandığı sınırda (1,25 ms)
    // başlar, ölçümü değiştirir ve süresi sonunda (1,00125 s) kalkar.
    std::string k = z1_metni();
    degistir(k, "sure_ns = 90_000_000_000", "sure_ns = 3_000_000_000");
    k += "\n[[olaylar]]\nkimlik = \"kofs\"\nsinif = \"sensor\"\ntur = \"ofset\"\nhedef = \"baro0\"\n"
         "sure_ns = 1_000_000_000\nsiddet = { ofset = 50.0 }\nkanal_maskesi = 4294967295\n"
         "kosul = { buyukluk = \"derinlik\", karsilastirma = \">=\", deger = -10.0 }\n";
    sunucu::Ayar ka = dosya_ayari(kaynak(k));
    std::int64_t k_ilk = -1, k_son = -1, k_disi = 0, k_say = 0;
    ka.olcum_kancasi = [&](const sensor::Olcum& o) {
        if (o.hdr.tur != sensor::SensorTuru::basinc) {
            return;
        }
        const bool ariza = o.hdr.ariza_maskesi != 0u;
        const bool pencere = o.hdr.t_meas_ns >= 1'250'000 && o.hdr.t_meas_ns < 1'001'250'000;
        if (ariza) {
            k_ilk = k_ilk < 0 ? o.hdr.t_meas_ns : k_ilk;
            k_son = o.hdr.t_meas_ns;
            ++k_say;
        }
        k_disi += ariza != pencere ? 1 : 0;
    };
    const sunucu::Sonuc ks = sunucu::kos(ka);
    std::string kofs_sonuc;
    for (const auto& o : ks.olay_sonuclari) {
        if (o.kimlik == "kofs") {
            kofs_sonuc = o.sonuc + "@" + std::to_string(o.t_ns);
        }
    }
    dogrula(ks.tamam && kofs_sonuc == "uygulandi@1250000", "koşullu sensör arızası 1,25 ms sınırında uygulandı (" +
                                                              kofs_sonuc + ") " + ks.hata);
    dogrula(k_say > 0 && k_disi == 0, "ofset yalnız [1,25 ms; 1,00125 s) ölçümlerinde (" + std::to_string(k_say) +
                                          " kayıt, ilk " + std::to_string(k_ilk) + ", son " + std::to_string(k_son) + ")");
    return 0;
}

int test_emniyet()
{
    // (a) Acil durdurma bas (12 s) / bırak (18 s).
    std::string m = z1_metni();
    degistir(m, "sure_ns = 90_000_000_000", "sure_ns = 25_000_000_000");
    m += "\n[[olaylar]]\nkimlik = \"bas\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 12_000_000_000\n"
         "girdi = \"acil_durdurma\"\ndeger = true\n"
         "\n[[olaylar]]\nkimlik = \"birak\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 18_000_000_000\n"
         "girdi = \"acil_durdurma\"\ndeger = false\n";
    sunucu::Ayar a = dosya_ayari(kaynak(m));
    std::uint64_t ilk_notr_disi = 0;
    a.sinir_kancasi = [&](const sunucu::Ayar::SinirGozlemi& x) {
        if (x.b >= 14400u && ilk_notr_disi == 0u) {
            for (double p : x.pwm) {
                if (p != 1500.0) {
                    ilk_notr_disi = x.b;
                    break;
                }
            }
        }
    };
    sunucu::Sonuc s = sunucu::kos(a);
    dogrula(s.tamam, "koşu " + s.hata);
    dogrula(s.estop_adim == 4800u, "acil durdurma 6 s etkin (" + std::to_string(s.estop_adim) + " adım)");
    dogrula(s.ihlal_estop == 0u, "EMNİYET: acil durdurmada darbe genişliği nötr ve itki isteği 0 (" +
                                     std::to_string(s.ihlal_estop) + " ihlal)");
    dogrula(s.sayac.estop_iptal > 0u, "W4-K3: bekleyen komut ve yuvalar iptal edildi");
    dogrula(ilk_notr_disi >= 14408u, "W4-K3: bırakıştan sonra itki yalnız bırakıştan sonra gönderilen partiyle (ilk " +
                                        std::to_string(ilk_notr_disi) + ". sınır; ≥ 14408)");
    // (b), (c) Aynı sınırda (1): zamanlı acil bas + koşullu pim f1 çıkar + koşullu acil bırak.
    m = z1_metni();
    degistir(m, "sure_ns = 90_000_000_000", "sure_ns = 2_000_000_000");
    m += "\n[[olaylar]]\nkimlik = \"bas1\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1_250_000\n"
         "girdi = \"acil_durdurma\"\ndeger = true\n"
         "\n[[olaylar]]\nkimlik = \"pimcik\"\nsinif = \"emniyet_girdisi\"\ngirdi = \"pim\"\nhedef = \"f1\"\ndeger = false\n"
         "kosul = { buyukluk = \"derinlik\", karsilastirma = \">=\", deger = -10.0 }\n"
         "\n[[olaylar]]\nkimlik = \"birak1\"\nsinif = \"emniyet_girdisi\"\ngirdi = \"acil_durdurma\"\ndeger = false\n"
         "kosul = { buyukluk = \"derinlik\", karsilastirma = \">=\", deger = -10.0 }\n";
    s = sunucu::kos(dosya_ayari(kaynak(m)));
    dogrula(s.tamam, "koşu " + s.hata);
    bool pim_red = false, birak_red = false;
    std::uint32_t rb = 0, r3 = 0;
    for (const auto& r : s.gevsetme_retleri) {
        if (r.olay_kimligi == "pimcik" && r.alan == "emniyet_girdileri.pim.f1" && !r.istenen_deger && r.t_ns == 1'250'000) {
            pim_red = true;
            rb = r.revizyon_adim_basi;
            r3 = r.revizyon_adim_3_sonu;
        }
        birak_red = birak_red || r.olay_kimligi == "birak1";
    }
    dogrula(pim_red && birak_red && r3 > rb, "gevşetmeler reddedildi ve kaydedildi (revizyon " + std::to_string(rb) +
                                                 " → " + std::to_string(r3) + ")");
    bool celiski = false;
    for (const auto& c : s.atama_celiskileri) {
        celiski = celiski || (c.alan == "emniyet_girdileri.acil_durdurma" && c.uygulanan_deger &&
                              c.olay_kimlikleri == std::vector<std::string>{"bas1", "birak1"});
    }
    dogrula(celiski, "atama_celiskisi: kısıtlayan (basılı) uygulandı, dosya sırasıyla kimlikler");
    dogrula(s.estop_adim > 1500u, "acil durdurma basılı kaldı (" + std::to_string(s.estop_adim) + " adım)");
    bool sonuc = false;
    for (const auto& o : s.olay_sonuclari) {
        sonuc = sonuc || (o.kimlik == "pimcik" && o.sonuc == "reddedildi" && o.neden == "gevsetme_reddedildi");
    }
    dogrula(sonuc, "olay sonucu: reddedildi / gevsetme_reddedildi");
    return 0;
}

std::vector<std::string> fizik_sutunlari(const std::string& csv)
{
    std::vector<std::string> v;
    for (const std::string& s : satirlar(csv)) {
        const auto f = alanlar(s);
        std::string r;
        for (std::size_t j : {0u, 2u, 4u, 6u, 8u}) {
            r += (j < f.size() ? f[j] : std::string()) + ",";
        }
        v.push_back(r);
    }
    return v;
}

int test_belirlenim()
{
    std::string m = z1_metni();
    degistir(m, "sure_ns = 90_000_000_000", "sure_ns = 20_000_000_000");
    const sunucu::Sonuc s1 = sunucu::kos(dosya_ayari(kaynak(m)));
    const sunucu::Sonuc s2 = sunucu::kos(dosya_ayari(kaynak(m)));
    dogrula(s1.tamam && !s1.csv.empty() && s1.csv == s2.csv, "aynı senaryo ve tohumla iki koşu aynı CSV");
    std::string m2 = m;
    degistir(m2, "ana_tohum = 1", "ana_tohum = 2");
    const sunucu::Sonuc s3 = sunucu::kos(dosya_ayari(kaynak(m2)));
    dogrula(s3.tamam && s3.csv != s1.csv && s3.davranis_ozeti != s1.davranis_ozeti, "farklı tohum farklı CSV");
    // A5-K53: silahlanmayan araçta otopilot bloğunu değiştirmek fizik izini değiştirmez.
    std::string m4 = m;
    degistir(m4, "zaman_ns = 3_000_000_000", "zaman_ns = 30_000_000_000");   // ARM koşu dışında
    std::string m5 = m4;
    degistir(m5, "{ kimlik = 534, tip = \"f32\", deger = 9.82 },",
             "{ kimlik = 534, tip = \"f32\", deger = 9.82 },\n  { kimlik = 1281, tip = \"f32\", deger = 0.2 },");
    const sunucu::Sonuc s4 = sunucu::kos(dosya_ayari(kaynak(m4)));
    const sunucu::Sonuc s5 = sunucu::kos(dosya_ayari(kaynak(m5)));
    dogrula(s4.tamam && s5.tamam && s4.davranis_ozeti != s5.davranis_ozeti, "otopilot bloğu farklı (davranış özeti) " + s5.hata);
    dogrula(fizik_sutunlari(s4.csv) == fizik_sutunlari(s5.csv), "A5-K53: fizik izi (gerçek durum sütunları) aynı");
    return 0;
}

int test_gorev()
{
    std::string m = z1_metni();
    degistir(m, "sure_ns = 90_000_000_000", "sure_ns = 5_000_000_000");
    m += "\n[gorev]\nyukleme_zamani_ns = 1_000_000_000\n"
         "\n[[gorev.ogeler]]\nitem_type = \"WAYPOINT\"\nframe = \"LOCAL\"\np = [0.3, 0.3, 0.0, 0.0]\nlat_e7 = 0\n"
         "lon_e7 = 0\nnorth_m = 2.0\neast_m = 0.0\ndepth = 0.0\n"
         "\n[[gorev.ogeler]]\nitem_type = \"SURFACE\"\nframe = \"NONE\"\np = [0.25, 0.0, 0.0, 0.0]\nlat_e7 = 0\n"
         "lon_e7 = 0\nnorth_m = 0.0\neast_m = 0.0\ndepth = 0.0\n";
    // (1) Alanlar: iletiler sonuç beklenmeden gönderilir; çekirdeğin girdisinde sıra ve değerler.
    sunucu::Ayar a = dosya_ayari(kaynak(m));
    a.gorev_beklemeden = true;
    std::vector<ida_cmd_t> g;
    a.ic_cagri_kancasi = [&](int, const ida_in_t& in) {
        for (unsigned i = 0; i < in.n_cmds; ++i) {
            const ida_cmd_t& c = in.cmds[i];
            if (c.type == IDA_CMD_MISSION_BEGIN || c.type == IDA_CMD_MISSION_ITEM || c.type == IDA_CMD_MISSION_END) {
                g.push_back(c);
            }
        }
    };
    const sunucu::Sonuc s0 = sunucu::kos(a);
    bool alanlar_ok = g.size() == 4u && g[0].type == IDA_CMD_MISSION_BEGIN && g[0].u.mission_begin.count == 2u &&
                      g[3].type == IDA_CMD_MISSION_END;
    if (alanlar_ok) {
        const ida_mission_item_t& w = g[1].u.mission_item;
        const ida_mission_item_t& y = g[2].u.mission_item;
        alanlar_ok = g[1].type == IDA_CMD_MISSION_ITEM && w.index == 0u && w.item_type == IDA_MI_WAYPOINT &&
                     w.frame == IDA_MI_FRAME_LOCAL && w.p[0] == 0.3f && w.p[1] == 0.3f && w.p[2] == 0.0f &&
                     w.north_m == 2.0f && w.east_m == 0.0f && w.depth == 0.0f && g[2].type == IDA_CMD_MISSION_ITEM &&
                     y.index == 1u && y.item_type == IDA_MI_SURFACE && y.frame == IDA_MI_FRAME_NONE && y.p[0] == 0.25f;
    }
    dogrula(s0.tamam && alanlar_ok, "görev iletileri sırayla ve aynı alan değerleriyle çekirdeğe ulaştı (" +
                                       std::to_string(g.size()) + " ileti)");
    // (2) Ağ geçidi kuralı: her ileti öncekinin ACCEPTED sonucundan sonra; ilk başka sonuçta durur.
    const sunucu::Sonuc s = sunucu::kos(dosya_ayari(kaynak(m)));
    dogrula(s.tamam, "koşu " + s.hata);
    std::printf("  RAPOR: görev yüklemesi %s (başarısız adım %d, ack %d, neden %d)\n", s.gorev_durumu.c_str(),
                s.gorev_basarisiz_adim, s.gorev_ack, s.gorev_neden);
    dogrula(s.gorev_durumu == "tamamlandi" ||
                (s.gorev_durumu == "reddedildi" && s.gorev_basarisiz_adim == 0 && s.gorev_ack == IDA_ACK_UNSUPPORTED),
            "MISSION_BEGIN'in sonucu bekleniyor; çekirdek desteklemiyorsa (dilim 2) adım 0'da durur");
    // (3) Tam kabul zinciri (sol notu): çekirdeğin görev yürütücüsü yokken karşı tarafın çıktısında
    // görev komutlarının sonucu kabul yapılır (sınama kancası); BEGIN → öğeler → END sırayla,
    // her biri öncekinin kabulünden sonra; (4) ikinci öğe reddedilince adım 2'de durur.
    for (int red_indis : {-1, 1}) {
        sunucu::Ayar b = dosya_ayari(kaynak(m));
        std::vector<std::pair<std::uint8_t, int>> sira;   // (tür, öğe indisi)
        b.ic_cikti_kancasi = [&, red_indis](int, const ida_in_t& in, ida_out_t& out) {
            for (unsigned i = 0; i < in.n_cmds; ++i) {
                const ida_cmd_t& c = in.cmds[i];
                if (c.type != IDA_CMD_MISSION_BEGIN && c.type != IDA_CMD_MISSION_ITEM && c.type != IDA_CMD_MISSION_END) {
                    continue;
                }
                const int indis = c.type == IDA_CMD_MISSION_ITEM ? static_cast<int>(c.u.mission_item.index) : -1;
                sira.emplace_back(c.type, indis);
                for (unsigned k = 0; k < out.n_results; ++k) {
                    if (out.results[k].cmd_id == c.cmd_id) {
                        const bool red = c.type == IDA_CMD_MISSION_ITEM && indis == red_indis;
                        out.results[k].ack = red ? IDA_ACK_REJECTED : IDA_ACK_ACCEPTED;
                        out.results[k].reason = red ? IDA_REASON_OUT_OF_RANGE : IDA_REASON_NONE;
                    }
                }
            }
        };
        const sunucu::Sonuc r = sunucu::kos(b);
        if (red_indis < 0) {
            const bool sirali = sira.size() == 4u && sira[0].first == IDA_CMD_MISSION_BEGIN && sira[1].second == 0 &&
                                sira[2].second == 1 && sira[3].first == IDA_CMD_MISSION_END;
            dogrula(r.tamam && r.gorev_durumu == "tamamlandi" && sirali,
                    "tam kabul zinciri: BEGIN → öğe 0 → öğe 1 → END, görev tamamlandı (" + r.gorev_durumu + ", " +
                        std::to_string(sira.size()) + " ileti)");
        } else {
            dogrula(r.tamam && r.gorev_durumu == "reddedildi" && r.gorev_basarisiz_adim == 2 && sira.size() == 3u &&
                        r.gorev_ack == IDA_ACK_REJECTED,
                    "öğe 1 reddedilince adım 2'de durur, END gönderilmez (" + r.gorev_durumu + ", adım " +
                        std::to_string(r.gorev_basarisiz_adim) + ", " + std::to_string(sira.size()) + " ileti)");
        }
    }
    return 0;
}

int test_hil_pty()
{
    std::string m = z1_metni();
    degistir(m, "kip = \"sil\"", "kip = \"hil_1x\"");
    degistir(m, "sure_ns = 90_000_000_000", "sure_ns = 60_000_000_000");
    degistir(m, "sil_azami_hiz = true          # sim SIL'de olabildiğince hızlı koşar\n", "");
    sunucu::Ayar a = dosya_ayari(kaynak(m));
    a.karsi = sunucu::Karsi::pty;
    a.karsi_verildi = true;
    a.gunluk = 1;
    const sunucu::Sonuc s = sunucu::kos(a);
    dogrula(s.tamam, "HIL 1x (dosya, kip hil_1x) koşusu " + s.hata);
    std::vector<std::int64_t> p = s.zaman.periyotlar_ns;
    std::sort(p.begin(), p.end());
    auto yuzde = [&](double q) {
        return p.empty() ? 0.0
                         : 1e-6 * static_cast<double>(p[std::min(p.size() - 1, static_cast<std::size_t>(q * static_cast<double>(p.size())))]);
    };
    std::printf("  ÖLÇÜM (bu Mac, HIL 1x, sözde uçbirim, 60 s, senaryo dosyası): TF %llu; periyot ms: p50 %.3f, p99,9 "
                "%.3f, en çok %.3f; TF geç yazım en çok %.3f ms; gerçek zaman kaçırma %llu; çalıştırma geçersiz 0x%x\n",
                static_cast<unsigned long long>(s.zaman.tf_sayisi), yuzde(0.5), yuzde(0.999),
                1e-6 * static_cast<double>(s.zaman.periyot_en_cok_ns), 1e-6 * static_cast<double>(s.zaman.gec_yazim_en_cok_ns),
                static_cast<unsigned long long>(s.zaman.gercek_zaman_kacirma), s.sayac.calistirma_gecersiz);
    dogrula(s.zaman.tf_sayisi >= 11'990u && s.zaman.gercek_zaman_kacirma == 0u, "60 s'de ≈ 12 000 TF, kaçırma 0");
    return 0;
}

}  // namespace

// test_butunleme.cpp'nin dağıtıcısından çağrılır; -1: bu dosyanın testi değil.
int senaryo_testi(const std::string& ad, int argc, char** argv)
{
    g_hata = 0;
    if (ad == "senaryo_z1_esitligi") {
        (void)test_z1_esitligi(argc > 2 ? argv[2] : nullptr);
    } else if (ad == "senaryo_gecersiz") {
        (void)test_gecersiz();
    } else if (ad == "senaryo_dogrulama_uyarilari") {
        (void)test_dogrulama_uyarilari();
    } else if (ad == "senaryo_zaman_cizelgesi") {
        (void)test_zaman_cizelgesi();
    } else if (ad == "senaryo_emniyet") {
        (void)test_emniyet();
    } else if (ad == "senaryo_belirlenim") {
        (void)test_belirlenim();
    } else if (ad == "senaryo_gorev") {
        (void)test_gorev();
    } else if (ad == "senaryo_hil_pty") {
        (void)test_hil_pty();
    } else {
        return -1;
    }
    return g_hata;
}
