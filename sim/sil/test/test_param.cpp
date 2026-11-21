// test_param.cpp — kabul 3: parametre deposu. PARAM_SET → COMMIT → SAVE; REBOOT (A1-K17) ve
// programın yeniden başlatılması sonrası değer ve kuşak korunur; bozuk dosyada varsayılanlara
// düşülür ve init_rc PARAM_DEFAULTS'tur.
#include <cstdio>
#include <cstring>
#include <fstream>

#include "test_destek.hpp"
#include "turetme.hpp"

namespace ida::sil::test {

namespace {

constexpr unsigned u(unsigned x)
{
    return x;
}

constexpr std::uint64_t kTohum = UINT64_C(0x0123456789abcdef);

struct Aday {
    std::uint16_t id = 0;
    std::uint8_t type = 0;
    std::uint32_t yeni = 0;   // 32 bitlik ham değer
    std::uint32_t def = 0;
    char ad[IDA_PARAM_NAME_MAX + 1] = {};
};

std::uint32_t bitler(float f)
{
    std::uint32_t b = 0;
    std::memcpy(&b, &f, sizeof b);
    return b;
}

// Tanımdan, varsayılandan farklı ve aralık içi bir değer.
bool aday_kur(const ida_param_desc_t& d, Aday* a)
{
    a->id = d.id;
    a->type = d.type;
    std::memcpy(a->ad, d.name, sizeof a->ad);
    if (u(d.type) == IDA_PT_F32) {
        const float mn = d.min.v.f32, mx = d.max.v.f32, df = d.def.v.f32;
        if (!(mn < mx)) {
            return false;
        }
        const float v = (df < mx) ? df + (mx - df) * 0.25f : df - (df - mn) * 0.25f;
        if (!(v >= mn && v <= mx) || bitler(v) == bitler(df)) {
            return false;
        }
        a->yeni = bitler(v);
        a->def = bitler(df);
        return true;
    }
    if (u(d.type) == IDA_PT_U32) {
        if (d.min.v.u32 >= d.max.v.u32) {
            return false;
        }
        a->yeni = d.def.v.u32 < d.max.v.u32 ? d.def.v.u32 + 1u : d.def.v.u32 - 1u;
        a->def = d.def.v.u32;
        return true;
    }
    if (u(d.type) == IDA_PT_I32) {
        if (d.min.v.i32 >= d.max.v.i32) {
            return false;
        }
        const std::int32_t v = d.def.v.i32 < d.max.v.i32 ? d.def.v.i32 + 1 : d.def.v.i32 - 1;
        std::memcpy(&a->yeni, &v, sizeof v);
        std::memcpy(&a->def, &d.def.v.i32, sizeof a->def);
        return true;
    }
    return false;
}

std::uint32_t param_al(SahteSim& sim, std::uint32_t* seq, std::uint32_t cmd_id, std::uint16_t id)
{
    mavlink_ida_cmd_result_t r{};
    if (!KONTROL(parti(sim, (*seq)++, nullptr, [cmd_id, id](PartiKurucu& p) {
            p.simple(cmd_id, static_cast<std::uint8_t>(IDA_CMD_PARAM_GET), id);
        }))) {
        return 0u;
    }
    if (!KONTROL(sonuc_bekle(sim, cmd_id, &r))) {
        return 0u;
    }
    KONTROL(u(r.ack) == IDA_ACK_ACCEPTED && u(r.kind) == IDA_RES_PARAM_VALUE && r.param_id == id);
    return r.value_bits;
}

void dosya_baytini_boz(const std::filesystem::path& p, std::size_t i)
{
    std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
    f.seekg(static_cast<std::streamoff>(i));
    char c = 0;
    f.read(&c, 1);
    c = static_cast<char>(c ^ 0x55);
    f.seekp(static_cast<std::streamoff>(i));
    f.write(&c, 1);
}

}  // namespace

int test_param_deposu(const Baglam& b)
{
    const std::filesystem::path depo = b.gecici / "depo.bin";
    const auto ayarla = [&depo](Ayarlar& a) {
        a.depo_yolu = depo.string();
        a.a1_tohum = kTohum;
    };
    Aday aday;
    std::uint32_t save_gen = 0, save_crc = 0, save_len = 0;

    // ---- A: ilk açılış; PARAM_SET → COMMIT → SAVE; REBOOT ----
    {
        Duzen d;
        if (!duzen_kur(&d, ayarla)) {
            return hata_sayisi();
        }
        mavlink_ida_board_hello_t h{};
        if (!KONTROL(el_sikis(*d.sim, 0x1001u, 501u, &h))) {
            return hata_sayisi();
        }
        KONTROL(h.boot_count == 1u && h.boot_count_ok == 1u);
        KONTROL(u(h.init_rc) == IDA_RC_PARAM_DEFAULTS && h.param_blob_len == 0u);
        KONTROL(u(h.param_src) == IDA_PSRC_NONE);
        std::uint32_t seq = 1;
        for (; seq <= 5u; ++seq) {
            KONTROL(parti(*d.sim, seq));
        }
        // Commit'i kabul edilen ilk aday (bir parametre grubu denetimi reddederse sonraki).
        std::uint32_t cmd = 1;
        bool commit = false;
        const std::uint16_t n = ida_core_param_count();
        for (std::uint16_t i = 0; i < n && !commit; ++i) {
            ida_param_desc_t desc{};
            if (ida_core_param_desc(i, &desc) != IDA_RC_OK || !aday_kur(desc, &aday)) {
                continue;
            }
            const std::uint32_t c_set = cmd++, c_commit = cmd++;
            if (!KONTROL(parti(*d.sim, seq++, nullptr, [&](PartiKurucu& p) {
                    p.param_set(c_set, aday.id, aday.type, aday.yeni);
                    p.simple(c_commit, static_cast<std::uint8_t>(IDA_CMD_PARAM_COMMIT), 0u);
                }))) {
                return hata_sayisi();
            }
            mavlink_ida_cmd_result_t rs{}, rc{};
            if (!KONTROL(sonuc_bekle(*d.sim, c_set, &rs)) ||
                !KONTROL(sonuc_bekle(*d.sim, c_commit, &rc))) {
                return hata_sayisi();
            }
            commit = u(rs.ack) == IDA_ACK_ACCEPTED && u(rc.ack) == IDA_ACK_ACCEPTED;
        }
        if (!KONTROL(commit)) {
            return hata_sayisi();
        }
        std::printf("  parametre %u (%s): yeni değer 0x%08x, varsayılan 0x%08x\n",
                    static_cast<unsigned>(aday.id), aday.ad, static_cast<unsigned>(aday.yeni),
                    static_cast<unsigned>(aday.def));
        const std::uint32_t c_save = cmd++;
        KONTROL(parti(*d.sim, seq++, nullptr, [c_save](PartiKurucu& p) {
            p.simple(c_save, static_cast<std::uint8_t>(IDA_CMD_PARAM_SAVE), 0u);
        }));
        mavlink_ida_cmd_result_t rsv{};
        if (!KONTROL(sonuc_bekle(*d.sim, c_save, &rsv)) ||
            !KONTROL(u(rsv.ack) == IDA_ACK_ACCEPTED && u(rsv.kind) == IDA_RES_PARAM_SAVE)) {
            return hata_sayisi();
        }
        save_gen = rsv.save_gen;
        save_crc = rsv.save_crc;
        save_len = rsv.save_len;
        KONTROL(save_gen >= 1u && save_len > 0u);
        // Kaydın tamamlanması sonraki çağrılarda param_store = OK, param_gen = gen (A4-K83).
        for (int i = 0; i < 25; ++i) {
            KONTROL(parti(*d.sim, seq++));
        }
        mavlink_ida_status_t s{};
        if (KONTROL(son_durum(*d.sim, &s))) {
            KONTROL(u(s.param_store) == IDA_PSTORE_OK && s.param_gen == save_gen);
            KONTROL(s.param_blob_crc == save_crc);
            KONTROL(u(s.param_ok) == 1u);
        }
        KONTROL(std::filesystem::exists(depo));

        // REBOOT (A1-K17): yeni nonce'lu SIM_HELLO oturumu bitirir (A1-K13); SESSION_ACCEPT
        // REBOOT; koşucu çekirdeği yeni boot_count ve SIL_REBOOT ile kurar, blok depodan.
        mavlink_ida_sim_hello_t sh{};
        sh.dialect_hash = IDA_LINK_DIALECT_HASH;
        sh.nonce = 0x2002u;
        sh.run_id = 1u;
        sh.ctrl_period_ns = 5000000u;
        sh.proto_major = 1u;
        d.sim->gonder(MAVLINK_MSG_ID_IDA_SIM_HELLO, sh);
        Cerceve c;
        if (!KONTROL(d.sim->bekle(MAVLINK_MSG_ID_IDA_BOARD_HELLO, &c, 5000, [](const Cerceve& x) {
                return x.coz<mavlink_ida_board_hello_t>().peer_nonce == 0x2002u;
            }))) {
            return hata_sayisi();
        }
        mavlink_ida_session_accept_t sa{};
        sa.run_id = 1u;
        sa.nonce_sim = 0x2002u;
        sa.nonce_board = c.coz<mavlink_ida_board_hello_t>().nonce;
        sa.result = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REBOOT);
        sa.run_mode = static_cast<std::uint8_t>(IDAW_RUN_MODE_SIL);
        d.sim->gonder(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, sa);
        mavlink_ida_board_hello_t hr{};
        if (!KONTROL(el_sikis(*d.sim, 0x3003u, 502u, &hr))) {
            return hata_sayisi();
        }
        KONTROL(hr.boot_count == 2u && hr.boot_count_ok == 1u);
        KONTROL(u(hr.reset_cause) == IDA_RESET_SIL_REBOOT);
        KONTROL(hr.sessions_since_boot == 0u && hr.prev_sess == 0u);
        KONTROL(u(hr.init_rc) == IDA_RC_OK);
        KONTROL(hr.param_blob_crc == save_crc && hr.param_blob_len == save_len &&
                hr.param_gen == save_gen);
        KONTROL(hr.nonce == turetme::u32_sifirsiz(kTohum, turetme::nonce_board_etiketi(2u, 0u)));
        // Sim zamanı REBOOT'ta sürer (A4-K11): partiler kaldığı yerden devam eder; geri giden
        // zaman sim zamanının geri başlaması sayılırdı (test_geri_donus).
        const std::uint32_t son = seq + 3u;
        for (; seq < son; ++seq) {
            KONTROL(parti(*d.sim, seq));
        }
        KONTROL(param_al(*d.sim, &seq, 900u, aday.id) == aday.yeni);
        KONTROL(d.kosucu->bitir() == Cikis::Tamam);
        KONTROL(d.kosucu->istatistik().yeniden_kurulum == 1u);
        KONTROL(d.kosucu->istatistik().kayit_ok == 1u);
    }

