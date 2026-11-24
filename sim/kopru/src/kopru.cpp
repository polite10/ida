// Kopru: oturum (A1 §4.2), çerçeve alımı ve kart çıktılarının doğrulanması (A1-K109,
// Tablo 4.6), komut sonuçları.
#include "kopru_ic.hpp"

#include "ida/algilama_kaydi.h"
#include "ida/port.h"
#include "ida/sensor/rastgele.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <random>

namespace ida::kopru {

// ---------------------------------------------------------------- A5 Ek D.7, CRC

std::uint32_t d7_u32(std::uint64_t tohum, const std::string& etiket, std::uint64_t n)
{
    const sensor::Akis a(tohum, etiket);
    return a.sozcukler(n, 0u)[0];
}

std::uint64_t d7_u64(std::uint64_t tohum, const std::string& etiket, std::uint64_t n)
{
    const sensor::Akis a(tohum, etiket);
    const auto w = a.sozcukler(n, 0u);
    return static_cast<std::uint64_t>(w[0]) | (static_cast<std::uint64_t>(w[1]) << 32U);
}

std::uint32_t crc32_iso_hdlc(const std::uint8_t* p, std::size_t n)
{
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) != 0u ? (c >> 1U) ^ 0xEDB88320u : c >> 1U;
        }
    }
    return c ^ 0xFFFFFFFFu;
}

namespace {

std::uint32_t gercek_rastgele()
{
    static std::random_device rd;
    return static_cast<std::uint32_t>(rd());
}

// Seri aritmetik (iskelet §3.1): a, b'den yeni mi.
bool yeni(std::uint32_t a, std::uint32_t b)
{
    const std::uint32_t d = a - b;
    return d >= 1u && d <= 0x7FFFFFFFu;
}

}  // namespace

// ---------------------------------------------------------------- Ic yardımcıları

void Kopru::Ic::cerceve(std::uint32_t msgid, const void* m, std::size_t boy, std::size_t* yazilan)
{
    std::uint8_t tampon[IDA_LINK_CERCEVE_AZAMI];
    std::size_t n = 0;
    if (ida_link_gonder(&gonderici, msgid, m, boy, tampon, sizeof tampon, &n) != IDA_LINK_TAMAM) {
        n = 0;
    }
    giden.insert(giden.end(), tampon, tampon + n);
    son_tx = duvar;
    if (yazilan != nullptr) {
        *yazilan = n;
    }
}

std::uint32_t Kopru::Ic::nonce_sec()
{
    std::uint32_t v = 0;
    if (belirlenimci) {
        const std::string etiket = "ida/a1/nonce_sim/" + std::to_string(nonce_deneme);
        for (std::uint64_t n = 0; v == 0; ++n) {
            v = d7_u32(a.ana_tohum, etiket, n);
        }
    } else {
        while (v == 0) {
            v = gercek_rastgele();
        }
    }
    ++nonce_deneme;
    return v;
}

std::uint16_t Kopru::Ic::sess_sec(std::uint16_t prev_sess)
{
    auto yasak = [&](std::uint16_t s) {
        return s == 0 || s == prev_sess ||
               std::find(son_oturumlar.begin(), son_oturumlar.end(), s) != son_oturumlar.end();
    };
    std::uint16_t s = 0;
    if (belirlenimci) {
        const std::string etiket = "ida/a1/sess/" + std::to_string(oturum_sirasi);
        for (std::uint64_t n = 0; yasak(s); ++n) {
            s = static_cast<std::uint16_t>(d7_u32(a.ana_tohum, etiket, n) & 0xFFFFu);
        }
    } else {
        while (yasak(s)) {
            s = static_cast<std::uint16_t>(gercek_rastgele() & 0xFFFFu);
        }
    }
    ++oturum_sirasi;
    son_oturumlar.push_back(s);
    if (son_oturumlar.size() > 16u) {
        son_oturumlar.pop_front();
    }
    return s;
}

void Kopru::Ic::el_sikis(std::int64_t duvar_ns)
{
    duvar = duvar_ns;
    // A1-K12: alıcı sıfırdan (yarım aday atılır), yeni nonce, SIM_HELLO.
    const ida_link_sayaclar_t eski = alici.sayac;
    (void)ida_link_alici_kur(&alici, IDA_LINK_ROL_SIM);
    alici.sayac = eski;
    nonce = nonce_sec();
    durum = OturumDurumu::el_sikisma;
    mavlink_ida_sim_hello_t h{};
    h.dialect_hash = a.dialect_hash;
    h.nonce = nonce;
    h.run_id = run_id;
    h.ctrl_period_ns = static_cast<std::uint32_t>(kP);
    h.proto_major = 1;
    h.proto_minor = 0;
    cerceve(MAVLINK_MSG_ID_IDA_SIM_HELLO, &h, sizeof h);
    son_hello = duvar_ns;
}

