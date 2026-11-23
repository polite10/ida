// test_destek.cpp — sim/sil testlerinin ortak yardımcıları (test_destek.hpp).
#include "test_destek.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace ida::sil::test {

namespace {
using Saat = std::chrono::steady_clock;
}

int& hata_sayisi()
{
    static int n = 0;
    return n;
}

bool kontrol(bool kosul, const char* ifade, const char* dosya, int satir)
{
    if (!kosul) {
        std::fprintf(stderr, "%s:%d: KONTROL tutmadı: %s\n", dosya, satir, ifade);
        ++hata_sayisi();
    }
    return kosul;
}

// ---- Sahte sim -----------------------------------------------------------------------------

SahteSim::SahteSim(tasima::Soket s) : soket_(std::move(s))
{
    (void)ida_link_gonderici_kur(&gon_, IDA_LINK_ROL_SIM);
    (void)ida_link_alici_kur(&al_, IDA_LINK_ROL_SIM);
    iplik_ = std::thread([this] { okuyucu(); });
}

SahteSim::~SahteSim()
{
    kapat();
}

void SahteSim::okumayi_durdur()
{
    dur_.store(true);
    if (iplik_.joinable()) {
        iplik_.join();
    }
}

bool SahteSim::bayt_gonder_durdurulabilir(const std::uint8_t* p, std::size_t n,
                                          const std::atomic<bool>* dur)
{
    return soket_.yaz(p, n, dur);
}

void SahteSim::kapat()
{
    dur_.store(true);
    if (iplik_.joinable()) {
        iplik_.join();
    }
    soket_.kapat();
}

void SahteSim::okuyucu()
{
    std::vector<std::uint8_t> b(4096);
    while (!dur_.load()) {
        const long long n = soket_.oku(b.data(), b.size(), 20);
        if (n == 0) {
            continue;
        }
        if (n < 0) {
            kapandi_.store(true);
            cv_.notify_all();
            return;
        }
        std::lock_guard<std::mutex> k(mu_);
        const auto un = static_cast<std::size_t>(n);
        alinan_.insert(alinan_.end(), b.begin(), b.begin() + static_cast<std::ptrdiff_t>(un));
        std::size_t i = 0;
        while (i < un) {
            Cerceve c;
            std::size_t t = 0;
            const ida_link_rc_t rc = ida_link_al(&al_, b.data() + i, un - i, &t, &c.ham);
            if (rc == IDA_LINK_CERCEVE) {
                cerceveler_.push_back(c);
            }
            if (rc != IDA_LINK_CERCEVE || t == 0u) {
                break;
            }
            i += t;
        }
        cv_.notify_all();
    }
}

std::vector<std::uint8_t> SahteSim::kodla(std::uint32_t msgid, const void* m, std::size_t boy)
{
    std::vector<std::uint8_t> b(IDA_LINK_CERCEVE_AZAMI);
    std::size_t n = 0;
    if (ida_link_gonder(&gon_, msgid, m, boy, b.data(), b.size(), &n) != IDA_LINK_TAMAM) {
        (void)kontrol(false, "sahte sim çerçeveyi kodlayamadı", __FILE__, __LINE__);
        return {};
    }
    b.resize(n);
    return b;
}

void SahteSim::mesaj_gonder(std::uint32_t msgid, const void* m, std::size_t boy)
{
    const std::vector<std::uint8_t> b = kodla(msgid, m, boy);
    bayt_gonder(b.data(), b.size());
}

void SahteSim::bayt_gonder(const std::uint8_t* p, std::size_t n)
{
    gonderilen_.insert(gonderilen_.end(), p, p + n);
    (void)soket_.yaz(p, n);
}

bool SahteSim::bekle(std::uint32_t msgid, Cerceve* c, int ms,
                     const std::function<bool(const Cerceve&)>& kosul)
{
    std::unique_lock<std::mutex> k(mu_);
    const auto son = Saat::now() + std::chrono::milliseconds(ms);
    for (;;) {
        for (std::size_t i = imlec_; i < cerceveler_.size(); ++i) {
            const Cerceve& x = cerceveler_[i];
            if (x.msgid() == msgid && (!kosul || kosul(x))) {
                if (c != nullptr) {
                    *c = x;
                }
                imlec_ = i + 1u;
                return true;
            }
        }
        if (kapandi_.load() || cv_.wait_until(k, son) == std::cv_status::timeout) {
            // Son bir tarama: kapanmadan önce gelenler.
            for (std::size_t i = imlec_; i < cerceveler_.size(); ++i) {
                const Cerceve& x = cerceveler_[i];
                if (x.msgid() == msgid && (!kosul || kosul(x))) {
                    if (c != nullptr) {
                        *c = x;
                    }
                    imlec_ = i + 1u;
                    return true;
                }
            }
            return false;
        }
    }
}

