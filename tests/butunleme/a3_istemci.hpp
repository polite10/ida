// Bütünleme testleri için en küçük A3 WebSocket istemcisi (W4-bagla-a3): yalnız 127.0.0.1, A1
// taşımasının TCP istemcisi (kopru::tcp_baglan) üzerinde. Sözleşmenin istemci tarafından gerekeni
// yapar: el sıkışma (A3-K07 alt protokolleri), `merhaba`, `abone_ol`, ikili iletilerin A3
// çerçevelerine ayrılması (§3.2), Ping'e Pong. Panel kitaplığının yerini tutmaz (o Node testinde).
#pragma once

#include "ida/kopru/tasima.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ida::test::a3 {

struct FizikOrnegi {
    std::uint64_t adim_no = 0;
    std::int64_t t_ns = 0;
    double derinlik = 0.0;   // govdeler[0].p_n[2]
};

class Istemci {
public:
    // El sıkışma ve `merhaba` (en çok bekle_ms).
    bool baglan(std::uint16_t port, const std::string& jeton, int bekle_ms = 3000);
    // `abone_ol` (A3-K30): (kanal, hz; hz ≤ 0: varsayılan).
    bool abone(const std::vector<std::pair<std::string, int>>& kanallar);
    // Gelenleri en çok bekle_ms bekleyerek okur ve işler; bağlantı kapandıysa false.
    bool oku(int bekle_ms);
    void kapat();

    std::map<std::uint16_t, std::uint64_t> cerceve_sayisi;   // kanal_id → çerçeve
    std::vector<FizikOrnegi> fizik;                          // fizik/durum (kanal 10)
    std::vector<std::string> metinler;                       // metin iletileri (merhaba, abonelik_yaniti, …)
    std::vector<std::string> olaylar;                        // olay kanalının ayrıntıları
    std::vector<std::uint16_t> olay_turleri;
    std::uint16_t kapanis_kodu = 0;
    bool kapandi = false;

private:
    bool cerceve_isle();
    bool yaz_(std::uint8_t opkod, const std::string& yuk);
    void a3_ayir(const std::string& ileti);

    std::unique_ptr<ida::kopru::Tasima> t_;
    std::vector<std::uint8_t> okuma_ = std::vector<std::uint8_t>(65536);   // yığında değil (MSVC C6262)
    std::string tampon_;
    std::string parca_;   // parçalı ileti
    std::uint8_t parca_opkod_ = 0;
    int istek_no_ = 1;
};

}  // namespace ida::test::a3
