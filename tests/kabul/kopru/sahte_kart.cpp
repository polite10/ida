#include "sahte_kart.hpp"

#include <array>
#include <cstring>

#include "ida/algilama_kaydi.h"
#include "ida/port.h"

namespace kabul {

SahteKart::SahteKart(const Ayar& ayar) : ayar_(ayar) {
    ida_link_alici_kur(&alici_, IDA_LINK_ROL_KART);
    ida_link_gonderici_kur(&gonderici_, IDA_LINK_ROL_KART);
}

std::vector<std::uint8_t> SahteKart::kodla(std::uint32_t msgid, const void* m, std::size_t boy) {
    std::vector<std::uint8_t> b(IDA_LINK_CERCEVE_AZAMI);
    std::size_t yazilan = 0;
    const ida_link_rc_t rc =
        ida_link_gonder(&gonderici_, msgid, m, boy, b.data(), b.size(), &yazilan);
    if (rc != IDA_LINK_TAMAM) {
        b.clear();
        return b;
    }
    b.resize(yazilan);
    return b;
}

void SahteKart::gonder(std::uint32_t msgid, const void* m, std::size_t boy, std::int64_t t_birak) {
    Giden g;
    g.t_birak = t_birak;
    g.bayt = kodla(msgid, m, boy);
    if (!g.bayt.empty()) {
        giden.push_back(std::move(g));
    }
}

void SahteKart::ham_gonder(std::vector<std::uint8_t> b, std::int64_t t_birak) {
    Giden g;
    g.t_birak = t_birak;
    g.bayt = std::move(b);
    giden.push_back(std::move(g));
}

mavlink_ida_event_t SahteKart::olay(std::uint8_t kind, std::uint8_t tube, const GelenTf& tf,
                                    std::uint32_t hold_ns) {
    mavlink_ida_event_t e{};
    ++olay_sayaci_;
    e.event_id = (static_cast<std::uint64_t>(ayar_.boot_count) << 32) | olay_sayaci_;
    e.t_decide_ns = tf.t;
    e.batch_seq = tf.seq;
    e.apply_by_ns = ayar_.apply_by_ns;
    e.hold_ns = (kind == w::kOlaySafetyOpen) ? hold_ns : 0u;
    e.safety_rev = son_rev_;
    e.sess = sess_;
    e.kind = kind;
    e.tube = tube;
    e.call_kind = w::kCagriStep;
    return e;
}

mavlink_ida_actuator_t SahteKart::yanit(const GelenTf& tf, std::uint16_t pwm,
                                        std::uint8_t n_events) {
    mavlink_ida_actuator_t a{};
    a.batch_seq = tf.seq;
    a.valid_for_ns = ayar_.valid_for_ns;
    for (auto& p : a.pwm_us) {
        p = pwm;
    }
    a.sess = sess_;
    a.act_flags = static_cast<std::uint16_t>(w::kThrValid | w::kArmed);
    a.fs_level = 0;
    a.core_rc = w::kRcOk;
    a.n_events = n_events;
    return a;
}

mavlink_ida_actuator_t SahteKart::gecersiz_yanit(const GelenTf& tf, std::uint8_t n_events) {
    mavlink_ida_actuator_t a{};
    a.batch_seq = tf.seq;
    a.valid_for_ns = 0;
    a.sess = sess_;
    a.act_flags = 0;
    a.core_rc = w::kRcImuRejected;
    a.n_events = n_events;
    return a;
}

std::uint32_t SahteKart::yanit_gonder(mavlink_ida_actuator_t a, std::int64_t t_birak) {
    ++out_seq_;
    a.out_seq = out_seq_;
    gonder(MAVLINK_MSG_ID_IDA_ACTUATOR, &a, sizeof a, t_birak);
    return out_seq_;
}

void SahteKart::olay_gonder(const mavlink_ida_event_t& e, std::int64_t t_birak) {
    gonder(MAVLINK_MSG_ID_IDA_EVENT, &e, sizeof e, t_birak);
}

void SahteKart::keepalive_gonder(std::int64_t t_birak) {
    mavlink_ida_keepalive_t k{};
    k.sess = kosuyor_ ? sess_ : std::uint16_t{0};
    k.state = kosuyor_ ? w::kLinkRunning : std::uint8_t{0};
    gonder(MAVLINK_MSG_ID_IDA_KEEPALIVE, &k, sizeof k, t_birak);
}

void SahteKart::oturum_kapat_gonder(std::uint8_t reason, std::int64_t t_birak) {
    mavlink_ida_session_close_t c{};
    c.sess = sess_;
    c.reason = reason;
    gonder(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &c, sizeof c, t_birak);
    kosuyor_ = false;
}

void SahteKart::sifirlan(std::int64_t t) {
    ++yeniden_baslatma;
    ++ayar_.boot_count;
    kosuyor_ = false;
    el_sikismada_ = false;
    oturum_sayisi_ = 0;
    prev_sess_ = 0;
    son_sim_nonce_ = 0;
    out_seq_ = 0;
    olay_sayaci_ = 0;
    son_rev_ = 0;
    ++nonce_sayaci_;
    nonce_ = 0x4B000000u + nonce_sayaci_;
    board_hello_gonder(0, t);
}

void SahteKart::board_hello_gonder(std::uint32_t peer_nonce, std::int64_t t) {
    ida_core_info_t info{};
    ida_core_info(&info);
    mavlink_ida_board_hello_t h{};
    h.dialect_hash = IDA_LINK_DIALECT_HASH;
    h.param_schema_hash = info.param_schema_hash;
    h.peer_nonce = peer_nonce;
    h.nonce = nonce_;
    h.boot_count = ayar_.boot_count;
    h.core_version = info.core_version;
    h.param_blob_crc = ayar_.blok_crc;
    h.param_blob_len = ayar_.blok_len;
    h.param_gen = 1;
    h.ctrl_period_ns = 5'000'000u;
    h.port_major = info.port_major;
    h.port_minor = info.port_minor;
    h.det_schema_major = static_cast<std::uint16_t>(IDA_DET_SCHEMA_MAJOR);
    h.det_schema_minor = static_cast<std::uint16_t>(IDA_DET_SCHEMA_MINOR);
    h.prev_sess = prev_sess_;
    h.sessions_since_boot = oturum_sayisi_;
    h.proto_major = 1;
    h.proto_minor = 0;
    h.role = ayar_.rol;
    h.boot_count_ok = 1;
    h.init_rc = w::kRcOk;
    gonder(MAVLINK_MSG_ID_IDA_BOARD_HELLO, &h, sizeof h, t);
}

void SahteKart::al(const std::uint8_t* p, std::size_t n, std::int64_t t) {
    std::size_t konum = 0;
    while (konum < n) {
        std::size_t tuketilen = 0;
        ida_link_cerceve_t c{};
        const ida_link_rc_t rc = ida_link_al(&alici_, p + konum, n - konum, &tuketilen, &c);
        konum += tuketilen;
        if (rc == IDA_LINK_CERCEVE) {
            cerceve_isle(c, t);
        } else {
            break;
        }
    }
}

void SahteKart::cerceve_isle(const ida_link_cerceve_t& c, std::int64_t t) {
    switch (c.msgid) {
    case MAVLINK_MSG_ID_IDA_SIM_HELLO: {
        mavlink_ida_sim_hello_t h{};
        ida_link_coz(&c, c.msgid, &h, sizeof h);
        ++sim_hello_sayisi;
        if (!el_sikisma_yanitla) {
            break;
        }
        if (h.nonce == son_sim_nonce_ && kosuyor_) {
            break;  // A1-K13: aynı nonce, RUNNING: yok say
        }
        if (h.nonce == son_sim_nonce_ && el_sikismada_) {
            board_hello_gonder(h.nonce, t);  // aynı BOARD_HELLO'yu yinele
            break;
        }
        kosuyor_ = false;
        son_sim_nonce_ = h.nonce;
        ++nonce_sayaci_;
        nonce_ = 0x4B000000u + nonce_sayaci_;
        el_sikismada_ = true;
        board_hello_gonder(h.nonce, t);
        break;
    }
    case MAVLINK_MSG_ID_IDA_SESSION_ACCEPT: {
        mavlink_ida_session_accept_t a{};
        ida_link_coz(&c, c.msgid, &a, sizeof a);
        kabuller.push_back(a);
        if (!el_sikismada_ || a.nonce_sim != son_sim_nonce_ || a.nonce_board != nonce_) {
            break;
        }
        el_sikismada_ = false;
        if (a.result == w::kOturumOk) {
            kosuyor_ = true;
            sess_ = a.sess;
            prev_sess_ = a.sess;
            ++oturum_sayisi_;
            son_rev_ = 0;  // A1-K106: SESSION_START'ta 0
            acik = Parti{};
            keepalive_gonder(t);
        } else if (a.result == w::kOturumReboot) {
            // A1-K17: planlı yeniden başlatma; yeni açılış sayacı, oturum sayısı 0, duyuru.
            sifirlan(t);
        }
        break;
    }
    case MAVLINK_MSG_ID_IDA_SESSION_CLOSE: {
        mavlink_ida_session_close_t k{};
        ida_link_coz(&c, c.msgid, &k, sizeof k);
        if (kosuyor_ && k.sess == sess_) {
            ++oturum_kapanis_alindi;
            kosuyor_ = false;
        }
        break;
    }
    case MAVLINK_MSG_ID_IDA_TIME_FRAME: {
        mavlink_ida_time_frame_t f{};
        ida_link_coz(&c, c.msgid, &f, sizeof f);
        if (!kosuyor_ || f.sess != sess_) {
            acik = Parti{};
            break;
        }
        acik.tf.seq = f.batch_seq;
        acik.tf.t = f.t_sim_ns;
        acik.tf.sess = f.sess;
        acik.tf.n_members = f.n_members;
        acik.tf.flags = f.tf_flags;
        acik.tf.run_mode = f.run_mode;
        partiler.push_back(acik);
        acik = Parti{};
        if (tf_geldi) {
            const GelenTf g = partiler.back().tf;
            tf_geldi(g, t);
        }
        break;
    }
    case MAVLINK_MSG_ID_IDA_SAFETY: {
        mavlink_ida_safety_t s{};
        ida_link_coz(&c, c.msgid, &s, sizeof s);
        ++acik.uye;
        acik.emniyet.push_back(s);
        if (kosuyor_) {
            son_rev_ = s.safety_rev;
        }
        break;
    }
    case MAVLINK_MSG_ID_IDA_EVENT_RESULT: {
        mavlink_ida_event_result_t r{};
        ida_link_coz(&c, c.msgid, &r, sizeof r);
        ++acik.uye;
        acik.sonuc.push_back(r);
        sonuclar.push_back(r);
        break;
    }
    case MAVLINK_MSG_ID_IDA_CMD_SIMPLE: {
        mavlink_ida_cmd_simple_t s{};
        ida_link_coz(&c, c.msgid, &s, sizeof s);
        ++acik.uye;
        GelenKomut k;
        k.msgid = c.msgid;
        k.cmd_id = s.cmd_id;
        k.sess = s.sess;
        k.type = s.type;
        komutlar.push_back(k);
        if (komut_geldi) {
            komut_geldi(k, t);
        }
        break;
    }
    case MAVLINK_MSG_ID_IDA_CMD_PARAM_SET: {
        mavlink_ida_cmd_param_set_t s{};
        ida_link_coz(&c, c.msgid, &s, sizeof s);
        ++acik.uye;
        GelenKomut k;
        k.msgid = c.msgid;
        k.cmd_id = s.cmd_id;
        k.sess = s.sess;
        k.param_id = s.param_id;
        k.param_type = s.param_type;
        k.value_bits = s.value_bits;
        komutlar.push_back(k);
        if (komut_geldi) {
            komut_geldi(k, t);
        }
        break;
    }
    case MAVLINK_MSG_ID_IDA_CMD_GUIDED:
    case MAVLINK_MSG_ID_IDA_CMD_MANUAL:
    case MAVLINK_MSG_ID_IDA_CMD_FC:
    case MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM: {
        // cmd_id ilk alandır (uint32, tel sırasında başta değilse de bilgi için yeterli değil);
        // bu vakalarda yalnız sayılır.
        ++acik.uye;
        GelenKomut k;
        k.msgid = c.msgid;
        komutlar.push_back(k);
        if (komut_geldi) {
            komut_geldi(k, t);
        }
        break;
    }
    case MAVLINK_MSG_ID_IDA_KEEPALIVE:
        break;
    default:
        ++acik.uye;  // sensör ve diğer üyeler
        break;
    }
}

std::optional<mavlink_ida_event_result_t> SahteKart::sonuc(std::uint64_t event_id) const {
    for (const auto& r : sonuclar) {
        if (r.event_id == event_id) {
            return r;
        }
    }
    return std::nullopt;
}

std::size_t SahteKart::sonuc_sayisi(std::uint64_t event_id) const {
    std::size_t n = 0;
    for (const auto& r : sonuclar) {
        if (r.event_id == event_id) {
            ++n;
        }
    }
    return n;
}

}  // namespace kabul