std::vector<Cerceve> SahteSim::cerceveler() const
{
    std::lock_guard<std::mutex> k(mu_);
    return cerceveler_;
}

std::vector<std::uint8_t> SahteSim::alinan() const
{
    std::lock_guard<std::mutex> k(mu_);
    return alinan_;
}

bool SahteSim::bayt_bekle(std::size_t n, int ms)
{
    std::unique_lock<std::mutex> k(mu_);
    const auto son = Saat::now() + std::chrono::milliseconds(ms);
    while (alinan_.size() < n && !kapandi_.load()) {
        if (cv_.wait_until(k, son) == std::cv_status::timeout) {
            break;
        }
    }
    return alinan_.size() >= n;
}

bool SahteSim::kapanmayi_bekle(int ms)
{
    std::unique_lock<std::mutex> k(mu_);
    const auto son = Saat::now() + std::chrono::milliseconds(ms);
    while (!kapandi_.load()) {
        if (cv_.wait_until(k, son) == std::cv_status::timeout) {
            break;
        }
    }
    return kapandi_.load();
}

// ---- Koşucu ipliği -------------------------------------------------------------------------

KosucuIpligi::KosucuIpligi(Ayarlar a, Kancalar k) : k_(std::move(a), std::move(k))
{
    t_ = std::thread([this] {
        cikis_ = k_.calistir();
        bitti_.store(true);
    });
}

KosucuIpligi::~KosucuIpligi()
{
    (void)bitir();
}

bool KosucuIpligi::bitmesini_bekle(int ms)
{
    const auto son = Saat::now() + std::chrono::milliseconds(ms);
    while (!bitti_.load() && Saat::now() < son) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (bitti_.load() && t_.joinable()) {
        t_.join();
    }
    return bitti_.load();
}

Cikis KosucuIpligi::bitir()
{
    k_.durdur();
    if (t_.joinable()) {
        t_.join();
    }
    return cikis_;
}

Ayarlar test_ayari(std::uint16_t port)
{
    Ayarlar a;
    a.a1_port = port;
    a.gunluk = 0;
    a.bekleme_ms = 3000;
    a.deneme_araligi_ms = 20;
    return a;
}

bool duzen_kur(Duzen* d, const std::function<void(Ayarlar&)>& ayarla, Kancalar k)
{
    std::string hata;
    d->dinleyici = tasima::Dinleyici::ac(0, &hata);
    if (!KONTROL(d->dinleyici.gecerli())) {
        std::fprintf(stderr, "dinleyici: %s\n", hata.c_str());
        return false;
    }
    Ayarlar a = test_ayari(d->dinleyici.port());
    if (ayarla) {
        ayarla(a);
    }
    d->kosucu = std::make_unique<KosucuIpligi>(a, std::move(k));
    tasima::Soket s = d->dinleyici.kabul(5000);
    if (!KONTROL(s.gecerli())) {
        return false;
    }
    d->sim = std::make_unique<SahteSim>(std::move(s));
    return true;
}

bool yeniden_kabul(Duzen* d)
{
    tasima::Soket s = d->dinleyici.kabul(5000);
    if (!KONTROL(s.gecerli())) {
        return false;
    }
    d->sim = std::make_unique<SahteSim>(std::move(s));
    return true;
}

// ---- Mesajlar ------------------------------------------------------------------------------

bool el_sikis(SahteSim& sim, std::uint32_t nonce, std::uint16_t sess,
              mavlink_ida_board_hello_t* hello)
{
    mavlink_ida_sim_hello_t h{};
    h.dialect_hash = IDA_LINK_DIALECT_HASH;
    h.nonce = nonce;
    h.run_id = 0x3f25d322u;
    h.ctrl_period_ns = 5000000u;
    h.proto_major = 1u;
    sim.gonder(MAVLINK_MSG_ID_IDA_SIM_HELLO, h);
    Cerceve c;
    if (!KONTROL(sim.bekle(MAVLINK_MSG_ID_IDA_BOARD_HELLO, &c, 5000, [nonce](const Cerceve& x) {
            return x.coz<mavlink_ida_board_hello_t>().peer_nonce == nonce;
        }))) {
        return false;
    }
    const auto b = c.coz<mavlink_ida_board_hello_t>();
    if (hello != nullptr) {
        *hello = b;
    }
    mavlink_ida_session_accept_t a{};
    a.run_id = h.run_id;
    a.nonce_sim = nonce;
    a.nonce_board = b.nonce;
    a.sess = sess;
    a.result = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_OK);
    a.run_mode = static_cast<std::uint8_t>(IDAW_RUN_MODE_SIL);
    sim.gonder(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, a);
    if (!KONTROL(sim.bekle(MAVLINK_MSG_ID_IDA_KEEPALIVE, nullptr, 5000, [sess](const Cerceve& x) {
            const auto k = x.coz<mavlink_ida_keepalive_t>();
            return k.sess == sess && k.state == static_cast<std::uint8_t>(IDAW_LINK_STATE_RUNNING);
        }))) {
        return false;
    }
    sim.sess = sess;
    return true;
}

