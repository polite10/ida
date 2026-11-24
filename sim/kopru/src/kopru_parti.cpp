// Kopru: parti kurulumu (A1 §4.3, §4.4) ve operatör komutları (A1 §4.5).
#include "kopru_ic.hpp"

#include "ida/port.h"

#include <algorithm>
#include <variant>

namespace ida::kopru {
namespace {

using sensor::Olcum;

// Emniyet kaydının kısıtlama sırası: a, b'ye göre bir alanı gevşetiyor mu (estop 1→0, fiş
// 0→1, pim çıkık 0→1, izin 0→1; A1-K56, A4-K116; izin açılması da gevşetme sayılır: EMNİYET).
bool gevsetiyor(const sensor::EmniyetOlcumu& a, const sensor::EmniyetOlcumu& b)
{
    if (b.estop != 0u && a.estop == 0u) {
        return true;
    }
    constexpr std::uint8_t kGevsek = static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_PLUG_IN | IDAW_TUBE_FLAGS_PIN_OUT |
                                                               IDAW_TUBE_FLAGS_PERMIT_OPEN);
    for (std::size_t t = 0; t < 2; ++t) {
        const std::uint8_t acilan = static_cast<std::uint8_t>(a.tube[t] & static_cast<std::uint8_t>(~b.tube[t]));
        if ((acilan & kGevsek) != 0u) {
            return true;
        }
    }
    return false;
}

// İki kaydın en kısıtlayıcı birleşimi (alan alan): estop VEYA, gevşek bitler ve LOADED VE;
// revizyon sonrakinin (gönderimde W4-K12 (3) ile yeniden seçilir).
sensor::EmniyetOlcumu en_kisitlayici(const sensor::EmniyetOlcumu& eski, const sensor::EmniyetOlcumu& yeni_)
{
    sensor::EmniyetOlcumu r = yeni_;
    r.estop = static_cast<std::uint8_t>((eski.estop != 0u || yeni_.estop != 0u) ? 1u : 0u);
    constexpr std::uint8_t kGevsek = static_cast<std::uint8_t>(IDAW_TUBE_FLAGS_PLUG_IN | IDAW_TUBE_FLAGS_PIN_OUT |
                                                               IDAW_TUBE_FLAGS_PERMIT_OPEN);
    for (std::size_t t = 0; t < 2; ++t) {
        const std::uint8_t gevsek = static_cast<std::uint8_t>(eski.tube[t] & yeni_.tube[t] & kGevsek);
        const std::uint8_t dolu = static_cast<std::uint8_t>(eski.tube[t] & yeni_.tube[t] & IDAW_TUBE_FLAGS_LOADED);
        r.tube[t] = static_cast<std::uint8_t>(gevsek | dolu);
    }
    return r;
}

}  // namespace

// Emniyet örneğinin alan kodu (W4-K12): bit 0 estop, bit 1–2 fiş takılı, bit 3–4 pim çıkık,
// bit 5–6 izin açık (tüp 0, 1).
std::uint8_t emniyet_kodu(const sensor::EmniyetOlcumu& y)
{
    std::uint8_t kod = y.estop != 0u ? 1u : 0u;
    for (std::size_t k = 0; k < 2; ++k) {
        kod = static_cast<std::uint8_t>(kod | ((y.tube[k] & IDAW_TUBE_FLAGS_PLUG_IN) != 0u ? (2u << k) : 0u) |
                                        ((y.tube[k] & IDAW_TUBE_FLAGS_PIN_OUT) != 0u ? (8u << k) : 0u) |
                                        ((y.tube[k] & IDAW_TUBE_FLAGS_PERMIT_OPEN) != 0u ? (32u << k) : 0u));
    }
    return kod;
}

// kod, revizyonun yetkili durumundan alan alan en az o kadar kısıtlayıcı mı: yetkili estop
// basılıysa kod da basılı; koddaki her gevşek bit (fiş takılı, pim çıkık, izin açık) yetkili
// durumda da gevşek (izin için: o revizyonda açılmış).
bool en_az_kisitlayici(std::uint8_t kod, const RevDurum& r)
{
    const std::uint8_t yetkili = static_cast<std::uint8_t>((r.girdi & 0x1Fu) | (r.izin_acilan & 0x60u));
    if ((yetkili & 1u) != 0u && (kod & 1u) == 0u) {
        return false;
    }
    return ((kod & 0x7Eu) & static_cast<std::uint8_t>(~yetkili)) == 0u;
}

// ---------------------------------------------------------------- Parti

namespace {

// Sensör üyesini pencereye ekler; yazım ve A1-K113 kaydırması pencere kapanışındadır.
template <typename M>
void uye_ekle(Kopru::Ic& i, std::uint32_t msgid, const M& m, const sensor::OlcumBasligi& h)
{
    SensorUye u;
    u.msgid = msgid;
    u.m = m;
    u.t_hazir = h.t_teslim_ns;
    u.tur = static_cast<std::uint8_t>(h.tur);
    u.inst = h.inst;
    u.gecersiz = (h.flags & sensor::kBayrakGecerli) == 0u;
    u.boy = i.uye_boy(u);
    i.pen_sensor.push_back(u);
}

}  // namespace

std::size_t Kopru::Ic::uye_boy(const SensorUye& u) const
{
    // Kodlanmış boy (kırpılmış yük; A1-K05): gönderici kopyasıyla, seq ilerletilmeden.
    ida_link_gonderici_t g = gonderici;
    std::uint8_t tampon[IDA_LINK_CERCEVE_AZAMI];
    std::size_t n = 0;
    std::visit([&](const auto& m) { (void)ida_link_gonder(&g, u.msgid, &m, sizeof m, tampon, sizeof tampon, &n); },
               u.m);
    return n;
}

