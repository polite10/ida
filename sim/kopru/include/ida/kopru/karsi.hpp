// Süreç içi karşı taraf (W4-kopru): ida_link üst katmanı + gerçek çekirdek, bir taşıma ucu
// üzerinde. Köprünün sınamasında ve `ida_sim --karsi ic` kipinde kartın (ya da kartsız
// koşucunun) yerine geçer. Ürün değildir; ürün karşı taraflar `ida_sil` (sim/sil) ve kart
// yazılımıdır (firmware/).
//
// İki rol:
//   SIL  saat sim zamanıdır: son alınan zaman çerçevesinin t_sim_ns'i (A4-K11, A1-K121);
//        parti üyeleri önceki TF'nin zamanıyla damgalanır; ızgara tick'i, T_bağ ve duyuru yok.
//        Belirlenimcidir: çıktı yalnız alınan bayt akışına bağlıdır.
//   KART saat bir süreğen saattir (duvar); ızgara tick'i, T_bağ ve duyuru var (A1-K11, K117).
//        HIL kipinin kartsız sınaması (sözde uçbirim ya da TCP) içindir.
#pragma once

#include "ida/kopru/tasima.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ida::kopru {

enum class KarsiRol { sil, kart };

struct KarsiAyari {
    KarsiRol rol = KarsiRol::sil;
    std::uint64_t a1_tohum = 0;            // --a1-seed karşılığı (A1-K114)
    std::uint32_t onceki_acilis = 0;       // ilk boot_count = onceki_acilis + 1
    std::vector<std::uint8_t> param_blok;  // kalıcı başlangıç bloğu (boşsa blok yok)
    std::uint32_t param_gen = 0;
    std::uint32_t n_bg = 1;                // A1-K121 (iii)
    std::uint16_t esc_alt_us = 1100, esc_notr_us = 1500, esc_ust_us = 1900;
    std::uint32_t t_bag_ns = 500'000'000u;
    // Yalnız sınama: BOARD_HELLO'daki dialect_hash'i değiştirmek mümkün değildir (derlenmiş);
    // yanlış dialect sınaması köprünün beklenen değeriyle yapılır.
};

struct KarsiIstatistik {
    std::uint64_t cagri_step = 0, cagri_tick = 0, cagri_izgara = 0, cagri_oturum_sonu = 0;
    std::uint64_t yeniden_kurulum = 0;
    std::uint64_t kayit_ok = 0;
    std::uint32_t boot_count = 0;
    std::uint8_t init_rc = 0;
    std::uint64_t pencere_ihlali = 0;
    // Son çağrının çıktısından.
    std::uint32_t sample_rejected = 0, sample_future = 0, imu_rejected = 0;
    // VALID bayraklı olduğu halde reddedilen örnek (çağrı başına artış − o çağrıdaki VALID = 0
    // örnek sayısı; alt sınır). 0 olmalı: zaman damgası ve sıra kabul kapısından geçer.
    std::uint64_t gecerli_red = 0;
};

class KarsiTaraf {
public:
    KarsiTaraf(std::unique_ptr<Tasima> tasima, const KarsiAyari& ayar);
    ~KarsiTaraf();
    KarsiTaraf(const KarsiTaraf&) = delete;
    KarsiTaraf& operator=(const KarsiTaraf&) = delete;

    // Çekirdeği ve ida_link'i kurar. false: hata (metin hata()).
    bool kur();
    // Eldeki baytları işler (beklemez), çağrıları yapar, çıktıyı taşımaya yazar. SIL'de
    // süreç içi kullanımda köprü her yazımdan sonra bunu çağırır. Dönüş: taşıma açık mı.
    bool pompala();
    // KART rolü: `dur` true olana kadar döner (ayrı iş parçacığı için); bekleme ≤ 0,2 ms.
    void calistir(const std::function<bool()>& dur);
    // Taşımayı değiştirir (yeni bağlantı; A1-K99).
    void tasima_degistir(std::unique_ptr<Tasima> t);

    const KarsiIstatistik& istatistik() const;
    std::string hata() const;
    std::uint16_t sess() const;

private:
    struct Ic;
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::kopru
