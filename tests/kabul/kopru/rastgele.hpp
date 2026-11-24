// tests/kabul/kopru — sabit tohumlu kart → sim çerçeve üreteci (dayanıklılık vakaları).
//
// Çerçeve türleri (olasılıklar yaklaşık): geçerli şablondan alan değiştirilmiş, CRC'si doğru
// IDA_EVENT (%30) ve IDA_ACTUATOR (%20); şablon çerçevede bayt çevrilmiş (çoğunlukla CRC'si
// yanlış; %15); kesik çerçeve (%10); 0xFD serpiştirilmiş çöp bayt (%10); tamamen rastgele yüklü
// ama CRC'si doğru kart → sim mesajı (%15; oturum denetimi mesajları hariç).
#pragma once

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "sahte_kart.hpp"

namespace kabul {

class RastgeleCerceve {
public:
    // olaylar = false: CRC'si doğru IDA_EVENT üretilmez (yerine IDA_ACTUATOR); geçerli olay
    // yağmuru sonuç kuyruğunu doldurup oturumu bitirdiğinde (A1-K111, H43) sürekli komut yolu
    // daha uzun sınanır.
    explicit RastgeleCerceve(std::uint32_t tohum, bool olaylar = true) : r_(tohum), olaylar_(olaylar) {}

    std::uint32_t u32() { return static_cast<std::uint32_t>(r_()); }
    std::uint64_t u64() { return (static_cast<std::uint64_t>(u32()) << 32) | u32(); }
    std::uint32_t aralik(std::uint32_t n) { return n == 0 ? 0u : u32() % n; }
    std::uint8_t u8() { return static_cast<std::uint8_t>(u32() & 0xFFu); }
    std::uint16_t u16() { return static_cast<std::uint16_t>(u32() & 0xFFFFu); }

    // Kartın güncel durumundan (sess, açılış, son TF, son revizyon) bir çerçeve üretir.
    std::vector<std::uint8_t> uret(SahteKart& kart, const GelenTf& tf) {
        const std::uint32_t z = aralik(100);
        if (z < 30) {
            return olaylar_ ? olay(kart, tf) : yanit(kart, tf);
        }
        if (z < 50) {
            return yanit(kart, tf);
        }
        if (z < 65) {
            auto b = (aralik(2) == 0) ? olay(kart, tf) : yanit(kart, tf);
            const std::uint32_t n = 1 + aralik(3);
            for (std::uint32_t i = 0; i < n && !b.empty(); ++i) {
                auto& x = b[aralik(static_cast<std::uint32_t>(b.size()))];
                x = static_cast<std::uint8_t>(x ^ (1u << aralik(8)));
            }
            return b;
        }
        if (z < 75) {
            auto b = (aralik(2) == 0) ? olay(kart, tf) : yanit(kart, tf);
            if (b.size() > 1) {
                b.resize(1 + aralik(static_cast<std::uint32_t>(b.size() - 1)));
            }
            return b;
        }
        if (z < 85) {
            std::vector<std::uint8_t> b(1 + aralik(80));
            for (auto& x : b) {
                x = u8();
                if (aralik(10) == 0) {
                    x = 0xFD;
                }
            }
            return b;
        }
        return rastgele_yuk(kart);
    }

private:
    std::vector<std::uint8_t> olay(SahteKart& kart, const GelenTf& tf) {
        static constexpr std::uint8_t kTurler[3] = {w::kOlaySafetyOpen, w::kOlaySafetyClose,
                                                    w::kOlayFire};
        const std::uint8_t tur = kTurler[aralik(3)];
        mavlink_ida_event_t e = kart.olay(tur, static_cast<std::uint8_t>(aralik(2)), tf,
                                          60'000'000u + aralik(390'000'000u));
        const std::uint32_t n = aralik(4);
        for (std::uint32_t i = 0; i < n; ++i) {
            switch (aralik(11)) {
            case 0: e.event_id = u64(); break;
            case 1: e.event_id ^= static_cast<std::uint64_t>(1u + aralik(4)) << 32; break;
            case 2: e.t_decide_ns = static_cast<std::int64_t>(u64() >> 2); break;
            case 3: e.batch_seq = e.batch_seq + aralik(5) - 2u; break;
            case 4: e.apply_by_ns = u32(); break;
            case 5: e.hold_ns = u32(); break;
            case 6: e.safety_rev = e.safety_rev + aralik(5) - 2u; break;
            case 7: e.sess = u16(); break;
            case 8: e.kind = static_cast<std::uint8_t>(aralik(6)); break;
            case 9: e.tube = (aralik(3) == 0) ? std::uint8_t{255} : static_cast<std::uint8_t>(aralik(4)); break;
            default: e.call_kind = static_cast<std::uint8_t>(aralik(6)); break;
            }
        }
        return kart.kodla(MAVLINK_MSG_ID_IDA_EVENT, &e, sizeof e);
    }

