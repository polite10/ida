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

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

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

// Kalıcı yazım için en küçük platform katmanı (A4-K84): yaz, aygıta kadar kalıcılaştır, kapat.
#if defined(_WIN32)
class Dosya {
public:
    ~Dosya() { (void)kapat(); }
    bool ac(const std::filesystem::path& p)
    {
        h_ = ::CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
        return h_ != INVALID_HANDLE_VALUE;
    }
    bool yaz(const std::vector<std::uint8_t>& v)
    {
        std::size_t i = 0;
        while (i < v.size()) {
            const std::size_t kalan = v.size() - i;
            const DWORD parca = static_cast<DWORD>(kalan > 0x100000u ? 0x100000u : kalan);
            DWORD yazilan = 0;
            if (::WriteFile(h_, v.data() + i, parca, &yazilan, nullptr) == 0 || yazilan == 0u) {
                return false;
            }
            i += yazilan;
        }
        return true;
    }
    bool kalicilastir() { return ::FlushFileBuffers(h_) != 0; }
    bool kapat()
    {
        if (h_ == INVALID_HANDLE_VALUE) {
            return true;
        }
        const bool ok = ::CloseHandle(h_) != 0;
        h_ = INVALID_HANDLE_VALUE;
        return ok;
    }

private:
    HANDLE h_ = INVALID_HANDLE_VALUE;
};

bool yeniden_adlandir(const std::filesystem::path& eski, const std::filesystem::path& yeni)
{
    return ::MoveFileExW(eski.c_str(), yeni.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool dizini_kalicilastir(const std::filesystem::path&)
{
    return true;   // MOVEFILE_WRITE_THROUGH: adlandırma diske yazılınca döner
}
#else
// macOS'ta fsync veriyi sürücüye verir ama sürücünün önbelleğinden kalıcı ortama yazılmasını
// beklemez; F_FULLFSYNC bunu ister. Desteklemeyen dosya sisteminde fsync'e düşülür.
bool fd_kalicilastir(int fd)
{
#if defined(F_FULLFSYNC)
    if (::fcntl(fd, F_FULLFSYNC) == 0) {
        return true;
    }
#endif
    for (;;) {
        if (::fsync(fd) == 0) {
            return true;
        }
        if (errno != EINTR) {
            return false;
        }
    }
}

class Dosya {
public:
    ~Dosya() { (void)kapat(); }
    bool ac(const std::filesystem::path& p)
    {
        fd_ = ::open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        return fd_ >= 0;
    }
    bool yaz(const std::vector<std::uint8_t>& v)
    {
        std::size_t i = 0;
        while (i < v.size()) {
            const ssize_t r = ::write(fd_, v.data() + i, v.size() - i);
            if (r < 0 && errno == EINTR) {
                continue;
            }
            if (r <= 0) {
                return false;
            }
            i += static_cast<std::size_t>(r);
        }
        return true;
    }
    bool kalicilastir() { return fd_kalicilastir(fd_); }
    bool kapat()
    {
        if (fd_ < 0) {
            return true;
        }
        const bool ok = ::close(fd_) == 0;
        fd_ = -1;
        return ok;
    }

private:
    int fd_ = -1;
};

bool yeniden_adlandir(const std::filesystem::path& eski, const std::filesystem::path& yeni)
{
    return ::rename(eski.c_str(), yeni.c_str()) == 0;   // aynı dizinde, eskinin yerine atomik
}

// Adlandırmanın kendisi dizin girdisidir: dizin de kalıcılaştırılır.
bool dizini_kalicilastir(const std::filesystem::path& hedef)
{
    std::filesystem::path dizin = hedef.parent_path();
    if (dizin.empty()) {
        dizin = ".";
    }
    const int fd = ::open(dizin.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    bool ok = false;
    for (;;) {
        if (::fsync(fd) == 0) {
            ok = true;
            break;
        }
        if (errno != EINTR) {
            break;
        }
    }
    (void)::close(fd);
    return ok;
}
#endif

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

// Adım izine ekler; sınamanın enjekte ettiği adımda false (adım yapılmadan başarısız sayılır).
bool Depo::adim(Adim a)
{
    iz_.push_back(a);
    return !hata_.has_value() || *hata_ != a;
}

bool Depo::atomik_yaz(const std::vector<std::uint8_t>& icerik, bool* dogrulama_hatasi)
{
    *dogrulama_hatasi = false;
    const std::filesystem::path hedef(yol_);
    std::filesystem::path gecici = hedef;
    gecici += ".yeni";
    std::error_code ec;
    const auto vazgec = [&gecici, &ec]() {
        std::filesystem::remove(gecici, ec);
        return false;
    };
    // (1) geçici dosyaya yaz, (2) kalıcılaştır, kapat.
    Dosya f;
    if (!adim(Adim::Yazma) || !f.ac(gecici) || !f.yaz(icerik)) {
        f.kapat();
        return vazgec();
    }
    if (!adim(Adim::Kalicilastirma) || !f.kalicilastir()) {
        f.kapat();
        return vazgec();
    }
    if (!f.kapat()) {
        return vazgec();
    }
    // (3) geri okuyup karşılaştır: tutmazsa eski dosya yerinde kalır.
    std::vector<std::uint8_t> geri;
    if (!adim(Adim::Dogrulama) || !dosya_oku(gecici.string(), geri) || geri != icerik) {
        *dogrulama_hatasi = true;
        return vazgec();
    }
    // (4) eskinin yerine adlandır (POSIX rename atomik; Windows MoveFileExW write-through).
    if (!adim(Adim::YenidenAdlandirma) || !yeniden_adlandir(gecici, hedef)) {
        return vazgec();
    }
    // (5) dizin girdisini kalıcılaştır (POSIX). Başarısızsa yeni içerik yerindedir ama
    // kalıcılığı güvencede değildir: OK verilmez (A4-K84).
    if (!adim(Adim::DizinKaliciligi) || !dizini_kalicilastir(hedef)) {
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