std::size_t Kopru::Ic::uye_yaz(const SensorUye& u)
{
    std::size_t n = 0;
    std::visit([&](const auto& m) { cerceve(u.msgid, &m, sizeof m, &n); }, u.m);
    return n;
}

void Kopru::olcum_ekle(const sensor::Olcum& o)
{
    Ic& i = *ic_;
    if (i.durum != OturumDurumu::kosuyor) {
        return;
    }
    const sensor::OlcumBasligi& h = o.hdr;
    if (const auto* e = std::get_if<sensor::EmniyetOlcumu>(&o.yuk)) {
        (void)e;
        // A1-K56: partide ≤ 1; kapanma o partide, sonraki açılma sonraki partide. Kayıt pencere
        // kapanışında yazılır (Okumalar, EMNİYET).
        // W4-K12 (1): kısıtlayan alan hiçbir zaman ertelenmez. Yeni kayıt penceredekiyle alan alan
        // en kısıtlayıcı biçimde birleştirilir; kayıt bir alanı gevşetiyorsa yalnız gevşetme
        // (kaydın kendisi, kendi seq'iyle) sonraki partiye kalır. Birleşim, penceredeki kaydın
        // seq ve ölçüm zamanını korur (kısıtlama daha erken bildirilir: kısıtlayan yön).
        if (!i.pen_emniyet) {
            i.pen_emniyet = o;
            return;
        }
        const auto& onceki = std::get<sensor::EmniyetOlcumu>(i.pen_emniyet->yuk);
        const auto& yeni_ = std::get<sensor::EmniyetOlcumu>(o.yuk);
        const sensor::EmniyetOlcumu birlesik = en_kisitlayici(onceki, yeni_);
        ++i.say.emniyet_birlesti;
        if (gevsetiyor(yeni_, onceki)) {
            auto& y = std::get<sensor::EmniyetOlcumu>(i.pen_emniyet->yuk);
            y = birlesik;
            y.safety_rev = std::min(onceki.safety_rev, yeni_.safety_rev);
            i.sonraki_emniyet = o;   // gevşetme sonraki partide (A1-K56)
            ++i.say.emniyet_ertelendi;
        } else {
            // Yeni kayıt penceredekinden gevşek değil: birleşim yeni kaydın durumudur ve en yeni
            // gerçektir; ertelenmiş eski gevşetme artık geçersizdir.
            Olcum b = o;
            b.yuk = birlesik;
            i.pen_emniyet = b;
            i.sonraki_emniyet.reset();
        }
        return;
    }
    switch (h.tur) {
    case sensor::SensorTuru::imu: {
        const auto& y = std::get<sensor::ImuOlcumu>(o.yuk);
        mavlink_ida_imu_t m{};
        m.t_meas_ns = h.t_meas_ns;
        m.seq = h.seq;
        m.window_ns = h.window_ns;
        for (std::size_t k = 0; k < 3; ++k) {
            m.gyro[k] = y.gyro[k];
            m.accel[k] = y.accel[k];
        }
        m.temp_c = y.temp_c;
        m.flags = h.flags;
        m.inst = h.inst;
        uye_ekle(i, MAVLINK_MSG_ID_IDA_IMU, m, h);
        break;
    }
    case sensor::SensorTuru::manyetometre: {
        const auto& y = std::get<sensor::MagOlcumu>(o.yuk);
        mavlink_ida_mag_t m{};
        m.t_meas_ns = h.t_meas_ns;
        m.seq = h.seq;
        for (std::size_t k = 0; k < 3; ++k) {
            m.field[k] = y.field[k];
        }
        m.flags = h.flags;
        m.inst = h.inst;
        uye_ekle(i, MAVLINK_MSG_ID_IDA_MAG, m, h);
        break;
    }
    case sensor::SensorTuru::basinc: {
        const auto& y = std::get<sensor::BaroOlcumu>(o.yuk);
        mavlink_ida_baro_t m{};
        m.t_meas_ns = h.t_meas_ns;
        m.seq = h.seq;
        m.window_ns = h.window_ns;
        m.p_abs = y.p_abs;
        m.temp_c = y.temp_c;
        m.flags = h.flags;
        m.inst = h.inst;
        uye_ekle(i, MAVLINK_MSG_ID_IDA_BARO, m, h);
        break;
    }
    case sensor::SensorTuru::gnss: {
        const auto& y = std::get<sensor::GnssOlcumu>(o.yuk);
        mavlink_ida_gnss_t m{};
        m.t_meas_ns = h.t_meas_ns;
        m.seq = h.seq;
        m.lat_e7 = y.lat_e7;
        m.lon_e7 = y.lon_e7;
        m.alt_mm = y.alt_mm;
        for (std::size_t k = 0; k < 3; ++k) {
            m.vel_ned[k] = y.vel_ned[k];
        }
        m.eph = y.eph;
        m.epv = y.epv;
        m.flags = h.flags;
        m.inst = h.inst;
        m.fix_type = y.fix_type;
        m.n_sv = y.n_sv;
        uye_ekle(i, MAVLINK_MSG_ID_IDA_GNSS, m, h);
        break;
    }
    case sensor::SensorTuru::dvl: {
        const auto& y = std::get<sensor::DvlOlcumu>(o.yuk);
        mavlink_ida_dvl_t m{};
        m.t_meas_ns = h.t_meas_ns;
        m.seq = h.seq;
        for (std::size_t k = 0; k < 3; ++k) {
            m.vel[k] = y.vel[k];
        }
        for (std::size_t k = 0; k < 6; ++k) {
            m.cov[k] = y.cov[k];
        }
        m.fom = y.fom;
        for (std::size_t k = 0; k < 4; ++k) {
            m.beam_dist[k] = y.beam_dist[k];
        }
        m.altitude = y.altitude;
        m.flags = h.flags;
        m.inst = h.inst;
        m.dvl_flags = y.dvl_flags;
        m.beam_valid = y.beam_valid;
        uye_ekle(i, MAVLINK_MSG_ID_IDA_DVL, m, h);
        break;
    }
    case sensor::SensorTuru::batarya_olcer: {
        const auto& y = std::get<sensor::BataryaOlcumu>(o.yuk);
        mavlink_ida_batt_t m{};
        m.t_meas_ns = h.t_meas_ns;
        m.seq = h.seq;
        m.window_ns = h.window_ns;
        m.voltage = y.voltage;
        m.current = y.current;
        m.temp_c = y.temp_c;
        m.flags = h.flags;
        m.inst = h.inst;
        uye_ekle(i, MAVLINK_MSG_ID_IDA_BATT, m, h);
        break;
    }
    case sensor::SensorTuru::sizinti: {
        const auto& y = std::get<sensor::SizintiOlcumu>(o.yuk);
        mavlink_ida_leak_t m{};
        m.t_meas_ns = h.t_meas_ns;
        m.seq = h.seq;
        m.flags = h.flags;
        m.inst = h.inst;
        m.mask = y.mask;
        uye_ekle(i, MAVLINK_MSG_ID_IDA_LEAK, m, h);
        break;
    }
    case sensor::SensorTuru::ping1d:
    case sensor::SensorTuru::ping360:
    case sensor::SensorTuru::firlatici_geri_okuma:
    case sensor::SensorTuru::kamera:
        break;
    }
}