void PartiKurucu::imu(std::uint32_t seq, std::int64_t t)
{
    mavlink_ida_imu_t m{};
    m.t_meas_ns = t;
    m.seq = seq;
    m.window_ns = 5000000u;
    m.accel[2] = -9.81f;
    m.temp_c = 20.0f;
    m.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
    sim_.gonder(MAVLINK_MSG_ID_IDA_IMU, m);
    ++uye_;
    imu_var_ = true;
}

void PartiKurucu::baro(std::uint32_t seq, std::int64_t t)
{
    mavlink_ida_baro_t m{};
    m.t_meas_ns = t;
    m.seq = seq;
    m.window_ns = 2000000u;
    m.p_abs = 101325.0f;
    m.temp_c = 18.0f;
    m.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
    sim_.gonder(MAVLINK_MSG_ID_IDA_BARO, m);
    ++uye_;
}

void PartiKurucu::mag(std::uint32_t seq, std::int64_t t)
{
    mavlink_ida_mag_t m{};
    m.t_meas_ns = t;
    m.seq = seq;
    m.field[0] = 2.0e-5f;
    m.field[2] = 4.0e-5f;
    m.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
    sim_.gonder(MAVLINK_MSG_ID_IDA_MAG, m);
    ++uye_;
}

void PartiKurucu::simple(std::uint32_t cmd_id, std::uint8_t type, std::uint16_t arg)
{
    mavlink_ida_cmd_simple_t m{};
    m.cmd_id = cmd_id;
    m.sess = sim_.sess;
    m.arg = arg;
    m.type = type;
    m.source = 255u;
    sim_.gonder(MAVLINK_MSG_ID_IDA_CMD_SIMPLE, m);
    ++uye_;
}

void PartiKurucu::param_set(std::uint32_t cmd_id, std::uint16_t id, std::uint8_t type,
                            std::uint32_t bits)
{
    mavlink_ida_cmd_param_set_t m{};
    m.cmd_id = cmd_id;
    m.value_bits = bits;
    m.sess = sim_.sess;
    m.param_id = id;
    m.param_type = type;
    m.source = 255u;
    sim_.gonder(MAVLINK_MSG_ID_IDA_CMD_PARAM_SET, m);
    ++uye_;
}

void PartiKurucu::guided(std::uint32_t cmd_id, std::uint8_t mask, float depth, float yaw)
{
    mavlink_ida_cmd_guided_t m{};
    m.cmd_id = cmd_id;
    m.depth = depth;
    m.yaw = yaw;
    m.sess = sim_.sess;
    m.mask = mask;
    m.source = 255u;
    sim_.gonder(MAVLINK_MSG_ID_IDA_CMD_GUIDED, m);
    ++uye_;
}

void PartiKurucu::emniyet(std::uint32_t seq, std::int64_t t, std::uint32_t rev)
{
    mavlink_ida_safety_t m{};
    m.t_meas_ns = t;
    m.seq = seq;
    m.safety_rev = rev;
    m.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
    m.estop = 0u;
    m.tube[0] = static_cast<std::uint8_t>(IDA_TUBE_LOADED);
    m.tube[1] = static_cast<std::uint8_t>(IDA_TUBE_LOADED);
    sim_.gonder(MAVLINK_MSG_ID_IDA_SAFETY, m);
    ++uye_;
}

void PartiKurucu::batarya(std::uint32_t seq, std::int64_t t)
{
    mavlink_ida_batt_t m{};
    m.t_meas_ns = t;
    m.seq = seq;
    m.window_ns = 1000000u;
    m.voltage = 16.0f;
    m.current = 2.0f;
    m.temp_c = 25.0f;
    m.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
    sim_.gonder(MAVLINK_MSG_ID_IDA_BATT, m);
    ++uye_;
}

void PartiKurucu::sizinti(std::uint32_t seq, std::int64_t t)
{
    mavlink_ida_leak_t m{};
    m.t_meas_ns = t;
    m.seq = seq;
    m.mask = 0u;
    m.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
    sim_.gonder(MAVLINK_MSG_ID_IDA_LEAK, m);
    ++uye_;
}