void Kopru::Ic::oturum_bitti(bool planli, bool yeniden)
{
    sil_tampon_bosalt();   // W4-K14: tampon kapanışta işlenir (son komut sonucu kaybolmaz)
    sil_tampon.clear();
    const bool oturum_vardi = durum == OturumDurumu::kosuyor || durum == OturumDurumu::hazir;
    if (oturum_vardi) {
        oturum_sonu_bekliyor = true;   // izinler ve revizyon: sonraki sınırın (1). adımı
        yururluk_sil = true;           // iticiler sonraki sınırdan itibaren nötr (A1-K23)
        if (!planli) {
            ++say.oturum_kopma;
            say.calistirma_gecersiz |= kGecPlanlanmamisBitis;
        }
    }
    yuvalar.clear();
    adim_yuva.clear();
    esz_bekleyen.clear();
    esz_muaf.clear();
    ilk_yanit_alindi = false;
    kume.temizle();
    olay_bekleyen.clear();
    karari_bekleyen.clear();
    sonuc_kuyrugu.clear();
    sakli_talep.clear();
    birlesik_seq.clear();
    tf_var = false;
    tf_kayit.temizle();
    yuva_sira.clear();
    out_seq_var = false;
    pen_uye = 0;
    pen_imu = false;
    pen_kritik = 0;
    pen_bayt = 0;
    pen_emniyet.reset();
    sonraki_emniyet.reset();
    pen_sensor.clear();
    kayan.clear();
    // Sonucu gelmemiş ve gönderilmemiş komutlar (A1-K23: LINK_SESSION_END).
    for (std::uint32_t id : sonuc_bekleyen) {
        yerel_sonuc(id, static_cast<std::uint8_t>(IDAW_ACK_REJECTED),
                    static_cast<std::uint16_t>(IDAW_REASON_LINK_SESSION_END));
        ++say.komut_sonucsuz;
    }
    sonuc_bekleyen.clear();
    for (const Komut& k : toplu) {
        yerel_sonuc(k.cmd_id, static_cast<std::uint8_t>(IDAW_ACK_REJECTED),
                    static_cast<std::uint16_t>(IDAW_REASON_LINK_SESSION_END));
        ++say.komut_sonucsuz;
    }
    toplu.clear();
    for (auto& d : dar_bekleyen) {
        if (d) {
            yerel_sonuc(d->cmd_id, static_cast<std::uint8_t>(IDAW_ACK_REJECTED),
                        static_cast<std::uint16_t>(IDAW_REASON_LINK_SESSION_END));
            ++say.komut_sonucsuz;
            d.reset();
        }
    }
    dar_bu_pencerede.fill(false);
    oturum_komutlari.clear();
    sess_ = 0;
    durum = OturumDurumu::kapali;
    if (yeniden) {
        el_sikis(duvar);
    }
}

void Kopru::Ic::canlilik_isle(std::int64_t t, bool sess_tasir)
{
    son_rx_gecerli = std::max(son_rx_gecerli, t);
    if (sess_tasir && durum == OturumDurumu::hazir) {
        durum = OturumDurumu::kosuyor;   // A1-K27
        ++say.oturum_kurulan;
    }
}

void Kopru::Ic::yerel_sonuc(std::uint32_t cmd_id, std::uint8_t ack, std::uint16_t reason)
{
    if (sonuclar.count(cmd_id) != 0u) {
        return;
    }
    KomutSonucu s;
    s.ack = ack;
    s.reason = reason;
    s.kind = static_cast<std::uint8_t>(IDA_RES_ACK);
    s.yerel = true;
    sonuclar[cmd_id] = s;
}

bool Kopru::Ic::tf_gonderildi(std::uint32_t seq) const
{
    return tf_kayit.bul(seq) != nullptr;
}

std::int64_t Kopru::Ic::tf_zamani(std::uint32_t seq) const
{
    const TfKaydi* k = tf_kayit.bul(seq);
    return k != nullptr ? k->t : -1;
}

// ---------------------------------------------------------------- El sıkışma (A1-K14 … K18)