    std::vector<std::uint8_t> yanit(SahteKart& kart, const GelenTf& tf) {
        mavlink_ida_actuator_t a = kart.yanit(tf, static_cast<std::uint16_t>(1100u + aralik(801u)), 0);
        a.out_seq = kart.out_seq() + aralik(4);
        const std::uint32_t n = aralik(4);
        for (std::uint32_t i = 0; i < n; ++i) {
            switch (aralik(9)) {
            case 0: a.batch_seq = a.batch_seq + aralik(7) - 3u; break;
            case 1: a.valid_for_ns = u32(); break;
            case 2: a.pwm_us[aralik(8)] = u16(); break;
            case 3: a.sess = u16(); break;
            case 4: a.act_flags = u16(); break;
            case 5: a.core_rc = static_cast<std::uint8_t>(aralik(10)); break;
            case 6: a.n_events = static_cast<std::uint8_t>(aralik(6)); break;
            case 7: a.fs_level = static_cast<std::uint8_t>(aralik(8)); break;
            default: a.out_seq = u32(); break;
            }
        }
        if (!olaylar_) {
            // Olaysız kipte sonraki partinin yuvasını dolduran sahte yanıt olay beklemesin
            // (SIL kilitli adımı T_SIL boyunca bekler, A1-K73, K76).
            a.n_events = 0;
        }
        // Akıştaki out_seq'i yeniyse kartın sayacı onu izler (A1-K105): sonraki olağan yanıtlar
        // V5'te reddedilip SIL kilitli adımını T_SIL boyunca bekletmesin.
        const std::uint32_t fark = a.out_seq - kart.out_seq();
        if (fark != 0u && fark < 0x80000000u) {
            kart.out_seq_ata(a.out_seq);
        }
        return kart.kodla(MAVLINK_MSG_ID_IDA_ACTUATOR, &a, sizeof a);
    }

    std::vector<std::uint8_t> rastgele_yuk(SahteKart& kart) {
        static constexpr std::uint32_t kKimlikler[] = {
            MAVLINK_MSG_ID_IDA_ACTUATOR,   MAVLINK_MSG_ID_IDA_EVENT,      MAVLINK_MSG_ID_IDA_CMD_RESULT,
            MAVLINK_MSG_ID_IDA_NAV,        MAVLINK_MSG_ID_IDA_STATUS,     MAVLINK_MSG_ID_IDA_FC_STATUS,
            MAVLINK_MSG_ID_IDA_LOG,        MAVLINK_MSG_ID_IDA_CORE_COUNTERS,
            MAVLINK_MSG_ID_IDA_LINK_STATS, MAVLINK_MSG_ID_IDA_CALL_TRACE, MAVLINK_MSG_ID_IDA_FAULT_RECORD,
            MAVLINK_MSG_ID_IDA_KEEPALIVE,  MAVLINK_MSG_ID_IDA_DET_RECORD};
        const std::uint32_t kimlik =
            kKimlikler[aralik(static_cast<std::uint32_t>(sizeof kKimlikler / sizeof kKimlikler[0]))];
        ida_link_mesaj_bilgisi_t bilgi{};
        if (ida_link_mesaj_bilgisi(kimlik, &bilgi) != IDA_LINK_TAMAM) {
            return {};
        }
        std::vector<std::uint8_t> yuk(bilgi.yapi_boyu);
        for (auto& x : yuk) {
            x = u8();
        }
        return kart.kodla(kimlik, yuk.data(), yuk.size());
    }

    std::mt19937 r_;
    bool olaylar_ = true;
};

}  // namespace kabul
