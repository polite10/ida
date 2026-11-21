// komut_satiri.cpp — ida_sil komut satırı (komut_satiri.hpp).
#include "komut_satiri.hpp"

#include <cstdint>
#include <string_view>

namespace ida::sil {

namespace {

// Onluk işaretsiz tamsayı; başında '+', '-' ya da boşluk kabul edilmez.
bool onluk(std::string_view s, std::uint64_t ust, std::uint64_t* deger)
{
    if (s.empty() || s.size() > 20u) {
        return false;
    }
    std::uint64_t v = 0;
    for (const char ch : s) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        const auto d = static_cast<std::uint64_t>(ch - '0');
        if (d > ust || v > (ust - d) / 10u) {
            return false;
        }
        v = v * 10u + d;
    }
    *deger = v;
    return true;
}

// A1-K114: --a1-seed tam 16 onaltılık hane.
bool onaltilik16(std::string_view s, std::uint64_t* deger)
{
    if (s.size() != 16u) {
        return false;
    }
    std::uint64_t v = 0;
    for (const char ch : s) {
        std::uint64_t d = 0;
        if (ch >= '0' && ch <= '9') {
            d = static_cast<std::uint64_t>(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            d = static_cast<std::uint64_t>(ch - 'a') + 10u;
        } else if (ch >= 'A' && ch <= 'F') {
            d = static_cast<std::uint64_t>(ch - 'A') + 10u;
        } else {
            return false;
        }
        v = (v << 4) | d;
    }
    *deger = v;
    return true;
}

bool esc_ayristir(std::string_view s, Ayarlar* a)
{
    std::uint64_t d[3] = {};
    for (int i = 0; i < 3; ++i) {
        const std::size_t virgul = s.find(',');
        const std::string_view parca = (i < 2) ? s.substr(0, virgul) : s;
        if ((i < 2 && virgul == std::string_view::npos) || !onluk(parca, 65535u, &d[i])) {
            return false;
        }
        if (i < 2) {
            s.remove_prefix(virgul + 1u);
        }
    }
    if (!(d[0] < d[1] && d[1] < d[2])) {
        return false;
    }
    a->esc_alt_us = static_cast<std::uint16_t>(d[0]);
    a->esc_notr_us = static_cast<std::uint16_t>(d[1]);
    a->esc_ust_us = static_cast<std::uint16_t>(d[2]);
    return true;
}

}  // namespace

const char* kullanim_metni()
{
    return "Kullanım: ida_sil --a1-port <port> [seçenekler]\n"
           "\n"
           "Ortak çekirdeği ve ida_link üst katmanını kartın yerine PC'de çalıştırır; sim\n"
           "sunucusuna 127.0.0.1:<port> üzerinden TCP ile bağlanır (A1-K97).\n"
           "\n"
           "  --a1-port <1..65535>     sim sunucusunun A1 portu (zorunlu)\n"
           "  --a1-seed <16 hex hane>  BOARD_HELLO.nonce tohumu (A1-K114); yoksa 0\n"
           "  --depo <yol>             kalıcı depo dosyası (açılış sayacı, parametre bloğu);\n"
           "                           yoksa bellek içi depo\n"
           "  --n-bg <sayı>            her step ve tick'ten önceki arka plan çağrısı (A1-K121);\n"
           "                           varsayılan 1\n"
           "  --gunluk <0..3>          stderr günlüğü: 0 sessiz, 1 hata (varsayılan), 2 bilgi,\n"
           "                           3 ayrıntı\n"
           "  --esc <alt,notr,ust>     ESC darbe değerleri, µs; varsayılan 1100,1500,1900\n"
           "  --onceki-acilis <sayı>   depo dosyası yokken önceki boot_count (A1-K114); 0\n"
           "  --prev-sess <sayı>       kalıcı başlangıç durumu (A1-K114); 0\n"
           "  --oturum-sayisi <sayı>   sessions_since_boot başlangıcı (A1-K114); 0\n"
           "  --last-cmd-id <sayı>     last_cmd_id başlangıcı (A1-K114); 0\n"
           "  --bekleme-ms <sayı>      bağlantı yokken en uzun bekleme; varsayılan 10000\n"
           "  --olcum                  çıkışta çağrı süresi istatistiği (stderr)\n"
           "  --surum                  sürümü yazar ve çıkar\n"
           "  --yardim, -h             bu metni yazar ve çıkar\n"
           "\n"
           "Çıkış kodları: 0 olağan bitiş; 1 komut satırı hatası; 2 çekirdek kurulamadı;\n"
           "3 güvenli bekleme (çekirdek iç tutarlılığı); 4 bağlantı kurulamadı.\n";
}

KomutSatiri komut_satiri_ayristir(int argc, const char* const* argv)
{
    KomutSatiri k;
    bool port_var = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view ad = argv[i];
        if (ad == "--surum") {
            k.surum = true;
            continue;
        }
        if (ad == "--yardim" || ad == "-h") {
            k.yardim = true;
            continue;
        }
        if (ad == "--olcum") {
            k.ayar.olcum = true;
            continue;
        }
        static constexpr std::string_view kDegerli[] = {
            "--a1-port", "--a1-seed", "--depo", "--n-bg", "--gunluk", "--esc", "--onceki-acilis",
            "--prev-sess", "--oturum-sayisi", "--last-cmd-id", "--bekleme-ms"};
        bool bilinen = false;
        for (const std::string_view b : kDegerli) {
            bilinen = bilinen || (b == ad);
        }
        if (!bilinen) {
            k.hata = "bilinmeyen seçenek: " + std::string(ad);
            return k;
        }
        if (i + 1 >= argc) {
            k.hata = std::string(ad) + ": değer eksik";
            return k;
        }
        const std::string_view d = argv[++i];
        std::uint64_t v = 0;
        bool tamam = true;
        if (ad == "--a1-port") {
            tamam = onluk(d, 65535u, &v) && v >= 1u;
            k.ayar.a1_port = static_cast<std::uint16_t>(v);
            port_var = tamam;
        } else if (ad == "--a1-seed") {
            tamam = onaltilik16(d, &k.ayar.a1_tohum);
        } else if (ad == "--depo") {
            tamam = !d.empty();
            k.ayar.depo_yolu = std::string(d);
        } else if (ad == "--n-bg") {
            tamam = onluk(d, UINT32_MAX, &v);
            k.ayar.n_bg = static_cast<std::uint32_t>(v);
        } else if (ad == "--gunluk") {
            tamam = onluk(d, 3u, &v);
            k.ayar.gunluk = static_cast<int>(v);
        } else if (ad == "--esc") {
            tamam = esc_ayristir(d, &k.ayar);
        } else if (ad == "--onceki-acilis") {
            tamam = onluk(d, UINT32_MAX, &v);
            k.ayar.onceki_acilis = static_cast<std::uint32_t>(v);
        } else if (ad == "--prev-sess") {
            tamam = onluk(d, 65535u, &v);
            k.ayar.prev_sess = static_cast<std::uint16_t>(v);
        } else if (ad == "--oturum-sayisi") {
            tamam = onluk(d, 65535u, &v);
            k.ayar.oturum_sayisi = static_cast<std::uint16_t>(v);
        } else if (ad == "--last-cmd-id") {
            tamam = onluk(d, UINT32_MAX, &v);
            k.ayar.last_cmd_id = static_cast<std::uint32_t>(v);
        } else {   // --bekleme-ms (kDegerli'nin son öğesi)
            tamam = onluk(d, UINT32_MAX, &v);
            k.ayar.bekleme_ms = static_cast<std::uint32_t>(v);
        }
        if (!tamam) {
            k.hata = std::string(ad) + ": geçersiz değer: " + std::string(d);
            return k;
        }
    }
    if (!k.surum && !k.yardim && !port_var) {
        k.hata = "--a1-port zorunlu";
    }
    return k;
}

}  // namespace ida::sil