void Kopru::Ic::hello_isle(const mavlink_ida_board_hello_t& h)
{
    sira_var = false;   // seq tanısı yeni oturumla başlar
    kart.boot_count = h.boot_count;
    kart.sessions_since_boot = h.sessions_since_boot;
    kart.param_blob_crc = h.param_blob_crc;
    kart.init_rc = h.init_rc;
    kart.role = h.role;
    board_nonce = h.nonce;

    if (reboot_bekleniyor) {
        // A1-K17: yeniden başlamış kartın yanıtı; değilse yeniden REBOOT istenir (aşağıda).
        if (h.boot_count > reboot_onceki_boot && h.sessions_since_boot == 0) {
            reboot_bekleniyor = false;
        }
    }

    mavlink_ida_session_accept_t ka{};
    ka.run_id = run_id;
    ka.nonce_sim = nonce;
    ka.nonce_board = h.nonce;
    ka.run_mode = static_cast<std::uint8_t>(a.kip);
    std::uint8_t sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_OK);
    ida_core_info_t bilgi{};
    ida_core_info(&bilgi);
    const std::uint64_t sema = a.param_schema_hash != 0 ? a.param_schema_hash : bilgi.param_schema_hash;
    const bool yap_tamam = a.valid_for_ns >= 60 * kMs && a.valid_for_ns <= 450 * kMs && a.t_bag_ns >= 500 * kMs &&
                           a.t_bag_ns <= 2000 * kMs && a.t_bekle_ns >= 10 * kMs && a.t_bekle_ns <= 35 * kMs &&
                           a.apply_by_ns >= 16 * kMs && a.apply_by_ns <= 1000 * kMs && a.hold_ns >= 60 * kMs &&
                           a.hold_ns <= 450 * kMs;
    if (!yap_tamam) {
        sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REJECT_CONFIG);
    } else if (h.proto_major != 1 || h.proto_minor != 0) {
        sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REJECT_PROTO);
    } else if (h.dialect_hash != a.dialect_hash) {
        sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REJECT_DIALECT);
    } else if (h.port_major != IDA_PORT_VERSION_MAJOR || h.port_minor != IDA_PORT_VERSION_MINOR) {
        sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REJECT_PORT);
    } else if (h.det_schema_major != IDA_DET_SCHEMA_MAJOR) {
        sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REJECT_DET_SCHEMA);
    } else if (h.ctrl_period_ns != static_cast<std::uint32_t>(kP)) {
        sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REJECT_RATE);
    } else if (h.param_schema_hash != sema) {
        sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REJECT_PARAM_SCHEMA);
    } else if (a.kip != CalistirmaKipi::hazirlik) {
        // A1-K18.
        // Çalıştırma içindeki yeniden bağlanmada (A1-K23) kart yeniden başlatılmaz: K18 çalıştırmanın
        // başlangıcına uygulanır (Okumalar).
        if ((h.sessions_since_boot > 0 && !calistirma_basladi) || reboot_bekleniyor) {
            sonuc = static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REBOOT);
        } else if (h.init_rc != static_cast<std::uint8_t>(IDA_RC_OK) || h.param_blob_crc != a.beklenen_blok_crc) {
            // A1-K19: hazırlık oturumu. Kip hazırlığa döner; çalıştırma kipine eşitlemeden sonra
            // zaman yöneticisi geri alır.
            hazirlik_gerekli = true;
            ka.run_mode = static_cast<std::uint8_t>(CalistirmaKipi::hazirlik);
        } else if (h.boot_count_ok == 0) {
            // A1-K18: çalıştırma yalnız boot_count_ok = 1 karta başlar; uygun sonuç kodu yok:
            // kabul gönderilmez, durum "reddedildi" (A3'e hata; bu dilimde sayaç).
            ++say.oturum_reddi;
            kart.son_oturum_reddi = 0xFFu;
            durum = OturumDurumu::reddedildi;
            return;
        }
    }
    ka.result = sonuc;
    kart.son_oturum_reddi = sonuc;
    if (sonuc == static_cast<std::uint8_t>(IDAW_SESSION_RESULT_OK)) {
        // A1-K105 (sol #4): out_seq kartın açılışından başlar; doğrulanmış yeni açılışta
        // karşılaştırma tabanı yeniden kurulur, aynı açılıştaki yeniden bağlantıda korunur.
        if (uygulanan_var && uygulanan_boot != h.boot_count) {
            uygulanan_var = false;
        }
        sess_ = sess_sec(h.prev_sess);
        ka.sess = sess_;
        cerceve(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, &ka, sizeof ka);
        durum = OturumDurumu::hazir;
        hazir_bas = duvar;
        son_rx_gecerli = duvar;
        if (!hazirlik_gerekli && a.kip != CalistirmaKipi::hazirlik) {
            calistirma_basladi = true;
        }
        // A1-K66: bu oturumun ilk komut kimliği.
        const std::uint32_t l = h.last_cmd_id;
        std::uint32_t x = yeni(this->c, l) ? this->c : l + 1u;
        if (x == 0u) {
            x = 1u;
        }
        this->c = x;
        return;
    }
    cerceve(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, &ka, sizeof ka);
    if (sonuc == static_cast<std::uint8_t>(IDAW_SESSION_RESULT_REBOOT)) {
        // A1-K17: REBOOT'tan sonra yeni el sıkışma (A1-K12, K114: yeni nonce).
        if (!reboot_bekleniyor) {
            reboot_bekleniyor = true;
            reboot_duvar = duvar;
            reboot_onceki_boot = h.boot_count;
        }
        el_sikis(duvar);
        return;
    }
    ++say.oturum_reddi;
    durum = OturumDurumu::reddedildi;
}

