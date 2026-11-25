// yazan_surec.cpp — çökme testinin (kabul 5) yazan süreci: `ida_kayit_test_yazan <dizin> <hazır dosyası>`.
//
// Kayıt dosyasını açar; 40 ve 60 iletilik iki parçayı 1 s kuralıyla (enjekte edilen saat ilerletilerek)
// kapatır; 25 iletiyi açık parçada bırakır; hazır dosyasını oluşturur ve öldürülmeyi bekler. Hiçbir
// kapanış işi yapmaz: üst süreç onu SIGKILL / TerminateProcess ile öldürür.
#include "test_destek.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>

using namespace ida::kayit;
using namespace ida::kayit::test;

namespace {

// Saati 1 s adımlarla ilerleterek en az `hedef` parçanın kapanmasını bekler (koşula bağlı bekleme;
// yazıcının iş parçacığı gerçek zamanda yoklar).
bool parca_bekle(Yazici& y, SahteSaat& saat, std::uint64_t hedef)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < son) {
        if (y.sayaclar().kapanan_parca >= hedef) {
            return true;
        }
        saat.ilerlet(1'000'000'000);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

// Üretilen bütün iletiler yazıcının açık parçasına alınana dek bekler (koşula bağlı). Saat bu sırada
// ilerlemediği için parça kapanmaz; böylece her parçanın ileti sayısı kesindir.
bool alinmasini_bekle(Yazici& y, std::uint64_t hedef)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < son) {
        if (y.sayaclar().kuyruktan_alinan >= hedef) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    return false;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "kullanım: ida_kayit_test_yazan <dizin> <hazır dosyası>\n");
        return 2;
    }
    SahteSaat saat;
    Ayar a;
    a.dizin = argv[1];
    a.kademe = Kademe::telemetri;
    a.ureticiler.push_back(UreticiAyari{"dunya", std::size_t{1} << 20});
    a.is_parcacigi = true;
    a.saat = saat.islev();
    a.yoklama_araligi_ns = 200'000;
    const OrnekEkler ekler = ornek_ekler();
    const std::vector<Ek> ek = ekler.ekler();
    std::string hata;
    auto y = Yazici::ac(a, ornek_ust_veri(kOrnekRunId, "hil_bariyer"), ek, hata);
    if (!y) {
        std::fprintf(stderr, "yazıcı açılamadı: %s\n", hata.c_str());
        return 3;
    }
    ida::a3::yuk_calistirma_durum cd;
    std::uint64_t n = 0;
    const auto yaz = [&](std::uint64_t adet) {
        for (std::uint64_t i = 0; i < adet; ++i, ++n) {
            if (y->uretici(0).yaz<Kanal::calistirma_durum>(static_cast<std::int64_t>(n) * 1000, n, cd) != YazmaSonucu::yazildi) {
                return false;
            }
        }
        return true;
    };
    if (!yaz(40) || !alinmasini_bekle(*y, 40) || !parca_bekle(*y, saat, 1) || !yaz(60) || !alinmasini_bekle(*y, 100) ||
        !parca_bekle(*y, saat, 2) || !yaz(25) || !alinmasini_bekle(*y, 125)) {
        std::fprintf(stderr, "parçalar hazırlanamadı\n");
        return 4;
    }
    {
        std::ofstream f(argv[2]);
        f << "hazir\n";
    }
    // Öldürülmeyi bekle (üst süreç ölürse de sonsuza dek kalmamak için sınırlı).
    std::this_thread::sleep_for(std::chrono::seconds(120));
    std::_Exit(5);
}