// ---------------------------------------------------------------- Sonar (W4-bagla-sonar)

void Kopru::ping1d_ekle(const sensor::OlcumBasligi& h, const Ping1dVerisi& v)
{
    Ic& i = *ic_;
    if (i.durum != OturumDurumu::kosuyor) {
        return;
    }
    // A1 Tablo 3.4b: alanlar dönüşümsüz; t_meas yansıma anıdır, gönderim anı t_meas − tx_offset
    // (A1-K54). Kritik üye: penceresi t_hazır'a göre (A1-K35), yazımı pencere kapanışında.
    mavlink_ida_ping1d_t m{};
    m.t_meas_ns = h.t_meas_ns;
    m.seq = h.seq;
    m.distance = v.distance;
    m.scan_start = v.scan_start;
    m.scan_length = v.scan_length;
    m.ping_number = v.ping_number;
    m.tx_offset_ns = v.tx_offset_ns;
    m.flags = h.flags;
    m.inst = h.inst;
    m.confidence = v.confidence;
    uye_ekle(i, MAVLINK_MSG_ID_IDA_PING1D, m, h);
    ++i.say.ping1d_uye;
}

void Kopru::ping360_ekle(const sensor::OlcumBasligi& h, const Ping360Verisi& v)
{
    Ic& i = *ic_;
    if (i.durum != OturumDurumu::kosuyor) {
        ++i.say.ping360_oturum_atilan;   // A1-K49: satır yalnız bir oturumun içinde gider
        return;
    }
    // A1 §3.4 gönderen aralıkları (kart aralık dışı satırı atar; sim böyle satır göndermez).
    if (v.data == nullptr || v.angle_grad > 399u || v.n_samples < 200u || v.n_samples > kPingOrnekAzami ||
        v.sample_period < 80u || v.sample_period > 40000u || h.inst > 3u) {
        ++i.say.ping360_gecersiz;
        return;
    }
    if (i.p360_bekleyen.var) {
        ++i.say.ping360_seyreltildi;   // A1-K37: gönderimi başlamamış eski satır atılır
    }
    Ping360Satir& s = i.p360_bekleyen;
    s.var = true;
    s.sonraki = 0;
    s.t_hazir = h.t_teslim_ns;
    s.bas = mavlink_ida_ping360_head_t{};
    s.bas.t_meas_ns = h.t_meas_ns;   // gönderim anı (A1-K53); teslimdeki gecikme değiştirmez
    s.bas.seq = h.seq;
    s.bas.angle_grad = v.angle_grad;
    s.bas.n_samples = v.n_samples;
    s.bas.sample_period = v.sample_period;
    s.bas.tx_duration_us = v.tx_duration_us;
    s.bas.tx_freq_khz = v.tx_freq_khz;
    s.bas.flags = h.flags;
    s.bas.inst = h.inst;
    s.bas.gain = v.gain;
    s.bas.n_frag = static_cast<std::uint8_t>((v.n_samples + kPingParca - 1u) / kPingParca);   // A1-K49
    s.veri.fill(0u);   // son parçanın kullanılmayan kuyruğu 0 (A1 §3.4)
    std::copy(v.data, v.data + v.n_samples, s.veri.begin());
}

std::size_t Kopru::ping360_kuyruk() const
{
    return (ic_->p360_aktif.var ? 1u : 0u) + (ic_->p360_bekleyen.var ? 1u : 0u);
}

