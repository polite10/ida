// sahte_dunya.hpp — A2 sunucusunu sınamak için sahte veri kaynağı. FİZİK DEĞİLDİR: pozlar adım
// numarasının kapalı işlevleridir; değerler (dalga, ışık, su optiği) sınama için seçilmiş
// varsayımlardır, ölçüm ya da kalibrasyon değildir. Gerçek veri kaynağı `ida_sim`'in dünya
// döngüsüdür (W5-bagla-a2).
//
// Sahne: dairede dönen bir araç (iki tüpü dolu), `hedef_sayisi` sabit hedef; `firlatma_adimi`
// sınırında araç bir torpido bırakır (govde_dogdu + kutle_durumu + sureklilik_kir: aracın hızı
// sıçrar), `isabet_adimi` sınırında torpido ve ilk hedef ölür (isabet + iki govde_oldu).
// Bütün durumlar `dogru_*` işlevleriyle adım numarasından yeniden hesaplanabilir (sınamalar
// görselleştiricinin gördüğünü bununla karşılaştırır).
#pragma once

#include "ida/a2/yayinci.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ida::a2::sahte {

struct SahteAyar {
    std::string manifest_yolu = "/sahte/a2/manifest.json";
    std::array<std::uint8_t, 32> manifest_ozeti{};
    SimKipi sim_kipi = SimKipi::hil_1x;
    Kademe kademe_3b = Kademe::tam;
    std::uint32_t hedef_sayisi = 1;          // ≤ 62
    std::uint64_t firlatma_adimi = 0;        // 0: yok
    std::uint64_t isabet_adimi = 0;          // 0: yok (firlatma_adimi'nden büyük olmalı)
    std::uint16_t dalga_bileseni = 6;        // ≤ 256
    std::uint32_t kimlik_tabani = 0;         // gövde kimlikleri tabandan sonra verilir
};

inline constexpr std::uint16_t kVarlikArac = 0;
inline constexpr std::uint16_t kVarlikHedef = 1;
inline constexpr std::uint16_t kVarlikTorpido = 3;

class SahteDunya {
public:
    SahteDunya(Yayinci& y, SahteAyar a);

    // Çalıştırmayı (yeniden) başlatır: adım 0, yeni çalıştırma tanımı. false: yayıncı reddetti.
    bool basla();
    // `n` fizik adımı ilerler; her sınırda olayları ve adim_siniri'ni yayınlar.
    void adimla(std::uint64_t n);
    // Sıfırlama (A2-K29): zaman 0'a döner, yeni kimlikler, doğumlar ve ilk durum yayınlanır.
    void sifirla();

    std::uint64_t adim() const noexcept { return adim_; }
    const SahteAyar& ayar() const noexcept { return a_; }
    std::uint32_t arac_kimligi() const noexcept { return a_.kimlik_tabani + 1u; }
    std::uint32_t hedef_kimligi(std::uint32_t i) const noexcept { return a_.kimlik_tabani + 2u + i; }
    std::uint32_t torpido_kimligi() const noexcept { return a_.kimlik_tabani + 100u; }

    // Adım sınırındaki doğru durumlar (olaydan sonraki; A2-K21).
    GovdeAni dogru_arac(std::uint64_t adim_no) const;
    GovdeAni dogru_hedef(std::uint32_t i, std::uint64_t adim_no) const;
    GovdeAni dogru_torpido(std::uint64_t adim_no) const;
    bool torpido_yasiyor(std::uint64_t adim_no) const noexcept;
    bool hedef_yasiyor(std::uint32_t i, std::uint64_t adim_no) const noexcept;
    // Sınırda yaşayan bütün gövdelerin durumu (yayın sırasıyla). Dönüş: gövde sayısı.
    std::size_t dogru_anlar(std::uint64_t adim_no, std::array<GovdeAni, kAzamiGovde>& out) const;

    // Tanımın parçaları (sınamalar karşılaştırma için okur).
    const std::vector<DalgaBileseni>& bilesenler() const noexcept { return bilesenler_; }
    DalgaGirdisi dalga() const;
    Ortam ortam() const;

private:
    void olaylar_(std::uint64_t adim_no);

    Yayinci& y_;
    SahteAyar a_;
    std::vector<DalgaBileseni> bilesenler_;
    std::uint64_t adim_ = 0;
    // Yayın sırası (tablodaki sıra): doğum sırası; ölen çıkar.
    std::vector<std::uint32_t> yasayan_;
};

}  // namespace ida::a2::sahte