// ---------------------------------------------------------------- Kart çerçeveleri

// W4-K14: SIL'de sim ACTUATOR(n) ve saydığı olaylar alınınca ilerler; ACTUATOR(n)'den sonra
// gelen çerçeveler ACTUATOR(n + 1) alınınca (oturum sonunda kapanışta) işlenir. A1-K90 sırası
// gereği ACTUATOR(n + 1)'den önceki her çerçeve o anda gelmiştir: işlenme anı geliş zamanından
// bağımsızdır. ACTUATOR(n + 1)'in saydığı yeni olaylar ondan önce gelir ve onunla işlenir.
void Kopru::Ic::cerceve_al(const ida_link_cerceve_t& fr)
{
    sira_izle(fr.seq);
    if (a.kip != CalistirmaKipi::sil || durum != OturumDurumu::kosuyor) {
        kart_cercevesi(fr);
        return;
    }
    if (fr.msgid == MAVLINK_MSG_ID_IDA_ACTUATOR || fr.msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE ||
        fr.msgid == MAVLINK_MSG_ID_IDA_BOARD_HELLO) {
        sil_tampon_bosalt();
        kart_cercevesi(fr);
        return;
    }
    sil_tampon.push_back(fr);
    say.sil_tampon_en_cok = std::max<std::uint64_t>(say.sil_tampon_en_cok, sil_tampon.size());
    if (sil_tampon.size() > kSilTamponAzami) {
        // Protokol hatası (A1-K23): oturum biter; tampon kapanışta işlenir (sessiz atma yok).
        ++say.sil_tampon_tasma;
        mavlink_ida_session_close_t kapanis{};
        kapanis.sess = sess_;
        kapanis.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_PROTOCOL);
        giden.clear();
        cerceve(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &kapanis, sizeof kapanis);
        oturum_bitti(false, true);
    }
}

void Kopru::Ic::sil_tampon_bosalt()
{
    if (sil_tampon_isleniyor || sil_tampon.empty()) {
        return;
    }
    sil_tampon_isleniyor = true;
    sil_isle.swap(sil_tampon);   // iki vektör: döngüde yeniden ayırma yok
    sil_tampon.clear();
    std::size_t j = 0;
    for (; j < sil_isle.size(); ++j) {
        if (durum != OturumDurumu::kosuyor) {
            break;   // işlenen bir çerçeve oturumu bitirdi: kalanlar biten oturumundur
        }
        kart_cercevesi(sil_isle[j]);
    }
    say.sil_tampon_atilan += sil_isle.size() - j;
    sil_isle.clear();
    sil_tampon_isleniyor = false;
}

void Kopru::Ic::sira_izle(std::uint8_t seq)
{
    if (!sira_var) {
        sira_var = true;
        sira_en = seq;
        sira_maske = 0;
        return;
    }
    const auto ileri = static_cast<std::uint8_t>(seq - sira_en);
    if (ileri == 0u) {
        return;   // yineleme
    }
    if (ileri < 128u) {
        const unsigned d = ileri;
        const unsigned atlanan = d - 1u;
        if (d >= 32u) {
            say.rx_kayip += static_cast<std::uint64_t>(std::popcount(sira_maske));
            const unsigned pencerede = std::min(atlanan, 32u);
            say.rx_kayip += atlanan - pencerede;
            sira_maske = pencerede >= 32u ? 0xFFFFFFFFu : ((1u << pencerede) - 1u);
        } else {
            const std::uint32_t cikan = sira_maske >> (32u - d);
            say.rx_kayip += static_cast<std::uint64_t>(std::popcount(cikan));
            sira_maske = static_cast<std::uint32_t>((sira_maske << d) | ((1u << atlanan) - 1u));
        }
        sira_en = seq;
        return;
    }
    const unsigned k = static_cast<std::uint8_t>(sira_en - seq) - 1u;
    if (k < 32u && ((sira_maske >> k) & 1u) != 0u) {
        ++say.rx_sira_disi;
        sira_maske &= ~(1u << k);
    }
}