void PartiKurucu::algilama(std::uint32_t batch_seq, std::uint32_t det_seq, std::int64_t t)
{
    mavlink_ida_det_batch_t b{};
    b.t_meas_ns = t;
    b.batch_seq = batch_seq;
    b.window_ns = 1000000u;
    b.source = 1u;
    b.count = 1u;
    sim_.gonder(MAVLINK_MSG_ID_IDA_DET_BATCH, b);
    ++uye_;
    mavlink_ida_det_record_t r{};
    r.t_meas_ns = t;
    r.det_seq = det_seq;
    r.batch_seq = batch_seq;
    r.window_ns = 1000000u;
    r.confidence = 0.5f;
    r.source = 1u;
    r.batch_count = 1u;
    sim_.gonder(MAVLINK_MSG_ID_IDA_DET_RECORD, r);
    ++uye_;
}

void PartiKurucu::ping360(std::uint32_t seq, std::int64_t t)
{
    mavlink_ida_ping360_data_t d{};
    d.seq = seq;
    d.frag_idx = 0u;
    for (std::size_t i = 0; i < sizeof d.data; ++i) {
        d.data[i] = static_cast<std::uint8_t>(i & 0x7Fu);
    }
    sim_.gonder(MAVLINK_MSG_ID_IDA_PING360_DATA, d);
    ++uye_;
    mavlink_ida_ping360_head_t h{};
    h.t_meas_ns = t;
    h.seq = seq;
    h.angle_grad = static_cast<std::uint16_t>(seq % 400u);
    h.n_samples = 240u;
    h.sample_period = 100u;
    h.tx_duration_us = 50u;
    h.tx_freq_khz = 750u;
    h.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
    h.gain = 1u;
    h.n_frag = 1u;
    sim_.gonder(MAVLINK_MSG_ID_IDA_PING360_HEAD, h);
    ++uye_;
}

void PartiKurucu::tf(std::uint32_t seq, std::int64_t t, bool imu_var)
{
    mavlink_ida_time_frame_t m{};
    m.t_sim_ns = t;
    m.batch_seq = seq;
    m.sess = sim_.sess;
    m.n_members = uye_;
    m.tf_flags = static_cast<std::uint8_t>(imu_var ? IDAW_TF_FLAGS_IMU : 0);
    m.run_mode = static_cast<std::uint8_t>(IDAW_RUN_MODE_SIL);
    sim_.gonder(MAVLINK_MSG_ID_IDA_TIME_FRAME, m);
    uye_ = 0u;
    imu_var_ = false;
}

bool parti(SahteSim& sim, std::uint32_t seq, Cerceve* act,
           const std::function<void(PartiKurucu&)>& ek)
{
    const std::int64_t t = parti_zamani(seq);
    PartiKurucu p(sim);
    p.imu(seq, t);
    if (seq % 4u == 0u) {
        p.baro(seq / 4u, t - 1000000);
    }
    if (seq % 8u == 0u) {
        p.mag(seq / 8u, t - 2000000);
    }
    if (seq % 4u == 2u) {
        p.batarya(seq / 4u, t - 1500000);
        p.sizinti(seq / 4u, t - 1500000);
    }
    if (ek) {
        ek(p);
    }
    p.tf(seq, t, true);
    return sim.bekle(MAVLINK_MSG_ID_IDA_ACTUATOR, act, 5000, [seq](const Cerceve& x) {
        return x.coz<mavlink_ida_actuator_t>().batch_seq == seq;
    });
}

bool sonuc_bekle(SahteSim& sim, std::uint32_t cmd_id, mavlink_ida_cmd_result_t* r, int ms)
{
    const auto son = Saat::now() + std::chrono::milliseconds(ms);
    while (Saat::now() < son) {
        for (const Cerceve& c : sim.cerceveler()) {
            if (c.msgid() != MAVLINK_MSG_ID_IDA_CMD_RESULT) {
                continue;
            }
            const auto x = c.coz<mavlink_ida_cmd_result_t>();
            if (x.cmd_id == cmd_id && x.ack != static_cast<std::uint8_t>(IDA_ACK_IN_PROGRESS)) {
                if (r != nullptr) {
                    *r = x;
                }
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

bool son_durum(const SahteSim& sim, mavlink_ida_status_t* s)
{
    bool var = false;
    for (const Cerceve& c : sim.cerceveler()) {
        if (c.msgid() == MAVLINK_MSG_ID_IDA_STATUS) {
            *s = c.coz<mavlink_ida_status_t>();
            var = true;
        }
    }
    return var;
}

}  // namespace ida::sil::test
