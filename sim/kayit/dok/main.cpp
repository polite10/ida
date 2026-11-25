// ida_kayit_dok — bir A6 kayıt dosyasının özetini yazar: bütünlük sonucu, üst veri, ekler, kanallar,
// ileti sayıları, eksik kayıt bildirimleri. Testlerde ve elle denetimde kullanılır.
//
//   ida_kayit_dok <dosya.mcap> [--iletiler <n>]
//
// Çıkış kodu: 0 sağlam (kapanmış ve bozuk değil), 1 okunamadı, 2 bozuk, 3 kapanmamış, 64 kullanım.
#include "ida/kayit/coz.hpp"
#include "ida/kayit/okuyucu.hpp"

#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace {

using namespace ida::kayit;

void ust_veri_yaz(const char* baslik, const UstVeri& u)
{
    std::printf("%s\n", baslik);
    for (const auto& [k, v] : u) {
        std::printf("  %s = %s\n", k.c_str(), v.c_str());
    }
}

const char* neden_adi(std::uint8_t neden)
{
    switch (neden) {
    case 0:
        return "halka_tasmasi";
    case 1:
        return "yazma_hatasi";
    case 2:
        return "disk_dolu";
    default:
        return "?";
    }
}

}  // namespace

int main(int argc, char** argv)
{
    const char* yol = nullptr;
    unsigned long long ileti_siniri = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--iletiler" && i + 1 < argc) {
            const std::string_view n = argv[++i];
            const auto r = std::from_chars(n.data(), n.data() + n.size(), ileti_siniri);
            if (r.ec != std::errc() || r.ptr != n.data() + n.size()) {
                std::fprintf(stderr, "ida_kayit_dok: --iletiler bir sayı ister\n");
                return 64;
            }
        } else if (yol == nullptr && !a.empty() && a[0] != '-') {
            yol = argv[i];
        } else {
            std::fprintf(stderr, "kullanım: ida_kayit_dok <dosya.mcap> [--iletiler <n>]\n");
            return 64;
        }
    }
    if (yol == nullptr) {
        std::fprintf(stderr, "kullanım: ida_kayit_dok <dosya.mcap> [--iletiler <n>]\n");
        return 64;
    }

    AcmaHatasi hata = AcmaHatasi::yok;
    const auto okuyucu = Okuyucu::ac(yol, hata);
    if (!okuyucu) {
        const std::string_view ad = acma_hatasi_adi(hata);
        std::printf("dosya: %s\ndurum: okunamadi (%.*s)\n", yol, static_cast<int>(ad.size()), ad.data());
        return 1;
    }
    const DosyaOzeti& o = okuyucu->ozet();
    std::printf("dosya: %s\n", yol);
    std::printf("bayt: %llu (kullanilan: %llu)\n", static_cast<unsigned long long>(o.dosya_bayt),
                static_cast<unsigned long long>(o.kullanilan_bayt));
    std::printf("durum: %s%s\n", o.bozuk ? "bozuk" : (o.kapanmis ? "saglam" : "kapanmamis"),
                o.bozuk && !o.kapanmis ? " (kapanmamis)" : "");
    if (o.bozuk) {
        std::printf("bozulma: %s (konum %llu)\n", o.bozulma.c_str(), static_cast<unsigned long long>(o.bozulma_konumu));
    }
    std::printf("kitaplik: %s\n", o.kitaplik.c_str());
    {
        const std::string_view k = o.kademe_var ? kademe_adi(o.kademe) : std::string_view("?");
        const std::string_view g = guvence_adi(o.guvence);
        std::printf("kademe: %.*s\n", static_cast<int>(k.size()), k.data());
        std::printf("guvence: %.*s", static_cast<int>(g.size()), g.data());
        if (o.guvence_on_eki_son_adim.has_value()) {
            std::printf(" (onek son adim: %llu)", static_cast<unsigned long long>(*o.guvence_on_eki_son_adim));
        }
        std::printf("\n");
    }
    std::printf("ust_veri: %s\n", !o.ust_veri_var ? "yok" : (o.ust_veri_gecerli ? "gecerli" : "gecersiz"));
    ust_veri_yaz("ida_kayit:", o.ust_veri);
    if (o.kapanis.has_value()) {
        ust_veri_yaz("ida_kayit_kapanis:", *o.kapanis);
    } else {
        std::printf("ida_kayit_kapanis: yok\n");
    }
    std::printf("ekler: %zu\n", o.ekler.size());
    for (const EkOzeti& e : o.ekler) {
        std::printf("  %s (%s, %llu bayt)\n", e.ad.c_str(), e.ortam_turu.c_str(), static_cast<unsigned long long>(e.boyut));
    }
    std::printf("parca: %llu\n", static_cast<unsigned long long>(o.parca_sayisi));
    std::printf("ileti: %llu (okunur: %llu)\n", static_cast<unsigned long long>(o.ileti_sayisi),
                static_cast<unsigned long long>(o.okunur_ileti_sayisi));
    if (o.ilk_log_time.has_value() && o.son_log_time.has_value()) {
        std::printf("log_time: %lld .. %lld ns\n", static_cast<long long>(*o.ilk_log_time),
                    static_cast<long long>(*o.son_log_time));
    }
    std::printf("kanallar: %zu\n", o.kanallar.size());
    for (const KanalOzeti& k : o.kanallar) {
        std::printf("  %-28s v%u %10llu%s%s\n", k.konu.c_str(), static_cast<unsigned>(k.kanal_surumu),
                    static_cast<unsigned long long>(k.ileti_sayisi), k.okunur ? "" : "  OKUNMAZ: ", k.uyari.c_str());
    }
    std::printf("eksikler: %zu\n", o.eksikler.size());
    for (const EksikKaydi& e : o.eksikler) {
        std::printf("  %.*s adim %llu..%llu t %lld..%lld ns kayip %llu neden %s\n", static_cast<int>(e.kanal->ad.size()),
                    e.kanal->ad.data(), static_cast<unsigned long long>(e.ilk_adim),
                    static_cast<unsigned long long>(e.son_adim), static_cast<long long>(e.ilk_sim_t_ns),
                    static_cast<long long>(e.son_sim_t_ns), static_cast<unsigned long long>(e.kayip_sayisi),
                    neden_adi(e.neden));
    }
    if (o.ilk_eksik_fizik.has_value()) {
        std::printf("ilk_eksik_adim (fizik): %llu\n", static_cast<unsigned long long>(*o.ilk_eksik_fizik));
    }
    if (o.ilk_eksik_girdi.has_value()) {
        std::printf("ilk_eksik_adim (girdi): %llu\n", static_cast<unsigned long long>(*o.ilk_eksik_girdi));
    }
    std::printf("uyarilar: %zu\n", o.uyarilar.size());
    for (const std::string& u : o.uyarilar) {
        std::printf("  %s\n", u.c_str());
    }

    if (ileti_siniri > 0) {
        std::printf("iletiler (log_time, sequence sirasiyla):\n");
        Ileti i;
        std::string json;
        unsigned long long n = 0;
        while (n < ileti_siniri && okuyucu->sonraki(i)) {
            const CozmeSonucu c = json_dok(*i.kanal->yuk, i.veri, json);
            std::printf("  %lld %u %.*s %s\n", static_cast<long long>(i.log_time), static_cast<unsigned>(i.sequence),
                        static_cast<int>(i.kanal->ad.size()), i.kanal->ad.data(), c.tamam ? json.c_str() : "COZULEMEDI");
            ++n;
        }
        if (!okuyucu->okuma_hatasi().empty()) {
            std::printf("okuma hatasi: %s\n", okuyucu->okuma_hatasi().c_str());
            return 2;
        }
    }
    if (o.bozuk) {
        return 2;
    }
    return o.kapanmis ? 0 : 3;
}