void Kopru::Ic::kart_cercevesi(const ida_link_cerceve_t& fr)
{
    // El sıkışmada yalnız bizim nonce'umuzu yanıtlayan BOARD_HELLO işlenir (A1-K12).
    if (durum == OturumDurumu::el_sikisma) {
        if (fr.msgid == MAVLINK_MSG_ID_IDA_BOARD_HELLO) {
            mavlink_ida_board_hello_t h{};
            (void)ida_link_coz(&fr, fr.msgid, &h, sizeof h);
            if (h.peer_nonce == nonce && nonce != 0 && h.nonce != 0 && (h.role == 1 || h.role == 2)) {
                hello_isle(h);
                return;
            }
        }
        ++say.el_sikisma_atilan;
        return;
    }
    if (durum == OturumDurumu::kapali || durum == OturumDurumu::reddedildi) {
        // Oturum dışında: oturum sonu çağrısının izi (A1-K95) yalnız sayılır.
        if (fr.msgid == MAVLINK_MSG_ID_IDA_CALL_TRACE) {
            ++say.cagri_izi;
        }
        return;
    }
    // A1-K109, W4-K15 ek (7): canlılık (T_bağ) ve HAZIR → KOSUYOR yalnız doğrulamadan geçen
    // çerçeveyle güncellenir; reddedilen çerçevenin sayaç dışında etkisi yoktur. Çerçeve
    // sınıflarına göre karar (BENIOKU "Olay kümesi" canlılık tablosu):
    //   kümeye bağlı (olağan yanıt, genişletici olay ve yinelemesi)  küme doğrulanınca (Ic::kume_tamamla)
    //   beklemeden uygulanan (SAFETY_CLOSE, hata dönüşü, ASYNC, uygulanmış olayın yinelemesi,
    //   komut sonucu, durum ve öteki geçerli çerçeveler)                alındığı anda
    auto gecerli = [this](bool sess_tasir) { canlilik_isle(duvar, sess_tasir); };
    auto karar = [&](Canlilik katki) {
        if (katki == Canlilik::aninda) {
            gecerli(true);
        }
    };
    switch (fr.msgid) {
    case MAVLINK_MSG_ID_IDA_BOARD_HELLO: {
        if (durum == OturumDurumu::kosuyor) {
            // A1-K23: KOSUYOR'da BOARD_HELLO: kart sıfırlandı.
            ++say.beklenmeyen_sifirlama;
            oturum_bitti(false, true);
        }
        return;
    }
    case MAVLINK_MSG_ID_IDA_KEEPALIVE: {
        mavlink_ida_keepalive_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.state > 2u) {
            ++say.alan_ret;
            return;
        }
        if (m.sess != sess_) {
            ++say.yanit_ret_oturum;
            return;
        }
        break;
    }
    case MAVLINK_MSG_ID_IDA_SESSION_CLOSE: {
        mavlink_ida_session_close_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.reason < 1u || m.reason > 7u) {
            ++say.alan_ret;
            return;
        }
        if (m.sess != sess_) {
            ++say.yanit_ret_oturum;
            return;
        }
        son_rx_gecerli = duvar;
        kart.son_kapanis_nedeni = m.reason;
        oturum_bitti(false, true);   // A1-K21, K23
        return;
    }
    case MAVLINK_MSG_ID_IDA_ACTUATOR: {
        mavlink_ida_actuator_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.sess != sess_) {
            ++say.yanit_ret_oturum;
            return;
        }
        karar(yanit_al(m));
        return;
    }
    case MAVLINK_MSG_ID_IDA_EVENT: {
        mavlink_ida_event_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.sess != sess_) {
            ++say.yanit_ret_oturum;
            return;
        }
        karar(olay_al(m));
        return;
    }
    case MAVLINK_MSG_ID_IDA_CMD_RESULT: {
        mavlink_ida_cmd_result_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.sess != sess_) {
            ++say.yanit_ret_oturum;
            return;
        }
        if (sonuc_al(m)) {
            gecerli(true);
        }
        return;
    }
    case MAVLINK_MSG_ID_IDA_STATUS: {
        mavlink_ida_status_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.sess != sess_) {
            ++say.yanit_ret_oturum;
            return;
        }
        if (m.mode > 7u || m.fs_level > 5u || m.link_state > 2u || m.armed > 1u || m.param_ok > 1u ||
            m.events_ok > 1u) {
            ++say.alan_ret;
            return;
        }
        kart.durum.var = true;
        kart.durum.batch_seq = m.batch_seq;
        kart.durum.mode = m.mode;
        kart.durum.armed = m.armed;
        kart.durum.fs_level = m.fs_level;
        kart.durum.fs_reasons = m.fs_reasons;
        kart.durum.health = m.health;
        kart.durum.link_state = m.link_state;
        kart.durum.param_store = m.param_store;
        kart.durum.param_blob_crc = m.param_blob_crc;
        kart.durum.boot_count = m.boot_count;
        break;
    }
    case MAVLINK_MSG_ID_IDA_NAV: {
        mavlink_ida_nav_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.est_level > 4u) {
            ++say.alan_ret;
            return;
        }
        kart.nav.var = true;
        kart.nav.t_ns = m.t_ns;
        kart.nav.batch_seq = m.batch_seq;
        kart.nav.depth = m.depth;
        kart.nav.est_level = m.est_level;
        for (std::size_t i = 0; i < 3; ++i) {
            kart.nav.p_ned[i] = m.p_ned[i];
        }
        for (std::size_t i = 0; i < 4; ++i) {
            kart.nav.q_nb[i] = m.q_nb[i];
        }
        gecerli(false);
        return;
    }
    case MAVLINK_MSG_ID_IDA_FC_STATUS: {
        mavlink_ida_fc_status_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        const bool sonuc_gecerli = m.last_event_result == 0u ||
                                   (m.last_event_result >= 1u && m.last_event_result <= 6u) ||
                                   (m.last_event_result >= 16u && m.last_event_result <= 19u);
        if (m.state > 5u || (m.inhibit_mask & 0xFFFC0000u) != 0u || !sonuc_gecerli) {
            ++say.alan_ret;
            return;
        }
        kart.fc_var = true;
        kart.fc_state = m.state;
        kart.fc_inhibit_mask = m.inhibit_mask;
        kart.fc_son_olay_sonucu = m.last_event_result;
        gecerli(false);
        return;
    }
    case MAVLINK_MSG_ID_IDA_LOG: {
        mavlink_ida_log_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.code < 1u || m.code > 23u || m.severity > 7u) {
            ++say.alan_ret;
            return;
        }
        gecerli(false);
        return;
    }
    case MAVLINK_MSG_ID_IDA_CALL_TRACE: {
        mavlink_ida_call_trace_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        if (m.call_kind < 1u || m.call_kind > 4u || m.n_rx > 28u || (m.plat_flags & 0xC0u) != 0u ||
            m.core_rc > 8u) {
            ++say.alan_ret;
            return;
        }
        ++say.cagri_izi;
        gecerli(false);
        return;
    }
    case MAVLINK_MSG_ID_IDA_CORE_COUNTERS: {
        mavlink_ida_core_counters_t m{};
        (void)ida_link_coz(&fr, fr.msgid, &m, sizeof m);
        KartSayaclari& s = kart.sayac;
        s.var = true;
        s.sample_rejected = m.sample_rejected;
        s.sample_future = m.sample_future;
        s.sample_conflict = m.sample_conflict;
        s.sample_dup_dropped = m.sample_dup_dropped;
        s.narrow_from_rejected = m.narrow_from_rejected;
        s.imu_rejected = m.imu_rejected;
        s.imu_gap = m.imu_gap;
        s.bad_input = m.bad_input;
        s.dup_seq = m.dup_seq;
        s.old_seq = m.old_seq;
        s.cmd_rejected = m.cmd_rejected;
        s.widen_blocked = m.widen_blocked;
        s.evres_ignored = m.evres_ignored;
        s.event_unknown = m.event_unknown;
        s.step_miss = m.step_miss;
        gecerli(false);
        return;
    }
    default:
        // LINK_STATS, FAULT_RECORD, DET_RECORD: aralık denetimi yok (Tablo 4.6); bu dilimde
        // yalnız alınır.
        gecerli(false);
        return;
    }
    // sess taşıyan, doğrulamadan geçen çerçeve (KEEPALIVE, STATUS).
    gecerli(true);
}