    // ---- B: programın yeniden başlatılması: değer ve kuşak korunur ----
    {
        Duzen d;
        if (!duzen_kur(&d, ayarla)) {
            return hata_sayisi();
        }
        mavlink_ida_board_hello_t h{};
        if (!KONTROL(el_sikis(*d.sim, 0x4004u, 503u, &h))) {
            return hata_sayisi();
        }
        KONTROL(h.boot_count == 3u && h.boot_count_ok == 1u);
        KONTROL(u(h.reset_cause) == IDA_RESET_SIL_START);
        KONTROL(u(h.init_rc) == IDA_RC_OK && u(h.param_src) == IDA_PSRC_FLASH);
        KONTROL(h.param_blob_crc == save_crc && h.param_blob_len == save_len &&
                h.param_gen == save_gen);
        std::uint32_t seq = 1;
        KONTROL(parti(*d.sim, seq++));
        KONTROL(param_al(*d.sim, &seq, 901u, aday.id) == aday.yeni);
        // Yeni bir kayıt öncekinin kuşağından büyük kuşak alır.
        KONTROL(parti(*d.sim, seq++, nullptr, [](PartiKurucu& p) {
            p.simple(902u, static_cast<std::uint8_t>(IDA_CMD_PARAM_SAVE), 0u);
        }));
        mavlink_ida_cmd_result_t r{};
        if (KONTROL(sonuc_bekle(*d.sim, 902u, &r))) {
            KONTROL(u(r.ack) == IDA_ACK_ACCEPTED && r.save_gen > save_gen);
            save_gen = r.save_gen;
            save_crc = r.save_crc;
        }
        KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    }

