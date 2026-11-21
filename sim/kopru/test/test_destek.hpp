// sim/kopru testlerinin ortak düzeni (sim/sensor/test ile aynı biçim): her test `int
// test_<ad>()`, 0 geçti. Sahte kart: tel katmanıyla kart rolünde çerçeve kurar ve çözer;
// yanıt ve olay zamanlamasını sınamanın denetlemesi içindir (gerçek çekirdek karsi.hpp'de).
#pragma once

#include "ida/kopru/karsi.hpp"
#include "ida/kopru/kopru.hpp"
#include "ida/kopru/param_blogu.hpp"
#include "ida/port.h"
#include "ida_link/tel.h"
#include "mavlink_ic.hpp"

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace ida::kopru::test {

struct Denetci {
    int hata = 0;
    void dogrula_v(const char* dosya, int satir, bool kosul, const std::string& ileti)
    {
        if (!kosul) {
            ++hata;
            if (hata <= 40) {
                std::printf("  KALDI %s:%d: %s\n", dosya, satir, ileti.c_str());
            }
        }
    }
};

#define DOGRULA(d, ...) (d).dogrula_v(__FILE__, __LINE__, __VA_ARGS__)

inline std::string s(std::uint64_t v) { return std::to_string(v); }

// Kart rolünde çerçeve üreten ve çözen sahte kart.
struct SahteKart {
    ida_link_alici_t alici{};
    ida_link_gonderici_t g{};
    std::vector<std::uint8_t> giden;
    std::uint16_t sess = 0;
    std::uint32_t boot_count = 1;
    std::uint16_t sessions_since_boot = 0;
    std::uint32_t param_blob_crc = 0;
    std::uint64_t dialect_hash = IDA_LINK_DIALECT_HASH;
    std::uint32_t out_seq = 0;
    std::uint32_t nonce = 0x1234u;
    std::uint32_t olay_ctr = 1;
    struct Tf {
        std::uint32_t seq;
        std::int64_t t;
        std::uint8_t n_members;
        std::uint8_t flags;
    };
    std::vector<Tf> tfler;
    std::vector<mavlink_ida_event_result_t> sonuclar;
    std::vector<mavlink_ida_safety_t> emniyet;
    std::vector<std::uint32_t> uye_msgid;   // TF dışındaki üyeler (son partiden beri)
    std::vector<std::vector<std::uint32_t>> parti_uyeleri;
    std::vector<mavlink_ida_cmd_simple_t> komutlar;
    bool kabul_reddi = false;
    std::uint8_t son_kabul = 0xFF;
    // TF gelince otomatik yanıt (true): THR_VALID, verilen darbe genişlikleri.
    bool oto_yanit = true;
    std::array<std::uint16_t, 8> pwm{1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};
    std::function<void(const Tf&)> tf_kancasi;

    SahteKart()
    {
        (void)ida_link_alici_kur(&alici, IDA_LINK_ROL_KART);
        (void)ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART);
    }

    template <typename T>
    void yaz(std::uint32_t msgid, const T& m)
    {
        std::uint8_t b[IDA_LINK_CERCEVE_AZAMI];
        std::size_t n = 0;
        if (ida_link_gonder(&g, msgid, &m, sizeof m, b, sizeof b, &n) == IDA_LINK_TAMAM) {
            giden.insert(giden.end(), b, b + n);
        }
    }

    void yanit(std::uint32_t seq, std::uint16_t bayrak, const std::array<std::uint16_t, 8>& p,
               std::uint8_t n_events = 0, std::uint8_t rc = 0, std::uint32_t valid_ns = 100'000'000u)
    {
        mavlink_ida_actuator_t a{};
        a.batch_seq = seq;
        a.valid_for_ns = (bayrak & IDAW_ACT_FLAGS_THR_VALID) != 0u ? valid_ns : 0u;
        a.out_seq = ++out_seq;
        for (std::size_t i = 0; i < 8; ++i) {
            a.pwm_us[i] = (bayrak & IDAW_ACT_FLAGS_THR_VALID) != 0u ? p[i] : 0u;
        }
        a.sess = sess;
        a.act_flags = bayrak;
        a.core_rc = rc;
        a.n_events = n_events;
        yaz(MAVLINK_MSG_ID_IDA_ACTUATOR, a);
    }

    // Olay: kimlik (boot_count << 32 | ctr).
    std::uint64_t olay(std::uint8_t kind, std::uint8_t tube, std::uint32_t seq, std::int64_t t_decide,
                       std::uint32_t rev, std::uint8_t call_kind = 1, std::uint64_t kimlik = 0)
    {
        mavlink_ida_event_t e{};
        e.event_id = kimlik != 0 ? kimlik : ((static_cast<std::uint64_t>(boot_count) << 32U) | olay_ctr++);
        e.t_decide_ns = t_decide;
        e.batch_seq = seq;
        e.apply_by_ns = 20'000'000u;
        e.hold_ns = kind == IDAW_EVENT_KIND_SAFETY_OPEN ? 200'000'000u : 0u;
        e.safety_rev = kind == IDAW_EVENT_KIND_SAFETY_CLOSE ? 0u : rev;
        e.sess = sess;
        e.kind = kind;
        e.tube = tube;
        e.call_kind = call_kind;
        yaz(MAVLINK_MSG_ID_IDA_EVENT, e);
        return e.event_id;
    }

    void al(const std::vector<std::uint8_t>& v)
    {
        const std::uint8_t* p = v.data();
        std::size_t n = v.size();
        while (n > 0) {
            std::size_t tuk = 0;
            ida_link_cerceve_t c;
            const ida_link_rc_t rc = ida_link_al(&alici, p, n, &tuk, &c);
            p += tuk;
            n -= tuk;
            if (rc != IDA_LINK_CERCEVE) {
                break;
            }
            isle(c);
        }
    }

    void isle(const ida_link_cerceve_t& c)
    {
        switch (c.msgid) {
        case MAVLINK_MSG_ID_IDA_SIM_HELLO: {
            mavlink_ida_sim_hello_t h{};
            (void)ida_link_coz(&c, c.msgid, &h, sizeof h);
            ida_core_info_t bilgi{};
            ida_core_info(&bilgi);
            mavlink_ida_board_hello_t b{};
            b.dialect_hash = dialect_hash;
            b.param_schema_hash = bilgi.param_schema_hash;
            b.peer_nonce = h.nonce;
            b.nonce = ++nonce;
            b.boot_count = boot_count;
            b.param_blob_crc = param_blob_crc;
            b.ctrl_period_ns = 5'000'000u;
            b.port_major = static_cast<std::uint16_t>(IDA_PORT_VERSION_MAJOR);
            b.port_minor = static_cast<std::uint16_t>(IDA_PORT_VERSION_MINOR);
            b.det_schema_major = 1;
            b.sessions_since_boot = sessions_since_boot;
            b.proto_major = 1;
            b.role = 2;
            b.boot_count_ok = 1;
            b.init_rc = 0;
            yaz(MAVLINK_MSG_ID_IDA_BOARD_HELLO, b);
            break;
        }
        case MAVLINK_MSG_ID_IDA_SESSION_ACCEPT: {
            mavlink_ida_session_accept_t a{};
            (void)ida_link_coz(&c, c.msgid, &a, sizeof a);
            son_kabul = a.result;
            if (a.result == 0u) {
                sess = a.sess;
                ++sessions_since_boot;
                mavlink_ida_keepalive_t k{};
                k.sess = sess;
                k.state = 2;
                yaz(MAVLINK_MSG_ID_IDA_KEEPALIVE, k);
            } else {
                kabul_reddi = true;
            }
            break;
        }
        case MAVLINK_MSG_ID_IDA_TIME_FRAME: {
            mavlink_ida_time_frame_t t{};
            (void)ida_link_coz(&c, c.msgid, &t, sizeof t);
            const Tf tf{t.batch_seq, t.t_sim_ns, t.n_members, t.tf_flags};
            tfler.push_back(tf);
            parti_uyeleri.push_back(uye_msgid);
            uye_msgid.clear();
            if (tf_kancasi) {
                tf_kancasi(tf);
            } else if (oto_yanit) {
                yanit(t.batch_seq, static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_ARMED), pwm);
            }
            break;
        }
        case MAVLINK_MSG_ID_IDA_EVENT_RESULT: {
            mavlink_ida_event_result_t r{};
            (void)ida_link_coz(&c, c.msgid, &r, sizeof r);
            sonuclar.push_back(r);
            uye_msgid.push_back(c.msgid);
            break;
        }
        case MAVLINK_MSG_ID_IDA_SAFETY: {
            mavlink_ida_safety_t r{};
            (void)ida_link_coz(&c, c.msgid, &r, sizeof r);
            emniyet.push_back(r);
            uye_msgid.push_back(c.msgid);
            break;
        }
        case MAVLINK_MSG_ID_IDA_CMD_SIMPLE: {
            mavlink_ida_cmd_simple_t r{};
            (void)ida_link_coz(&c, c.msgid, &r, sizeof r);
            komutlar.push_back(r);
            uye_msgid.push_back(c.msgid);
            break;
        }
        default:
            uye_msgid.push_back(c.msgid);
            break;
        }
    }
};