bool Kopru::Ic::sonuc_al(const mavlink_ida_cmd_result_t& m)
{
    // V2
    const bool reason_ok = m.reason <= 15u || (m.reason >= 128u && m.reason <= 131u);
    bool ok = m.kind >= 1u && m.kind <= 3u && m.ack >= 1u && m.ack <= 5u && reason_ok;
    if (ok && m.kind == IDA_RES_PARAM_VALUE) {
        if (m.ack == IDAW_ACK_ACCEPTED) {
            ok = m.param_type >= 1u && m.param_type <= 3u;
        } else {
            ok = m.param_type == 0u && m.value_bits == 0u;
        }
    }
    if (!ok) {
        ++say.alan_ret;
        return false;
    }
    // V3
    if (oturum_komutlari.count(m.cmd_id) == 0u) {
        ++say.bag_ret;
        return false;
    }
    sonuc_bekleyen.erase(m.cmd_id);
    if (sonuclar.count(m.cmd_id) != 0u) {
        return true;   // A1-K67: ilk sonuç iletilir, sonrakiler yalnız kaydedilir
    }
    KomutSonucu s;
    s.ack = m.ack;
    s.reason = m.reason;
    s.kind = m.kind;
    s.value_bits = m.value_bits;
    s.save_crc = m.save_crc;
    s.t_ns = tf_var ? tf_zamani(son_seq) : 0;
    sonuclar[m.cmd_id] = s;
    return true;
}

// ---------------------------------------------------------------- Kopru

