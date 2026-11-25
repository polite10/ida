// katalog.cpp — üretilen A6 katalog tabloları üzerinde yardımcılar.
#include "ida/kayit/katalog.hpp"

namespace ida::kayit {

const KanalTanimi* kanal_bul(std::string_view ad) noexcept
{
    for (const auto& k : kanallar()) {
        if (k.ad == ad) {
            return &k;
        }
    }
    return nullptr;
}

std::string_view kademe_adi(Kademe k) noexcept
{
    switch (k) {
    case Kademe::olaylar:
        return "olaylar";
    case Kademe::telemetri:
        return "telemetri";
    case Kademe::tam:
        return "tam";
    }
    return {};
}

bool kademe_coz(std::string_view ad, Kademe& k) noexcept
{
    for (const Kademe aday : {Kademe::olaylar, Kademe::telemetri, Kademe::tam}) {
        if (kademe_adi(aday) == ad) {
            k = aday;
            return true;
        }
    }
    return false;
}

bool kip_cekirdek_pc(std::string_view kip) noexcept
{
    return kip == "sil" || kip == "tekrar_girdi_sil" || kip == "tekrar_yeniden_kurma";
}

bool kip_gecerli(std::string_view kip) noexcept
{
    return kip_cekirdek_pc(kip) || kip == "hil_1x" || kip == "hil_bariyer" || kip == "tekrar_fizik";
}

bool kanal_etkin(const KanalTanimi& k, Kademe kademe, std::string_view kip) noexcept
{
    if (static_cast<std::uint8_t>(k.kademe) > static_cast<std::uint8_t>(kademe)) {
        return false;
    }
    switch (k.kosul) {
    case KanalKosulu::yok:
        return true;
    case KanalKosulu::cekirdek_pc_kipleri:
        return kip_cekirdek_pc(kip);
    }
    return false;
}

}  // namespace ida::kayit