// A1-K36 (3) ve (5), A1-K112. `ayrilmis`: adım (3), sonarın ayrılmış payı: kredi yeterse sıradaki
// tek çerçeve. Değilse adım (5): kredi ve B_pencere yettikçe sıradaki çerçeveler. Çerçeveler
// bölünmez; sığmayan sırasını koruyarak sonraki pencereye kalır. Pencere bütçesi, kritik üyelerin
// henüz yazılmamış payı yerine üst sınırı B_krit ile denetlenir (kritik üyeler hiçbir koşulda
// sonar yüzünden kaymaz; A1 3.10 "en kötü pencere").
void Kopru::Ic::sonar_hizmet(bool ayrilmis)
{
    for (;;) {
        Ping360Satir* s = p360_aktif.var ? &p360_aktif : (p360_bekleyen.var ? &p360_bekleyen : nullptr);
        if (s == nullptr) {
            return;
        }
        const bool bas = s->sonraki >= s->bas.n_frag;
        if (bas && p360_bas_bu_pencerede) {
            return;   // A1-K34: partide en çok bir IDA_PING360_HEAD
        }
        mavlink_ida_ping360_data_t d{};
        std::uint32_t msgid = MAVLINK_MSG_ID_IDA_PING360_HEAD;
        const void* m = &s->bas;
        std::size_t mb = sizeof s->bas;
        if (!bas) {
            d.seq = s->bas.seq;
            d.inst = s->bas.inst;
            d.frag_idx = s->sonraki;
            std::copy_n(s->veri.begin() + static_cast<std::ptrdiff_t>(s->sonraki * kPingParca), kPingParca, d.data);
            msgid = MAVLINK_MSG_ID_IDA_PING360_DATA;
            m = &d;
            mb = sizeof d;
        }
        // Kodlanmış boy (kırpılmış yük; A1-K05): gönderici kopyasıyla, seq ilerletilmeden.
        std::size_t boy = 0;
        {
            ida_link_gonderici_t g = gonderici;
            std::uint8_t t[IDA_LINK_CERCEVE_AZAMI];
            if (ida_link_gonder(&g, msgid, m, mb, t, sizeof t, &boy) != IDA_LINK_TAMAM) {
                boy = 0;
            }
        }
        if (boy == 0u || sonar_kredi < static_cast<std::int64_t>(boy) || pen_bayt + kBKrit + boy > kBPencere) {
            return;
        }
        std::size_t n = 0;
        cerceve(msgid, m, mb, &n);
        sonar_kredi -= static_cast<std::int64_t>(n);
        pen_bayt += n;
        ++pen_uye;
        ++say.ping360_cerceve;
        if (s == &p360_bekleyen) {
            // Gönderimi başladı: artık seyreltilemez (A1-K37); yeni satır boşalan yere bekler.
            p360_aktif = p360_bekleyen;
            p360_bekleyen.var = false;
        }
        Ping360Satir& ak = p360_aktif;
        if (bas) {
            // HEAD satırın son çerçevesidir; teslim partisi HEAD'in partisidir (A1-K49, K50).
            p360_bas_bu_pencerede = true;
            ++say.ping360_satir;
            const std::int64_t gec = pen_t_kapanis - ak.t_hazir;
            if (gec > 0) {
                say.ping360_gecikme_en_buyuk =
                    std::max<std::uint64_t>(say.ping360_gecikme_en_buyuk, static_cast<std::uint64_t>((gec + kP - 1) / kP));
            }
            ak.var = false;
        } else {
            ++ak.sonraki;
        }
        if (ayrilmis) {
            return;
        }
    }
}

void Kopru::Ic::pencere_ac(std::int64_t t_onceki)
{
    pen_uye = 0;
    pen_imu = false;
    pen_kritik = 0;
    pen_bayt = 0;
    pen_emniyet.reset();
    if (sonraki_emniyet) {
        pen_emniyet = sonraki_emniyet;
        sonraki_emniyet.reset();
    }
    dar_bu_pencerede.fill(false);
    pen_t_kapanis = t_onceki + kP;
    // A1-K112: sonar kredisi her pencere başında (kuyruk boşken de) birikir.
    sonar_kredi = std::min<std::int64_t>(sonar_kredi + kRs, kCsMax);
    p360_bas_bu_pencerede = false;
    // Bekleyen daraltıcılar: tür kovası izin verdiğinde (A1-K61, K119; sol #10).
    dar_hizmet();
    // A1-K36 (3): sonarın ayrılmış payı (kuyruk boş değilse, kredi yeterse bir tam çerçeve).
    sonar_hizmet(true);
    // Toplu komutlar (A1-K36): t_hazır ≤ t_{n−1}; partide ≤ 4, kredi ve kova içinde.
    kredi = std::min<std::int64_t>(kredi + kRc, kCcMax);
    std::size_t yazilan = 0;
    while (!toplu.empty() && toplu.front().t_hazir <= t_onceki && yazilan < kPartiTopluKomut) {
        Komut& k = toplu.front();
        // A1-K129: genişletici, kabul edildiği revizyon değiştiyse gönderilmez.
        if (k.sinif == Sinif::genisletir && k.rev != emn.emniyet_revizyonu) {
            yerel_sonuc(k.cmd_id, static_cast<std::uint8_t>(IDAW_ACK_REJECTED),
                        static_cast<std::uint16_t>(IDAW_REASON_NARROWED));
            ++say.komut_bayat_yetki;
            toplu.pop_front();
            continue;
        }
        const std::int64_t boy = k.msgid == MAVLINK_MSG_ID_IDA_CMD_GUIDED          ? 56
                                 : k.msgid == MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM ? 59
                                 : k.msgid == MAVLINK_MSG_ID_IDA_CMD_FC           ? 29
                                 : k.msgid == MAVLINK_MSG_ID_IDA_CMD_PARAM_SET    ? 26
                                                                                  : 22;
        if (kredi < boy) {
            break;
        }
        // A1-K119: kova (duvar saati; patlama 8, 5 ms'de 2).
        if (kova_var) {
            kova = std::min(8.0, kova + 2.0 * static_cast<double>(duvar - kova_duvar) / static_cast<double>(kP));
        }
        kova_var = true;
        kova_duvar = duvar;
        if (kova < 1.0) {
            break;
        }
        kova -= 1.0;
        kredi -= boy;
        pen_bayt += komut_yaz(k);
        toplu.pop_front();
        ++yazilan;
    }
    say.komut_ertelendi += toplu.empty() ? 0u : (toplu.front().t_hazir <= t_onceki ? 1u : 0u);
    // A1-K36 (5): sonar kredisi ve B_pencere yettiği sürece sıradaki tam Ping360 çerçeveleri.
    sonar_hizmet(false);
}