Kopru::Kopru(const KopruAyari& ayar) : ic_(std::make_unique<Ic>())
{
    Ic& i = *ic_;
    i.a = ayar;
    i.sonraki_seq = ayar.ilk_batch_seq;
    i.belirlenimci = ayar.kip == CalistirmaKipi::sil;
    if (i.a.dialect_hash == 0) {
        i.a.dialect_hash = IDA_LINK_DIALECT_HASH;
    }
    (void)ida_link_alici_kur(&i.alici, IDA_LINK_ROL_SIM);
    (void)ida_link_gonderici_kur(&i.gonderici, IDA_LINK_ROL_SIM);
    if (i.a.run_id != 0) {
        i.run_id = i.a.run_id;
    } else {
        for (std::uint64_t n = 0; i.run_id == 0; ++n) {
            i.run_id = d7_u32(i.a.ana_tohum, "a1/run_id", n);
        }
    }
    i.board_tohum = d7_u64(i.a.ana_tohum, "a1/board_tohum", 0);
    i.pwm.fill(static_cast<double>(i.a.esc_notr_us));
    i.emn = sensor::EmniyetDurumu{};
    i.izin_bitis.fill(0);
    i.rev_girdi[i.emn.emniyet_revizyonu] = RevDurum{i.girdi_kodu(), i.izin_kodu()};   // W4-K12
}

Kopru::~Kopru() = default;
Kopru::Kopru(Kopru&&) noexcept = default;
Kopru& Kopru::operator=(Kopru&&) noexcept = default;

void Kopru::el_sikisma_baslat(std::int64_t duvar_ns)
{
    ic_->giden.clear();
    ic_->el_sikis(duvar_ns);
}

void Kopru::al(const std::uint8_t* p, std::size_t n, std::int64_t duvar_ns)
{
    (void)al(p, n, duvar_ns, std::function<bool()>{});
}

std::size_t Kopru::al(const std::uint8_t* p, std::size_t n, std::int64_t duvar_ns, const std::function<bool()>& dur)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    std::size_t tuketilen = 0;
    while (n > 0) {
        std::size_t tuk = 0;
        ida_link_cerceve_t fr;
        const ida_link_rc_t rc = ida_link_al(&i.alici, p, n, &tuk, &fr);
        p += tuk;
        n -= tuk;
        tuketilen += tuk;
        if (rc == IDA_LINK_CERCEVE) {
            while (i.sira_boslugu_t.size() < i.alici.sayac.rx_seq_gap && i.sira_boslugu_t.size() < 1024u) {
                i.sira_boslugu_t.push_back(i.sinir_var ? static_cast<std::int64_t>(i.son_sinir) * kH : 0);
            }
            i.cerceve_al(fr);
            if (dur && dur()) {
                break;
            }
        } else {
            tuketilen += n;   // BEKLE: hepsi tüketildi; hata dönüşünde kalan (eskisi gibi) atılır
            break;
        }
    }
    const ida_link_sayaclar_t& s = i.alici.sayac;
    i.say.rx_crc_err = s.rx_crc_err;
    i.say.rx_resync_bytes = s.rx_resync_bytes;
    i.say.rx_unknown_msgid = s.rx_unknown_msgid;
    i.say.rx_bad_len = s.rx_bad_len;
    i.say.rx_wrong_sys = s.rx_wrong_sys;
    i.say.rx_iflag = s.rx_iflag;
    i.say.rx_mav1 = s.rx_mav1;
    i.say.rx_seq_gap = s.rx_seq_gap;
    i.say.rx_frames = s.rx_frames;
    return tuketilen;
}

void Kopru::zamanla(std::int64_t duvar_ns)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    switch (i.durum) {
    case OturumDurumu::el_sikisma:
        if (duvar_ns - i.son_hello >= kTHello) {
            mavlink_ida_sim_hello_t h{};
            h.dialect_hash = i.a.dialect_hash;
            h.nonce = i.nonce;
            h.run_id = i.run_id;
            h.ctrl_period_ns = static_cast<std::uint32_t>(kP);
            h.proto_major = 1;
            h.proto_minor = 0;
            i.cerceve(MAVLINK_MSG_ID_IDA_SIM_HELLO, &h, sizeof h);
            i.son_hello = duvar_ns;
        }
        break;
    case OturumDurumu::hazir:
        if (duvar_ns - i.hazir_bas >= kTHazir) {
            i.oturum_bitti(false, true);   // A1-K27
            return;
        }
        [[fallthrough]];
    case OturumDurumu::kosuyor:
        if (i.a.kip != CalistirmaKipi::sil && duvar_ns - i.son_rx_gecerli >= i.a.t_bag_ns) {
            // A1-K23: T_bağ (HIL).
            mavlink_ida_session_close_t c{};
            c.sess = i.sess_;
            c.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_PEER_TIMEOUT);
            i.giden.clear();
            i.cerceve(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &c, sizeof c);
            i.oturum_bitti(false, true);
            return;
        }
        if (duvar_ns - i.son_tx >= kTCanli) {
            mavlink_ida_keepalive_t k{};
            k.sess = i.sess_;
            k.state = static_cast<std::uint8_t>(i.durum == OturumDurumu::kosuyor ? IDAW_LINK_STATE_RUNNING
                                                                                  : IDAW_LINK_STATE_HANDSHAKE);
            i.cerceve(MAVLINK_MSG_ID_IDA_KEEPALIVE, &k, sizeof k);
        }
        break;
    case OturumDurumu::kapali:
    case OturumDurumu::reddedildi:
        break;
    }
}

