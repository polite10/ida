// depo.cpp — kalıcı depo (depo.hpp).
#include "depo.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "ida/port.h"

namespace ida::sil {

namespace {

constexpr std::array<std::uint8_t, 8> kSihir = {'I', 'D', 'A', '-', 'S', 'I', 'L', '1'};
constexpr std::uint32_t kSurum = 1u;
constexpr std::size_t kSayacSonu = 24u;  // sayaç kesimi [0,24)
constexpr std::size_t kBaslikBoyu = 44u; // blok kesiminin başlığı [24,44)

std::array<std::uint32_t, 256> crc_tablosu()
{
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256u; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) != 0u ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        t[i] = c;
    }
    return t;
}

void le32_yaz(std::vector<std::uint8_t>& v, std::uint32_t x)
{
    for (unsigned j = 0; j < 4u; ++j) {
        v.push_back(static_cast<std::uint8_t>((x >> (8u * j)) & 0xFFu));
    }
}

std::uint32_t le32_oku(const std::vector<std::uint8_t>& v, std::size_t i)
{
    return static_cast<std::uint32_t>(v[i]) | (static_cast<std::uint32_t>(v[i + 1u]) << 8) |
           (static_cast<std::uint32_t>(v[i + 2u]) << 16) |
           (static_cast<std::uint32_t>(v[i + 3u]) << 24);
}

bool dosya_oku(const std::string& yol, std::vector<std::uint8_t>& v)
{
    std::ifstream f(std::filesystem::path(yol), std::ios::binary);
    if (!f) {
        return false;
    }
    v.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !f.bad();
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* p, std::size_t n)
{
    static const std::array<std::uint32_t, 256> kTablo = crc_tablosu();
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i) {
        c = kTablo[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

Depo::Depo(std::string yol, std::uint32_t onceki_acilis)
    : yol_(std::move(yol)), sayac_(onceki_acilis)
{
}

Depo::Yukleme Depo::yukle()
{
    blok_.reset();
    if (yol_.empty()) {
        return Yukleme::DosyaYok;
    }
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::path(yol_), ec)) {
        return Yukleme::DosyaYok;   // sayaç kurucudaki onceki_acilis'te kalır
    }
    std::vector<std::uint8_t> v;
    if (!dosya_oku(yol_, v) || v.size() < kSayacSonu ||
        !std::equal(kSihir.begin(), kSihir.end(), v.begin()) || le32_oku(v, 8u) != kSurum) {
        sayac_.reset();
        return Yukleme::DosyaBozuk;
    }
    bool sayac_tamam = false;
    if (crc32(v.data(), 20u) == le32_oku(v, 20u) && (le32_oku(v, 12u) & 1u) != 0u) {
        sayac_ = le32_oku(v, 16u);
        sayac_tamam = true;
    } else {
        sayac_.reset();
    }
    bool blok_tamam = false;
    bool blok_bozuk = false;
    if (v.size() >= kBaslikBoyu && crc32(v.data() + kSayacSonu, 16u) == le32_oku(v, 40u)) {
        const std::uint32_t var = le32_oku(v, 24u);
        const std::uint32_t gen = le32_oku(v, 28u);
        const std::uint32_t len = le32_oku(v, 32u);
        const std::uint32_t crc = le32_oku(v, 36u);
        if (var == 0u) {
            blok_tamam = v.size() == kBaslikBoyu;
            blok_bozuk = !blok_tamam;
        } else if (var == 1u && len <= IDA_PARAM_BLOB_MAX && v.size() == kBaslikBoyu + len &&
                   crc32(v.data() + kBaslikBoyu, len) == crc) {
            Blok b;
            b.veri.assign(v.begin() + static_cast<std::ptrdiff_t>(kBaslikBoyu), v.end());
            b.gen = gen;
            b.crc = crc;
            blok_ = std::move(b);
            blok_tamam = true;
        } else {
            blok_bozuk = true;
        }
    } else {
        blok_bozuk = true;
    }
    if (!sayac_tamam) {
        return Yukleme::SayacBozuk;
    }
    return (blok_bozuk || !blok_tamam) ? Yukleme::BlokBozuk : Yukleme::Tamam;
}