std::optional<std::uint32_t> Kopru::parti_kapat(std::int64_t t_n, std::int64_t duvar_ns, std::int64_t w_tn)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    if (i.durum != OturumDurumu::kosuyor) {
        i.pen_uye = 0;
        i.pen_imu = false;
        i.pen_kritik = 0;
        i.pen_bayt = 0;
        i.pen_emniyet.reset();
        i.pen_sensor.clear();
        return std::nullopt;
    }
    std::size_t n = 0;
    // A1-K113 sırası: (1) TF, (2) IMU, (3) emniyet, (4) daraltıcılar (pencerede yazıldı),
    // (5) olay sonuçları, (6) diğer sensörler. (1)–(5) en çok 348 B'tır (E13); (6) B_krit'i
    // aşarsa sırasının sonundan sonraki partiye kayar. Sayım gerçek kodlanmış boyladır (sol #9).
    std::size_t kritik = 29u + i.pen_kritik;   // TF (kırpılmaz; E20) + daraltıcı komutlar
    std::size_t ornek = 0;                     // A1-K34: IMU dışı örnek ≤ 16 (emniyet dahil)
    // (2) IMU: partide 1 (A1-K34); önce kayan IMU. Fazlası IMU olarak sonraki partiye kayar.
    std::vector<SensorUye> diger;     // (6): önce kayanlar kendi sıralarıyla, sonra yeniler
    std::vector<SensorUye> imu_kayan;
    diger.reserve(i.kayan.size() + i.pen_sensor.size());
    auto imu_isle = [&](SensorUye& u) {
        if (!i.pen_imu) {
            kritik += i.uye_yaz(u);
            i.pen_imu = true;
            ++i.pen_uye;
            if (u.gecersiz) {
                ++i.say.gecersiz_ornek;
            }
        } else {
            ++u.kayma;
            ++i.say.kritik_erteleme;
            i.say.kritik_erteleme_en_buyuk = std::max<std::uint64_t>(i.say.kritik_erteleme_en_buyuk, u.kayma);
            imu_kayan.push_back(u);
        }
    };
    for (SensorUye& u : i.kayan) {
        if (u.tur == IDA_SENS_IMU) {
            imu_isle(u);
        } else {
            diger.push_back(u);
        }
    }
    i.kayan.clear();
    std::vector<SensorUye> yeni;
    yeni.reserve(i.pen_sensor.size());
    for (SensorUye& u : i.pen_sensor) {
        if (u.tur == IDA_SENS_IMU) {
            imu_isle(u);
        } else {
            yeni.push_back(u);
        }
    }
    i.pen_sensor.clear();
    i.kayan = std::move(imu_kayan);
    std::stable_sort(yeni.begin(), yeni.end(), [](const SensorUye& x, const SensorUye& y) {
        if (x.t_hazir != y.t_hazir) {
            return x.t_hazir < y.t_hazir;
        }
        if (x.tur != y.tur) {
            return x.tur < y.tur;   // MAG, BARO, GNSS, DVL, PING1D, BATT, LEAK (A4 tür numarası)
        }
        return x.inst < y.inst;
    });
    for (SensorUye& u : yeni) {
        diger.push_back(u);
    }
    // Emniyet örneği (A1-K56).
    if (i.pen_emniyet) {
        const auto& o = *i.pen_emniyet;
        const auto& y = std::get<sensor::EmniyetOlcumu>(o.yuk);
        mavlink_ida_safety_t m{};
        m.t_meas_ns = o.hdr.t_meas_ns;
        m.seq = o.hdr.seq;
        m.safety_rev = y.safety_rev;
        // W4-K12 (2), (3) (EMNİYET): gönderilen durum, gönderilen revizyonun yetkili durumundan
        // alan alan en az o kadar kısıtlayıcı olmalıdır. Revizyon köprünün bütün yetkili
        // geçmişinden seçilir (kaydın revizyonu üst sınır değildir; sol-2 #1): tam eşleşen en
        // yeni; yoksa durumun en az o kadar kısıtlayıcı olduğu en yeni. Hiçbiri yoksa kaydın
        // revizyonu kalır, durum o revizyonun yetkili durumuyla birleştirilir ve sayılır.
        {
            const std::uint8_t kod = emniyet_kodu(y);
            std::uint32_t secilen = m.safety_rev;
            bool tam = false;
            bool bulundu = false;
            std::uint32_t kisit_rev = 0;
            for (auto r = i.rev_girdi.rbegin(); r != i.rev_girdi.rend(); ++r) {
                if ((kod & 0x1Fu) == (r->second.girdi & 0x1Fu) &&
                    ((kod & 0x60u) & static_cast<std::uint8_t>(~r->second.izin_acilan)) == 0u) {
                    secilen = r->first;
                    tam = true;
                    break;
                }
                if (!bulundu && en_az_kisitlayici(kod, r->second)) {
                    kisit_rev = r->first;
                    bulundu = true;
                }
            }
            if (!tam && bulundu) {
                secilen = kisit_rev;
            }
            m.estop = y.estop;
            m.tube[0] = y.tube[0];
            m.tube[1] = y.tube[1];
            if (!tam && !bulundu) {
                // W4-K15 (5): yetkili geçmişte uygun revizyon yok (kaydın revizyonu tablodan
                // atılmış olabilir). Değişmez (2) korunur: durum güncel yetkili durumla alan alan
                // en kısıtlayıcı biçimde birleştirilir ve güncel revizyonla gönderilir.
                ++i.say.emniyet_rev_eslesmedi;
                secilen = i.emn.emniyet_revizyonu;
                {
                    const std::uint8_t yet = static_cast<std::uint8_t>((i.girdi_kodu() & 0x1Fu) | i.izin_kodu());
                    if ((yet & 1u) != 0u) {
                        m.estop = 1u;
                    }
                    for (std::size_t k = 0; k < 2; ++k) {
                        std::uint8_t tb = m.tube[k];
                        if ((yet & (2u << k)) == 0u) {
                            tb = static_cast<std::uint8_t>(tb & static_cast<std::uint8_t>(~IDAW_TUBE_FLAGS_PLUG_IN));
                        }
                        if ((yet & (8u << k)) == 0u) {
                            tb = static_cast<std::uint8_t>(tb & static_cast<std::uint8_t>(~IDAW_TUBE_FLAGS_PIN_OUT));
                        }
                        if ((yet & (32u << k)) == 0u) {
                            tb = static_cast<std::uint8_t>(tb & static_cast<std::uint8_t>(~IDAW_TUBE_FLAGS_PERMIT_OPEN));
                        }
                        m.tube[k] = tb;
                    }
                }
            }
            if (secilen != m.safety_rev) {
                m.safety_rev = secilen;
                ++i.say.emniyet_rev_duzeltme;
            }
        }
        m.flags = o.hdr.flags;
        m.inst = o.hdr.inst;
        i.cerceve(MAVLINK_MSG_ID_IDA_SAFETY, &m, sizeof m, &n);
        ++i.pen_uye;
        kritik += n;
        ++ornek;
        i.pen_emniyet.reset();
    }
    // Olay sonuçları (A1-K111): yeni sonuçlar ≤ 4, sonra ≤ 1 saklı sonuç talebi.
    std::size_t s = 0;
    while (!i.sonuc_kuyrugu.empty() && s < kPartiSonuc) {
        mavlink_ida_event_result_t r = i.sonuc_kuyrugu.front();
        i.sonuc_kuyrugu.pop_front();
        r.sess = i.sess_;
        i.cerceve(MAVLINK_MSG_ID_IDA_EVENT_RESULT, &r, sizeof r, &n);
        ++i.pen_uye;
        kritik += n;
        ++s;
    }
    if (!i.sakli_talep.empty()) {
        if (s < kPartiSonuc) {
            const std::uint64_t id = i.sakli_talep.front();
            const SakliSonuc& ss = i.sakli[id];
            mavlink_ida_event_result_t r{};
            r.event_id = id;
            r.result = ss.sonuc == IDA_EVR_APPLIED ? static_cast<std::uint8_t>(IDA_EVR_DUPLICATE) : ss.sonuc;
            r.t_apply_ns = ss.t_apply;
            r.sess = i.sess_;
            i.cerceve(MAVLINK_MSG_ID_IDA_EVENT_RESULT, &r, sizeof r, &n);
            ++i.pen_uye;
            kritik += n;
        } else {
            ++i.say.sonuc_talebi_atildi;
        }
        i.sakli_talep.pop_front();
        i.say.sonuc_talebi_atildi += i.sakli_talep.size();
        i.sakli_talep.clear();
    }
    // (6) Diğer sensörler: B_krit ve A1-K34 örnek sınırı içinde; sığmayanlar sırasının sonundan
    // başlayarak sonraki partiye kayar (sıra korunur).
    for (SensorUye& u : diger) {
        if (kritik + u.boy <= kBKrit && ornek < IDA_PORT_MAX_SAMPLES) {
            kritik += i.uye_yaz(u);
            ++i.pen_uye;
            ++ornek;
            if (u.gecersiz) {
                ++i.say.gecersiz_ornek;
            }
        } else {
            ++u.kayma;
            ++i.say.kritik_erteleme;
            i.say.kritik_erteleme_en_buyuk = std::max<std::uint64_t>(i.say.kritik_erteleme_en_buyuk, u.kayma);
            i.kayan.push_back(u);
        }
    }
    i.say.kritik_bayt_en_buyuk = std::max<std::uint64_t>(i.say.kritik_bayt_en_buyuk, kritik);
    if (i.pen_bayt + kritik > kBPencere) {   // A1-K34, H20
        ++i.say.pencere_tasma;
        i.say.calistirma_gecersiz |= kGecPencereTasma;
    }
    // TF(n). batch_seq 2^32'de sarar; 0 geçerli bir değerdir (A1-K29, K123; sol #12).
    mavlink_ida_time_frame_t tf{};
    tf.t_sim_ns = t_n;
    tf.batch_seq = i.sonraki_seq++;
    tf.sess = i.sess_;
    tf.n_members = static_cast<std::uint8_t>(std::min<std::uint16_t>(i.pen_uye, 255u));
    tf.tf_flags = static_cast<std::uint8_t>(i.pen_imu ? IDAW_TF_FLAGS_IMU : 0u);
    tf.run_mode = static_cast<std::uint8_t>(i.hazirlik_gerekli ? CalistirmaKipi::hazirlik : i.a.kip);
    i.cerceve(MAVLINK_MSG_ID_IDA_TIME_FRAME, &tf, sizeof tf);
    {
        // Parti boyu (TF dahil; W4-bagla-sonar kabul 6): yalnız sayaç, akışı değiştirmez.
        const std::uint64_t bayt = i.pen_bayt + kritik;
        i.say.parti_bayt_en_buyuk = std::max<std::uint64_t>(i.say.parti_bayt_en_buyuk, bayt);
        i.say.parti_uye_en_buyuk = std::max<std::uint64_t>(i.say.parti_uye_en_buyuk, i.pen_uye);
        ++i.say.parti_bayt_dagilim[std::min<std::size_t>(static_cast<std::size_t>(bayt / 100u),
                                                         i.say.parti_bayt_dagilim.size() - 1u)];
    }
    i.tf_var = true;
    i.son_seq = tf.batch_seq;
    i.tf_kayit.koy(tf.batch_seq, TfKaydi{t_n, duvar_ns});
    i.yuva_sira.push_back(tf.batch_seq);
    Yuva& y = i.yuvalar[tf.batch_seq];
    y = Yuva{};
    y.t_n = t_n;
    y.adim = static_cast<std::uint64_t>((t_n + kLact) / kH);
    if (w_tn >= 0) {
        y.son_tarih = w_tn + kLact - kCSim;   // D_n = W(a_n) − C_sim (A1-K108)
        const std::int64_t gec = duvar_ns - w_tn;
        if (gec > 0) {
            i.say.tf_gec_yazim_ns_en_buyuk =
                std::max<std::uint64_t>(i.say.tf_gec_yazim_ns_en_buyuk, static_cast<std::uint64_t>(gec));
        }
    }
    i.adim_yuva[y.adim] = tf.batch_seq;
    ++i.say.parti_gonderilen;
    // Sonraki pencere: TF(n)'den hemen sonra toplu üyeler (A1-K39).
    i.pencere_ac(t_n);
    return tf.batch_seq;
}

