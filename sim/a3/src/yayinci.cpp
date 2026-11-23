// yayinci.cpp — dünya iş parçacığı tarafı (kaynak.hpp). Bellek ayırmaz, kilit almaz.
#include "yayinci_ic.hpp"

#include "ida/a3/katalog.hpp"
#include "kodlayici.hpp"

#include <cstring>

namespace ida::a3 {

namespace {

void yaz_u16(std::uint8_t* p, std::uint16_t v) noexcept { std::memcpy(p, &v, 2); }
void yaz_u32(std::uint8_t* p, std::uint32_t v) noexcept { std::memcpy(p, &v, 4); }
void yaz_u64(std::uint8_t* p, std::uint64_t v) noexcept { std::memcpy(p, &v, 8); }
void yaz_i64(std::uint8_t* p, std::int64_t v) noexcept { std::memcpy(p, &v, 8); }

}  // namespace

Yayinci::Yayinci(std::size_t telemetri_bayt, std::size_t olay_bayt)
    : ic_(std::make_unique<Ic>(telemetri_bayt, olay_bayt))
{
}

Yayinci::~Yayinci() = default;

bool Yayinci::yaz_(std::uint16_t kanal_indeks, std::uint64_t anahtar, std::int64_t t_ns, std::size_t boy,
                   KodlaIslevi kodlayici, const void* yuk) noexcept
{
    Ic& ic = *ic_;
    auto dusme_kaydet = [&ic, kanal_indeks, anahtar]() noexcept {
        ic.telemetri_dusen.fetch_add(1, std::memory_order_relaxed);
        for (auto& d : ic.dusmeler) {
            if (d.kanal == kanal_indeks && d.anahtar == anahtar) {
                return;
            }
        }
        for (auto& d : ic.dusmeler) {
            if (d.kanal == 0xFFFFu) {
                d.kanal = kanal_indeks;
                d.anahtar = anahtar;
                return;
            }
        }
        if (static_cast<std::size_t>(kanal_indeks) < ic.kanal_genel_dusme.size()) {
            ic.kanal_genel_dusme[kanal_indeks] = true;
        }
    };
    if (boy > kYukAzamiBayt) {
        ic.kodlama_hatasi.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::uint8_t* p = ic.telemetri.ayir(kTelemetriBaslik + boy);
    if (p == nullptr) {
        dusme_kaydet();
        return false;
    }
    std::size_t yazilan = 0;
    if (!kodlayici(yuk, p + kTelemetriBaslik, boy, yazilan) || yazilan != boy) {
        // Ayrılan yer yayınlanmaz; sonraki `ayir` aynı yeri yeniden verir.
        ic.kodlama_hatasi.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::uint16_t bayrak = 0;
    for (auto& d : ic.dusmeler) {
        if (d.kanal == kanal_indeks && d.anahtar == anahtar) {
            d.kanal = 0xFFFFu;
            bayrak = 1;
        }
    }
    if (static_cast<std::size_t>(kanal_indeks) < ic.kanal_genel_dusme.size() && ic.kanal_genel_dusme[kanal_indeks]) {
        ic.kanal_genel_dusme[kanal_indeks] = false;
        bayrak = 1;
    }
    yaz_u16(p, kanal_indeks);
    yaz_u16(p + 2, bayrak);
    yaz_u32(p + 4, 0);
    yaz_u64(p + 8, anahtar);
    yaz_i64(p + 16, t_ns);
    ic.telemetri.yayinla();
    return true;
}

bool Yayinci::olay(std::int64_t t_ns, const yuk_olay& o) noexcept
{
    Ic& ic = *ic_;
    const std::size_t boy = boyut(o);
    if (boy > kYukAzamiBayt) {
        ic.kodlama_hatasi.fetch_add(1, std::memory_order_relaxed);
        ic.olay_dusen.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::uint8_t* p = ic.olaylar.ayir(kOlayBaslik + boy);
    if (p == nullptr) {
        ic.olay_dusen.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::size_t yazilan = 0;
    if (!kodla(o, p + kOlayBaslik, boy, yazilan) || yazilan != boy) {
        ic.kodlama_hatasi.fetch_add(1, std::memory_order_relaxed);
        ic.olay_dusen.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::memset(p, 0, 8);
    yaz_i64(p + 8, t_ns);
    ic.olaylar.yayinla();
    return true;
}

bool Yayinci::komut_sonucu(const KomutSonucuGirdisi& s) noexcept
{
    Ic& ic = *ic_;
    if (s.hata_kodu.size() > 64 || s.sonuc_ayrinti_json.size() > 8192) {
        ic.kodlama_hatasi.fetch_add(1, std::memory_order_relaxed);
        ic.olay_dusen.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const std::size_t boy = kSonucBaslik + s.hata_kodu.size() + s.sonuc_ayrinti_json.size();
    std::uint8_t* p = ic.olaylar.ayir(boy);
    if (p == nullptr) {
        ic.olay_dusen.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    p[0] = 1;
    p[1] = static_cast<std::uint8_t>(s.uygulandi ? 1u : 0u);
    p[2] = static_cast<std::uint8_t>(s.uygulama_adimi_var ? 1u : 0u);
    p[3] = 0;
    yaz_u32(p + 4, s.komut_no);
    yaz_i64(p + 8, s.t_ns);
    yaz_u64(p + 16, s.uygulama_adimi);
    yaz_u64(p + 24, s.adim_no);
    yaz_u16(p + 32, static_cast<std::uint16_t>(s.hata_kodu.size()));
    yaz_u16(p + 34, 0);
    yaz_u32(p + 36, static_cast<std::uint32_t>(s.sonuc_ayrinti_json.size()));
    if (!s.hata_kodu.empty()) {
        std::memcpy(p + kSonucBaslik, s.hata_kodu.data(), s.hata_kodu.size());
    }
    if (!s.sonuc_ayrinti_json.empty()) {
        std::memcpy(p + kSonucBaslik + s.hata_kodu.size(), s.sonuc_ayrinti_json.data(), s.sonuc_ayrinti_json.size());
    }
    ic.olaylar.yayinla();
    return true;
}

std::uint64_t Yayinci::halka_tasma() const noexcept
{
    return ic_->telemetri_dusen.load(std::memory_order_relaxed) + ic_->olay_dusen.load(std::memory_order_relaxed);
}

std::uint64_t Yayinci::kodlama_hatasi() const noexcept
{
    return ic_->kodlama_hatasi.load(std::memory_order_relaxed);
}

}  // namespace ida::a3