    // ---- C: bloğu bozuk dosya: varsayılanlar, PARAM_DEFAULTS; sayaç geçerli ----
    {
        dosya_baytini_boz(depo, 44u + 30u);
        Duzen d;
        if (!duzen_kur(&d, ayarla)) {
            return hata_sayisi();
        }
        mavlink_ida_board_hello_t h{};
        if (!KONTROL(el_sikis(*d.sim, 0x5005u, 504u, &h))) {
            return hata_sayisi();
        }
        KONTROL(u(h.init_rc) == IDA_RC_PARAM_DEFAULTS);
        KONTROL(h.param_blob_len == 0u && h.param_blob_crc == 0u);
        KONTROL(h.boot_count == 4u && h.boot_count_ok == 1u);
        std::uint32_t seq = 1;
        KONTROL(parti(*d.sim, seq++));
        KONTROL(param_al(*d.sim, &seq, 903u, aday.id) == aday.def);
        KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    }

    // ---- D: tanınmayan dosya: varsayılanlar ve boot_count_ok = 0 (EMNİYET okuması) ----
    {
        {
            std::ofstream f(depo, std::ios::binary | std::ios::trunc);
            f << "bu bir depo dosyası değil";
        }
        Duzen d;
        if (!duzen_kur(&d, ayarla)) {
            return hata_sayisi();
        }
        mavlink_ida_board_hello_t h{};
        if (!KONTROL(el_sikis(*d.sim, 0x6006u, 505u, &h))) {
            return hata_sayisi();
        }
        KONTROL(u(h.init_rc) == IDA_RC_PARAM_DEFAULTS);
        KONTROL(h.boot_count_ok == 0u);
        KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    }
    return hata_sayisi();
}

}  // namespace ida::sil::test