bool Kopru::yuva_tamam(std::uint32_t seq) const
{
    const auto it = ic_->yuvalar.find(seq);
    return it != ic_->yuvalar.end() && ic_->yuva_tamam(it->second);
}

std::optional<YanitBilgisi> Kopru::yanit(std::uint32_t seq) const
{
    const mavlink_ida_actuator_t* a = ic_->gecen_yanit.bul(seq);
    if (a == nullptr) {
        return std::nullopt;
    }
    YanitBilgisi y;
    y.act_flags = a->act_flags;
    for (std::size_t k = 0; k < 8; ++k) {
        y.pwm_us[k] = a->pwm_us[k];
    }
    y.fs_level = a->fs_level;
    y.core_rc = a->core_rc;
    y.out_seq = a->out_seq;
    if (const std::int64_t* ys = ic_->yanit_sure.bul(seq)) {
        y.yanit_suresi_ns = *ys;
    }
    return y;
}

void Kopru::sim_gecikmesi_bildir()
{
    ++ic_->say.sim_gecikmesi;
    ic_->say.calistirma_gecersiz |= kGecSimGecikmesi;
}

const std::vector<std::int64_t>& Kopru::yanit_sureleri() const { return ic_->yanit_sureleri; }
const std::vector<std::int64_t>& Kopru::sira_boslugu_zamanlari() const { return ic_->sira_boslugu_t; }