void Kopru::oturum_kapat(std::uint8_t reason, std::int64_t duvar_ns)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    if (i.durum == OturumDurumu::kosuyor || i.durum == OturumDurumu::hazir) {
        i.giden.clear();
        mavlink_ida_session_close_t c{};
        c.sess = i.sess_;
        c.reason = reason;
        i.cerceve(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &c, sizeof c);
    }
    if (reason == static_cast<std::uint8_t>(IDAW_CLOSE_REASON_RUN_END) || reason == static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SHUTDOWN)) {
        i.calistirma_basladi = false;   // A1-K128: yeni çalıştırma
    }
    i.oturum_bitti(true, false);
}

void Kopru::akis_kapandi(std::int64_t duvar_ns)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    i.giden.clear();
    i.oturum_bitti(false, false);
}

void Kopru::sil_gozetimi_doldu(std::int64_t duvar_ns)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    i.giden.clear();
    i.oturum_bitti(false, false);
}

void Kopru::protokol_hatasi(std::int64_t duvar_ns)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    if (i.durum != OturumDurumu::kosuyor && i.durum != OturumDurumu::hazir) {
        return;
    }
    mavlink_ida_session_close_t kapanis{};
    kapanis.sess = i.sess_;
    kapanis.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_PROTOCOL);
    i.giden.clear();
    i.cerceve(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &kapanis, sizeof kapanis);
    i.oturum_bitti(false, true);
}

void Kopru::kip_ayarla(CalistirmaKipi kip)
{
    ic_->a.kip = kip;
    ic_->hazirlik_gerekli = false;
}

OturumDurumu Kopru::durum() const { return ic_->durum; }
std::uint16_t Kopru::sess() const { return ic_->sess_; }
bool Kopru::yeniden_baslatma_bekleniyor() const { return ic_->reboot_bekleniyor; }
bool Kopru::hazirlik_gerekli() const { return ic_->hazirlik_gerekli; }

void Kopru::giden_al(std::vector<std::uint8_t>& v)
{
    v.clear();
    v.swap(ic_->giden);
}

bool Kopru::giden_var() const { return !ic_->giden.empty(); }

std::optional<KomutSonucu> Kopru::komut_sonucu(std::uint32_t cmd_id) const
{
    const auto it = ic_->sonuclar.find(cmd_id);
    if (it == ic_->sonuclar.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::size_t Kopru::bekleyen_komut() const
{
    std::size_t n = ic_->toplu.size() + ic_->sonuc_bekleyen.size();
    for (const auto& d : ic_->dar_bekleyen) {
        n += d ? 1u : 0u;
    }
    return n;
}

const Sayaclar& Kopru::sayaclar() const { return ic_->say; }
const KartBilgisi& Kopru::kart() const { return ic_->kart; }
const KopruAyari& Kopru::ayar() const { return ic_->a; }
std::uint32_t Kopru::run_id() const { return ic_->run_id; }
std::uint64_t Kopru::board_tohumu() const { return ic_->board_tohum; }

std::string Kopru::yapilandirma_hatasi() const
{
    const KopruAyari& a = ic_->a;
    if (a.valid_for_ns < 60 * kMs || a.valid_for_ns > 450 * kMs) {
        return "valid_for_ns 60–450 ms dışında (E2, E3)";
    }
    if (a.t_bag_ns < 500 * kMs || a.t_bag_ns > 2000 * kMs) {
        return "T_bağ 500–2000 ms dışında (E2, E4, E5)";
    }
    if (a.t_bekle_ns < 10 * kMs || a.t_bekle_ns > 35 * kMs) {
        return "T_bekle 10–35 ms dışında (E2, E8)";
    }
    if (a.apply_by_ns < 16 * kMs || a.apply_by_ns > 1000 * kMs) {
        return "apply_by_ns 16–1000 ms dışında (E6, E7)";
    }
    if (a.hold_ns < 60 * kMs || a.hold_ns > 450 * kMs) {
        return "hold_ns 60–450 ms dışında (E9)";
    }
    return {};
}

}  // namespace ida::kopru
