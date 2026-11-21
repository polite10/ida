// A5'ten kurulan parametre bloğu (A4 §3.10; A1-K18, K19): çekirdeğin varsayılanları + verilen
// değişiklikler, yalnız çekirdeğin genel parametre arayüzüyle (ida_core_param_*). Sim, kartın
// bildirdiği param_blob_crc'yi bu bloğun CRC-32/ISO-HDLC değeriyle karşılaştırır; uyuşmazsa
// hazırlık oturumunda aynı değerleri PARAM_SET ile verir.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ida::kopru {

struct ParamDegeri {
    std::uint16_t id = 0;
    std::uint8_t type = 0;      // IDA_PT_*
    std::uint32_t bits = 0;     // değerin 32 biti
};

// Değişiklikler (ad, değer): değer parametrenin tipine çevrilir (F32 doğrudan; I32/U32 yuvarlanır).
// Bilinmeyen ad: boş blok ve hata metni.
struct ParamBlogu {
    std::vector<std::uint8_t> blok;
    std::vector<ParamDegeri> degisenler;   // varsayılandan farklı olanlar
    std::vector<ParamDegeri> tumu;         // hazırlık oturumunun PARAM_SET listesi (A1-K19), id sırasıyla
    std::uint32_t crc = 0;
    std::string hata;
};

ParamBlogu parametre_blogu(const std::vector<std::pair<std::string, double>>& degisiklikler);

}  // namespace ida::kopru