std::uint32_t Kopru::son_parti() const { return ic_->tf_var ? ic_->son_seq : 0u; }
bool Kopru::parti_gonderildi() const { return ic_->tf_var; }

// ---------------------------------------------------------------- Komutlar

std::size_t Kopru::Ic::komut_yaz(Komut& k)
{
    std::size_t n = 0;
    switch (k.msgid) {
    case MAVLINK_MSG_ID_IDA_CMD_SIMPLE:
        k.basit.sess = sess_;
        cerceve(k.msgid, &k.basit, sizeof k.basit, &n);
        break;
    case MAVLINK_MSG_ID_IDA_CMD_GUIDED:
        k.guided.sess = sess_;
        cerceve(k.msgid, &k.guided, sizeof k.guided, &n);
        break;
    case MAVLINK_MSG_ID_IDA_CMD_PARAM_SET:
        k.param.sess = sess_;
        cerceve(k.msgid, &k.param, sizeof k.param, &n);
        break;
    case MAVLINK_MSG_ID_IDA_CMD_FC:
        k.fc.sess = sess_;
        cerceve(k.msgid, &k.fc, sizeof k.fc, &n);
        break;
    case MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM:
        k.gorev.sess = sess_;
        cerceve(k.msgid, &k.gorev, sizeof k.gorev, &n);
        break;
    default:
        return 0;
    }
    ++pen_uye;
    oturum_komutlari.insert(k.cmd_id);
    sonuc_bekleyen.insert(k.cmd_id);
    return n;
}

void Kopru::Ic::dar_hizmet()
{
    if (durum != OturumDurumu::kosuyor) {
        return;
    }
    for (std::size_t t = 0; t < dar_bekleyen.size(); ++t) {
        if (!dar_bekleyen[t]) {
            continue;
        }
        dar_doldur(t);
        if (dar_kova[t] >= 1.0 && !dar_bu_pencerede[t]) {
            Komut k = *dar_bekleyen[t];
            dar_bekleyen[t].reset();
            dar_kova[t] -= 1.0;
            pen_kritik += komut_yaz(k);
            dar_bu_pencerede[t] = true;
        }
    }
}

