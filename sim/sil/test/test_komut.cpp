// test_komut.cpp — kabul 2: komut yolu (ARM, SET_MODE, DISARM) uçtan uca, sonuç mesajlarıyla.
//
// Görev modülü henüz bütünleştirilmedi: ARM ve SET_MODE'un kabul edilip edilmediği bu sınamada
// sabitlenmez; komutun çekirdeğe ulaştığı ve nihai sonucunun döndüğü denetlenir. Emniyet
// yönündeki iki özellik sabitlenir: DISARM'dan sonra araç silahsızdır ve aynı partide
// daraltıcıyla gelen ARM kabul edilmez (A4-K45; öncelik kuralı protokol modülünde sınandı,
// burada duman düzeyinde).
#include <cstdio>

#include "test_destek.hpp"

namespace ida::sil::test {

namespace {

constexpr unsigned u(unsigned x)
{
    return x;
}

void sonuc_yaz(const char* ad, const mavlink_ida_cmd_result_t& r)
{
    std::printf("  %s: cmd_id %u, ack %u, reason %u, kind %u\n", ad, static_cast<unsigned>(r.cmd_id),
                u(r.ack), u(r.reason), u(r.kind));
}

bool nihai(const mavlink_ida_cmd_result_t& r)
{
    return u(r.ack) == IDA_ACK_ACCEPTED || u(r.ack) == IDA_ACK_REJECTED ||
           u(r.ack) == IDA_ACK_DUPLICATE || u(r.ack) == IDA_ACK_UNSUPPORTED;
}

// Bir partide komut gönderir, ardından birkaç parti daha sürer; sonucu döndürür.
bool komut(SahteSim& sim, std::uint32_t* seq, const std::function<void(PartiKurucu&)>& ek,
           std::uint32_t cmd_id, mavlink_ida_cmd_result_t* r)
{
    if (!KONTROL(parti(sim, (*seq)++, nullptr, ek))) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (!KONTROL(parti(sim, (*seq)++))) {
            return false;
        }
    }
    if (!KONTROL(sonuc_bekle(sim, cmd_id, r))) {
        std::printf("  cmd_id %u için sonuç gelmedi\n", static_cast<unsigned>(cmd_id));
        return false;
    }
    KONTROL(r->sess == sim.sess);
    KONTROL(nihai(*r));
    return true;
}

// STATUS 100 ms'de bir (A1-K91): 25 parti (125 ms) sonra en az bir yeni STATUS vardır.
bool silahsiz_mi(SahteSim& sim, std::uint32_t* seq)
{
    for (int i = 0; i < 25; ++i) {
        if (!KONTROL(parti(sim, (*seq)++))) {
            return false;
        }
    }
    mavlink_ida_status_t s{};
    return KONTROL(son_durum(sim, &s)) && u(s.armed) == 0u;
}

}  // namespace

int test_komut_yolu(const Baglam&)
{
    Duzen d;
    if (!duzen_kur(&d)) {
        return hata_sayisi();
    }
    if (!KONTROL(el_sikis(*d.sim, 0xC0FFEEu, 4242u))) {
        return hata_sayisi();
    }
    std::uint32_t seq = 1;
    for (; seq <= 40u; ++seq) {
        if (!KONTROL(parti(*d.sim, seq))) {
            return hata_sayisi();
        }
    }
    mavlink_ida_cmd_result_t r{};
    const auto simple = [](std::uint32_t id, unsigned tur, unsigned arg) {
        return [id, tur, arg](PartiKurucu& p) {
            p.simple(id, static_cast<std::uint8_t>(tur), static_cast<std::uint16_t>(arg));
        };
    };

    // ARM (genişletici).
    if (komut(*d.sim, &seq, simple(1u, IDA_CMD_ARM, 0u), 1u, &r)) {
        sonuc_yaz("ARM", r);
    }
    // SET_MODE(GUIDED) (genişletici) ve GUIDED_SP (derinlik + yön; W3-K7).
    if (komut(*d.sim, &seq, simple(2u, IDA_CMD_SET_MODE, IDA_MODE_GUIDED), 2u, &r)) {
        sonuc_yaz("SET_MODE(GUIDED)", r);
    }
    if (komut(*d.sim, &seq,
              [](PartiKurucu& p) {
                  p.guided(3u, static_cast<std::uint8_t>(IDA_SP_DEPTH | IDA_SP_YAW), 1.5f, 0.0f);
              },
              3u, &r)) {
        sonuc_yaz("GUIDED_SP", r);
    }
    // SET_MODE(STABILIZE) (daraltıcı: mandal).
    if (komut(*d.sim, &seq, simple(4u, IDA_CMD_SET_MODE, IDA_MODE_STABILIZE), 4u, &r)) {
        sonuc_yaz("SET_MODE(STABILIZE)", r);
    }
    // DISARM (daraltıcı): ardından araç silahsız.
    if (komut(*d.sim, &seq, simple(5u, IDA_CMD_DISARM, 0u), 5u, &r)) {
        sonuc_yaz("DISARM", r);
        KONTROL(u(r.ack) == IDA_ACK_ACCEPTED);
    }
    KONTROL(silahsiz_mi(*d.sim, &seq));

    // Daraltıcı önceliği (duman): aynı partide ARM ve DISARM, iki sırada da ARM kabul edilmez.
    {
        mavlink_ida_cmd_result_t ra{}, rd{};
        const auto ek = [](PartiKurucu& p) {
            p.simple(6u, static_cast<std::uint8_t>(IDA_CMD_ARM), 0u);
            p.simple(7u, static_cast<std::uint8_t>(IDA_CMD_DISARM), 0u);
        };
        if (komut(*d.sim, &seq, ek, 6u, &ra) && KONTROL(sonuc_bekle(*d.sim, 7u, &rd))) {
            sonuc_yaz("ARM (DISARM ile aynı partide, önce)", ra);
            sonuc_yaz("DISARM (aynı parti)", rd);
            KONTROL(u(ra.ack) == IDA_ACK_REJECTED);
        }
        KONTROL(silahsiz_mi(*d.sim, &seq));
    }
    {
        mavlink_ida_cmd_result_t ra{};
        const auto ek = [](PartiKurucu& p) {
            p.simple(8u, static_cast<std::uint8_t>(IDA_CMD_DISARM), 0u);
            p.simple(9u, static_cast<std::uint8_t>(IDA_CMD_ARM), 0u);
        };
        if (komut(*d.sim, &seq, ek, 9u, &ra)) {
            sonuc_yaz("ARM (DISARM ile aynı partide, sonra)", ra);
            KONTROL(u(ra.ack) == IDA_ACK_REJECTED);
        }
        KONTROL(silahsiz_mi(*d.sim, &seq));
    }
    for (const Cerceve& c : d.sim->cerceveler()) {
        KONTROL(c.msgid() != MAVLINK_MSG_ID_IDA_SESSION_CLOSE);
    }
    KONTROL(d.kosucu->bitir() == Cikis::Tamam);
    KONTROL(d.kosucu->istatistik().pencere_ihlali == 0u);
    return hata_sayisi();
}

}  // namespace ida::sil::test
