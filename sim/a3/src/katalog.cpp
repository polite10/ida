// katalog.cpp — üretilen katalog tabloları üzerinde arama yardımcıları.
#include "ida/a3/katalog.hpp"

namespace ida::a3 {

const KanalTanimi* kanal_bul(std::uint16_t kanal_id) noexcept
{
    for (const auto& k : kanallar()) {
        if (k.kanal_id == kanal_id) {
            return &k;
        }
    }
    return nullptr;
}

const KanalTanimi* kanal_bul(std::string_view ad) noexcept
{
    for (const auto& k : kanallar()) {
        if (k.ad == ad) {
            return &k;
        }
    }
    return nullptr;
}

const OlayTuru* olay_turu_bul(std::uint16_t kod) noexcept
{
    for (const auto& o : olay_turleri()) {
        if (o.kod == kod) {
            return &o;
        }
    }
    return nullptr;
}

std::uint16_t hata_http(std::string_view kod) noexcept
{
    for (const auto& h : hata_kodlari()) {
        if (h.kod == kod) {
            return h.http;
        }
    }
    return 0;
}

const YukTanimi* yuk_bul(std::string_view ad) noexcept
{
    for (const auto& y : yukler()) {
        if (y.ad == ad) {
            return &y;
        }
    }
    return nullptr;
}

namespace {

const AlanTanimi* alan_bul(std::string_view yuk, std::string_view alan) noexcept
{
    const YukTanimi* y = yuk_bul(yuk);
    if (y == nullptr) {
        return nullptr;
    }
    for (std::uint16_t i = 0; i < y->alan_sayisi; ++i) {
        if (y->alanlar[i].ad == alan) {
            return &y->alanlar[i];
        }
    }
    return nullptr;
}

}  // namespace

std::string_view deger_adi(std::string_view yuk, std::string_view alan, std::uint32_t kod) noexcept
{
    const AlanTanimi* a = alan_bul(yuk, alan);
    if (a == nullptr) {
        return {};
    }
    for (std::uint16_t i = 0; i < a->deger_sayisi; ++i) {
        if (a->degerler[i].kod == kod) {
            return a->degerler[i].ad;
        }
    }
    return {};
}

bool deger_kodu(std::string_view yuk, std::string_view alan, std::string_view ad, std::uint32_t& kod) noexcept
{
    const AlanTanimi* a = alan_bul(yuk, alan);
    if (a == nullptr) {
        return false;
    }
    for (std::uint16_t i = 0; i < a->deger_sayisi; ++i) {
        if (a->degerler[i].ad == ad) {
            kod = a->degerler[i].kod;
            return true;
        }
    }
    return false;
}

}  // namespace ida::a3