std::uint32_t Kopru::Ic::komut_ekle(Komut k)
{
    k.cmd_id = c;
    ++c;
    if (c == 0u) {
        c = 1u;
    }
    k.basit.cmd_id = k.guided.cmd_id = k.param.cmd_id = k.fc.cmd_id = k.gorev.cmd_id = k.cmd_id;
    if (durum != OturumDurumu::kosuyor) {
        yerel_sonuc(k.cmd_id, static_cast<std::uint8_t>(IDAW_ACK_REJECTED),
                    static_cast<std::uint16_t>(IDAW_REASON_LINK_SESSION_END));
        ++say.komut_oturumsuz;
        return k.cmd_id;
    }
    if (k.sinif == Sinif::daraltir) {
        ++dar_komut_bekleyen;   // A1-K106 (1): sonraki fizik sınırında revizyon artar
        const auto t = static_cast<std::size_t>(k.dar_tur);
        dar_doldur(t);
        if (!dar_bu_pencerede[t] && dar_kova[t] >= 1.0 && !dar_bekleyen[t]) {
            dar_kova[t] -= 1.0;
            pen_kritik += komut_yaz(k);   // kritik üye: t_hazır'da yazılır (A1-K35, K39)
            dar_bu_pencerede[t] = true;
        } else {
            if (dar_bekleyen[t]) {
                // A1-K61: öncekini sim birleştirir.
                yerel_sonuc(dar_bekleyen[t]->cmd_id, static_cast<std::uint8_t>(IDAW_ACK_ACCEPTED),
                            static_cast<std::uint16_t>(IDAW_REASON_COALESCED));
                ++say.komut_birlestirildi;
            }
            dar_bekleyen[t] = k;
        }
        return k.cmd_id;
    }
    k.rev = emn.emniyet_revizyonu;
    toplu.push_back(k);
    return k.cmd_id;
}

std::uint32_t Kopru::komut_basit(std::uint8_t type, std::uint16_t arg, std::int64_t t_ns, std::uint8_t kaynak)
{
    Ic& i = *ic_;
    Komut k;
    k.msgid = MAVLINK_MSG_ID_IDA_CMD_SIMPLE;
    k.t_hazir = t_ns;
    k.basit.type = type;
    k.basit.arg = arg;
    k.basit.source = kaynak;
    if (type == IDAW_CMD_TYPE_DISARM) {
        k.sinif = Sinif::daraltir;
        k.dar_tur = 0;
    } else if (type == IDAW_CMD_TYPE_SET_MODE && arg >= 2u && arg <= 5u) {
        k.sinif = Sinif::daraltir;
        k.dar_tur = 1;
    } else if (type == IDAW_CMD_TYPE_ARM || type == IDAW_CMD_TYPE_FAILSAFE_RESET ||
               (type == IDAW_CMD_TYPE_SET_MODE && (arg == 6u || arg == 7u))) {
        k.sinif = Sinif::genisletir;
    }
    return i.komut_ekle(k);
}

std::uint32_t Kopru::komut_guided(const GuidedHedef& h, std::int64_t t_ns, std::uint8_t kaynak)
{
    Komut k;
    k.msgid = MAVLINK_MSG_ID_IDA_CMD_GUIDED;
    k.t_hazir = t_ns;
    k.guided.mask = h.mask;
    k.guided.pos_ne[0] = h.pos_ne[0];
    k.guided.pos_ne[1] = h.pos_ne[1];
    k.guided.depth = h.depth;
    k.guided.altitude = h.altitude;
    for (std::size_t j = 0; j < 3; ++j) {
        k.guided.vel_ned[j] = h.vel_ned[j];
    }
    k.guided.yaw = h.yaw;
    k.guided.yaw_rate = h.yaw_rate;
    k.guided.source = kaynak;
    return ic_->komut_ekle(k);
}

std::uint32_t Kopru::komut_gorev_ogesi(const GorevOgesi& g, std::int64_t t_ns, std::uint8_t kaynak)
{
    Komut k;
    k.msgid = MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM;
    k.t_hazir = t_ns;   // nötr sınıf (A1 Tablo 3.6): toplu
    k.gorev.index = g.index;
    k.gorev.item_type = g.item_type;
    k.gorev.frame = g.frame;
    for (std::size_t j = 0; j < 4; ++j) {
        k.gorev.p[j] = g.p[j];
    }
    k.gorev.lat_e7 = g.lat_e7;
    k.gorev.lon_e7 = g.lon_e7;
    k.gorev.north_m = g.north_m;
    k.gorev.east_m = g.east_m;
    k.gorev.depth = g.depth;
    k.gorev.source = kaynak;
    return ic_->komut_ekle(k);
}

std::uint32_t Kopru::komut_param_set(std::uint16_t param_id, std::uint8_t param_type, std::uint32_t value_bits,
                                     std::int64_t t_ns, std::uint8_t kaynak)
{
    Komut k;
    k.msgid = MAVLINK_MSG_ID_IDA_CMD_PARAM_SET;
    k.t_hazir = t_ns;
    k.param.param_id = param_id;
    k.param.param_type = param_type;
    k.param.value_bits = value_bits;
    k.param.source = kaynak;
    return ic_->komut_ekle(k);
}

std::uint32_t Kopru::komut_fc(std::uint8_t op, std::uint8_t tube, std::int64_t t_ns, std::uint8_t kaynak)
{
    Komut k;
    k.msgid = MAVLINK_MSG_ID_IDA_CMD_FC;
    k.t_hazir = t_ns;
    k.fc.op = op;
    k.fc.tube = tube;
    k.fc.source = kaynak;
    if (op == IDAW_FC_OP_DISARM) {
        k.sinif = Sinif::daraltir;
        k.dar_tur = 2;
        k.fc.tube = 255u;
    } else if (op == IDAW_FC_OP_ABORT) {
        k.sinif = Sinif::daraltir;
        k.dar_tur = 3;
        k.fc.tube = 255u;
    } else if (op == IDAW_FC_OP_ARM || op == IDAW_FC_OP_FIRE || op == IDAW_FC_OP_RESET ||
               op == IDAW_FC_OP_TEST_ENTER) {
        k.sinif = Sinif::genisletir;
    }
    return ic_->komut_ekle(k);
}

}  // namespace ida::kopru