// Köprü ile sahte kart arasında bayt alışverişi (süreç içi, sim zamanı = duvar).
inline void alisveris(Kopru& k, SahteKart& sk, std::int64_t duvar)
{
    for (int tur = 0; tur < 8; ++tur) {
        std::vector<std::uint8_t> v;
        k.giden_al(v);
        if (!v.empty()) {
            sk.al(v);
        }
        if (sk.giden.empty() && v.empty()) {
            return;
        }
        if (!sk.giden.empty()) {
            std::vector<std::uint8_t> w;
            w.swap(sk.giden);
            k.al(w.data(), w.size(), duvar);
        }
    }
}

inline sensor::Olcum imu_olcumu(std::uint32_t seq, std::int64_t t);

// Köprü + sahte kart (zamanlama ve olay sınamaları).
struct SahteDuzenek {
    Kopru k{KopruAyari{}};
    SahteKart sk;
    std::uint64_t sinir = 0;
    std::int64_t t = 0;
    std::vector<SinirSonucu> sinirlar;   // dizin: sınır numarası
    std::function<void(std::uint64_t)> sinir_kancasi;   // sinir_isle'den önce

    SahteDuzenek()
    {
        sinirlar.push_back(k.sinir_isle(0, 0));
    }
    bool el_sikis()
    {
        k.el_sikisma_baslat(t);
        alisveris(k, sk, t);
        return k.durum() == OturumDurumu::kosuyor;
    }
    void sinir_ilerle()
    {
        ++sinir;
        t = static_cast<std::int64_t>(sinir) * kH;
        if (sinir_kancasi) {
            sinir_kancasi(sinir);
        }
        sinirlar.push_back(k.sinir_isle(sinir, t));
        alisveris(k, sk, t);
    }
    std::uint32_t periyot(const std::vector<sensor::Olcum>& ek = {})
    {
        for (int j = 0; j < 4; ++j) {
            sinir_ilerle();
        }
        for (const auto& o : ek) {
            k.olcum_ekle(o);
        }
        const std::uint32_t seq = k.parti_kapat(t, t);
        alisveris(k, sk, t);
        return seq;
    }
};

