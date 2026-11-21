// A4 §3.10 parametre bloğu (param_blogu.hpp). Bayt düzeni: magic (4), sürüm 1 (1) + dolgu (3),
// param_schema_hash (8), yük boyu (4), yük CRC-32 (4), yük: param_id artan sırayla öğe başına
// 8 bayt (id 2, tip 1, dolgu 1, değer 4); küçük uçlu (sim/sensor bütünleme testindeki kurulumla
// aynı).
#include "ida/kopru/param_blogu.hpp"

#include "ida/kopru/kopru.hpp"
#include "ida/port.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ida::kopru {
namespace {

void yaz32(std::vector<std::uint8_t>& b, std::size_t yer, std::uint32_t v)
{
    for (std::size_t i = 0; i < 4; ++i) {
        b[yer + i] = static_cast<std::uint8_t>((v >> (8U * i)) & 0xFFu);
    }
}

}  // namespace

ParamBlogu parametre_blogu(const std::vector<std::pair<std::string, double>>& degisiklikler)
{
    ParamBlogu sonuc;
    ida_core_info_t info{};
    ida_core_info(&info);
    const std::uint16_t n = ida_core_param_count();
    std::vector<ida_param_value_t> d;
    std::vector<bool> kullanildi(degisiklikler.size(), false);
    for (std::uint16_t i = 0; i < n; ++i) {
        ida_param_desc_t pd{};
        if (ida_core_param_desc(i, &pd) != IDA_RC_OK) {
            continue;
        }
        ida_param_value_t v = pd.def;
        v.id = pd.id;
        v.type = pd.type;
        for (std::size_t k = 0; k < degisiklikler.size(); ++k) {
            if (degisiklikler[k].first != pd.name) {
                continue;
            }
            kullanildi[k] = true;
            const double x = degisiklikler[k].second;
            if (pd.type == IDA_PT_F32) {
                v.v.f32 = static_cast<float>(x);
            } else if (pd.type == IDA_PT_I32) {
                v.v.i32 = static_cast<std::int32_t>(std::llround(x));
            } else {
                v.v.u32 = static_cast<std::uint32_t>(std::llround(x));
            }
            sonuc.degisenler.push_back({v.id, v.type, v.v.u32});
        }
        d.push_back(v);
    }
    for (std::size_t k = 0; k < degisiklikler.size(); ++k) {
        if (!kullanildi[k]) {
            sonuc.hata = "bilinmeyen parametre: " + degisiklikler[k].first;
            return sonuc;
        }
    }
    std::sort(d.begin(), d.end(), [](const ida_param_value_t& a, const ida_param_value_t& b) { return a.id < b.id; });
    for (const ida_param_value_t& v : d) {
        sonuc.tumu.push_back({v.id, v.type, v.v.u32});
    }
    const std::size_t yuk = 8u * d.size();
    std::vector<std::uint8_t> b(24u + yuk, 0u);
    yaz32(b, 0, IDA_PBLOB_MAGIC);
    b[4] = 1u;
    for (std::size_t i = 0; i < 8; ++i) {
        b[8 + i] = static_cast<std::uint8_t>((info.param_schema_hash >> (8U * i)) & 0xFFu);
    }
    yaz32(b, 16, static_cast<std::uint32_t>(yuk));
    for (std::size_t i = 0; i < d.size(); ++i) {
        const std::size_t y = 24u + 8u * i;
        b[y] = static_cast<std::uint8_t>(d[i].id & 0xFFu);
        b[y + 1] = static_cast<std::uint8_t>(d[i].id >> 8U);
        b[y + 2] = d[i].type;
        yaz32(b, y + 4, d[i].v.u32);
    }
    yaz32(b, 20, crc32_iso_hdlc(b.data() + 24, yuk));
    sonuc.crc = crc32_iso_hdlc(b.data(), b.size());
    sonuc.blok = std::move(b);
    return sonuc;
}

}  // namespace ida::kopru