std::vector<std::uint8_t> Depo::kodla(const std::optional<Blok>& b) const
{
    std::vector<std::uint8_t> v(kSihir.begin(), kSihir.end());
    le32_yaz(v, kSurum);
    le32_yaz(v, sayac_.has_value() ? 1u : 0u);
    le32_yaz(v, sayac_.value_or(0u));
    le32_yaz(v, crc32(v.data(), 20u));
    const std::uint32_t len = b.has_value() ? static_cast<std::uint32_t>(b->veri.size()) : 0u;
    le32_yaz(v, b.has_value() ? 1u : 0u);
    le32_yaz(v, b.has_value() ? b->gen : 0u);
    le32_yaz(v, len);
    le32_yaz(v, b.has_value() ? b->crc : 0u);
    le32_yaz(v, crc32(v.data() + kSayacSonu, 16u));
    if (b.has_value()) {
        v.insert(v.end(), b->veri.begin(), b->veri.end());
    }
    return v;
}

bool Depo::atomik_yaz(const std::vector<std::uint8_t>& icerik, bool* dogrulama_hatasi) const
{
    *dogrulama_hatasi = false;
    const std::filesystem::path hedef(yol_);
    std::filesystem::path gecici = hedef;
    gecici += ".yeni";
    {
        std::ofstream f(gecici, std::ios::binary | std::ios::trunc);
        if (!f) {
            return false;
        }
        f.write(reinterpret_cast<const char*>(icerik.data()),
                static_cast<std::streamsize>(icerik.size()));
        f.flush();
        if (!f) {
            return false;
        }
    }
    std::vector<std::uint8_t> geri;
    std::error_code ec;
    if (!dosya_oku(gecici.string(), geri) || geri != icerik) {
        *dogrulama_hatasi = true;
        std::filesystem::remove(gecici, ec);
        return false;
    }
    std::filesystem::rename(gecici, hedef, ec);   // aynı dizinde: eskinin yerine atomik
    if (ec) {
        std::filesystem::remove(gecici, ec);
        return false;
    }
    return true;
}

Depo::Acilis Depo::acilis_ayir()
{
    Acilis a;
    if (!sayac_.has_value() || *sayac_ >= IDA_BOOT_COUNT_MAX) {
        return a;                                  // geçersiz ya da tükendi (A4-K86)
    }
    const std::uint32_t yeni = *sayac_ + 1u;
    sayac_ = yeni;
    if (dosyali()) {
        bool dogrulama = false;
        if (!atomik_yaz(kodla(blok_), &dogrulama)) {
            return a;                              // ayrılamadı: bu açılışta olay yok
        }
    }
    a.boot_count = yeni;
    a.ok = true;
    return a;
}

Depo::KayitSonucu Depo::kaydet(const std::uint8_t* veri, std::uint32_t len, std::uint32_t gen)
{
    KayitSonucu s;
    if (len > IDA_PARAM_BLOB_MAX || (veri == nullptr && len > 0u)) {
        s.store = static_cast<std::uint8_t>(IDA_PSTORE_FAIL);
        s.fail = static_cast<std::uint8_t>(IDA_PSFAIL_TOO_BIG);
        return s;
    }
    Blok b;
    if (len > 0u) {
        b.veri.assign(veri, veri + len);
    }
    b.gen = gen;
    b.crc = crc32(b.veri.data(), b.veri.size());
    std::optional<Blok> aday(std::move(b));
    if (dosyali()) {
        bool dogrulama = false;
        if (!atomik_yaz(kodla(aday), &dogrulama)) {
            s.store = static_cast<std::uint8_t>(IDA_PSTORE_FAIL);
            s.fail = static_cast<std::uint8_t>(dogrulama ? IDA_PSFAIL_VERIFY : IDA_PSFAIL_FLASH);
            return s;
        }
    }
    blok_ = std::move(aday);
    s.store = static_cast<std::uint8_t>(IDA_PSTORE_OK);
    s.fail = static_cast<std::uint8_t>(IDA_PSFAIL_NONE);
    return s;
}

void Depo::kalici_blok(std::uint32_t* crc, std::uint32_t* len, std::uint32_t* gen) const
{
    *crc = blok_.has_value() ? blok_->crc : 0u;
    *len = blok_.has_value() ? static_cast<std::uint32_t>(blok_->veri.size()) : 0u;
    *gen = blok_.has_value() ? blok_->gen : 0u;
}

}  // namespace ida::sil