// Köprü + gerçek çekirdekli süreç içi karşı taraf (SIL, bellek taşıması).
struct IcDuzenek {
    std::unique_ptr<Kopru> k;
    std::unique_ptr<KarsiTaraf> karsi;
    std::unique_ptr<Tasima> sim_ucu;
    std::uint32_t imu_seq = 0;
    std::uint64_t sinir = 0;
    std::int64_t t = 0;

    explicit IcDuzenek(KopruAyari ka = {}, KarsiAyari kk = {})
    {
        ParamBlogu pb = parametre_blogu({{"YER_G", 9.82}});
        if (kk.param_blok.empty()) {
            kk.param_blok = pb.blok;
        }
        if (ka.beklenen_blok_crc == 0) {
            ka.beklenen_blok_crc = pb.crc;
        }
        k = std::make_unique<Kopru>(ka);
        kk.a1_tohum = k->board_tohumu();
        auto [a, b] = bellek_cifti();
        sim_ucu = std::move(a);
        karsi = std::make_unique<KarsiTaraf>(std::move(b), kk);
        (void)karsi->kur();
    }

    void pompala()
    {
        for (int tur = 0; tur < 16; ++tur) {
            std::vector<std::uint8_t> v;
            k->giden_al(v);
            if (!v.empty()) {
                (void)sim_ucu->yaz(v.data(), v.size());
            }
            (void)karsi->pompala();
            std::uint8_t b[8192];
            std::size_t n = 0;
            bool geldi = false;
            while (sim_ucu->oku(b, sizeof b, 0, n) == OkumaSonucu::tamam) {
                k->al(b, n, t);
                geldi = true;
            }
            if (!geldi && !k->giden_var()) {
                return;
            }
        }
    }

    bool el_sikis()
    {
        k->el_sikisma_baslat(t);
        pompala();
        return k->durum() == OturumDurumu::kosuyor;
    }

    // Bir kontrol periyodu: dört fizik sınırı, IMU, TF; SIL kilitli adım (yuva tamamlanana
    // kadar pompala). Dönüş: batch_seq.
    std::uint32_t periyot(SinirSonucu* son = nullptr)
    {
        SinirSonucu ss;
        for (int j = 0; j < 4; ++j) {
            ++sinir;
            t = static_cast<std::int64_t>(sinir) * kH;
            ss = k->sinir_isle(sinir, t);
        }
        if (son != nullptr) {
            *son = ss;
        }
        k->olcum_ekle(imu_olcumu(++imu_seq, t));
        const std::uint32_t seq = k->parti_kapat(t, t);
        pompala();
        return seq;
    }
};

// Sentetik IMU ölçümü (A4 kabul kapısından geçen değerler).
inline sensor::Olcum imu_olcumu(std::uint32_t seq, std::int64_t t)
{
    sensor::Olcum o;
    o.hdr.tur = sensor::SensorTuru::imu;
    o.hdr.seq = seq;
    o.hdr.t_meas_ns = t;
    o.hdr.window_ns = 5'000'000u;
    o.hdr.t_hazir_ns = t;
    o.hdr.t_teslim_ns = t;
    o.hdr.flags = sensor::kBayrakGecerli;
    sensor::ImuOlcumu y;
    y.accel = {0.0F, 0.0F, -9.82F};
    y.temp_c = 20.0F;
    o.yuk = y;
    return o;
}

}  // namespace ida::kopru::test
