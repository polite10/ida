// W0-MP3a masaüstü testleri: sahte hat. PC tarafı gerçek PosixPort okuma/yazma kodunu bir socketpair ucunda
// kullanır; diğer uçta bir iş parçacığı KARTIN TEST MANTIĞINI (ortak/mp3_kart.c, kartta koşan kodun aynısı)
// koşar. Hata enjeksiyonu: küçük soket tamponu ve rastgele küçük parçalarla kısmi okuma/yazma, bayt çevirme,
// bayt düşürme, hız uyuşmazlığında bozulma, kopma, kartın susması, donanım sayacı enjeksiyonu.
#pragma once

#include "seri.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <set>
#include <thread>

namespace mp3test {

struct HataAyar {
    uint32_t parca_azami = 0;          // >0: öykünücü her adımda en çok 1..parca_azami bayt okur/yazar (kısmi IO)
    int64_t pk_cevir = -1;             // KOSU'dan sonra PC->kart yönünde bu sıradaki baytı çevir (bit 0)
    int64_t kp_cevir = -1;             // kart->PC yönünde
    int64_t kp_dusur = -1;             // kart->PC yönünde bu sıradaki baytı düşür
    int64_t pk_dusur = -1;
    int64_t kopma_kp = -1;             // KOSU'dan sonra kart->PC bu kadar bayttan sonra hattı kapat
    bool kosuda_sus = false;           // KOSU'ya girince kart hiçbir şey işlemez/göndermez (özet gelmez)
    bool kosuda_okuma_dur = false;     // KOSU'ya girince öykünücü PC'den okumayı bırakır (PC yazması tıkanır)
    std::set<uint32_t> ret_baud;       // kartın baud_hesapla'sı bunları reddeder
    std::set<uint32_t> os_ret_baud;    // PC tarafı hız kancası bunları reddeder (işletim sistemi reddi gibi)
    bool kart_hiz_degistirmez = false; // kart yeni hıza geçmiş gibi davranır ama eski hızda kalır (uyuşmazlık)
    uint32_t enjekte_ore = 0;          // KOSU'da donanım sayacı olarak bildirilir
    uint32_t enjekte_tasma = 0;
    int64_t pc_yazma_gecikme_us = 0;   // >0: PC'nin her yazma çağrısı bu kadar gecikir (yavaş gönderici)
    int64_t pc_yazma_gecikme_sonra = 0; // gecikme yalnız bu kadar yazma çağrısından sonra başlar
    int64_t kp_gecikme_us = 0;         // KOSU'da kartın her çıkışı PC'ye bu kadar geç ulaşır (gerçek bekleyen kuyruğu)
};

class SahteHat {
public:
    explicit SahteHat(const HataAyar &h);
    ~SahteHat();
    mp3::Port &port() { return *port_; }
    uint64_t kosu_sayisi() const { return kosu_girisi_.load(); }

private:
    void dongu();
    HataAyar h_;
    int fd_kart_ = -1;
    std::unique_ptr<mp3::Port> port_;
    std::atomic<uint32_t> pc_baud_{0};
    std::atomic<bool> dur_{false};
    std::atomic<uint64_t> kosu_girisi_{0};
    std::thread t_;
};

}  // namespace mp3test
