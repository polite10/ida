// sol düzeltme turu, B: #6 (sim gecikmesi), #9 (parti bütçeleri), #10 (daraltıcı kovası),
// #12 (batch_seq sarması) karşı örnekleri.
#include "test_destek.hpp"

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

ida::sensor::Olcum mag(std::uint32_t seq, std::int64_t t, std::uint8_t inst)
{
    ida::sensor::Olcum o;
    o.hdr.tur = ida::sensor::SensorTuru::manyetometre;
    o.hdr.seq = seq;
    o.hdr.t_meas_ns = t;
    o.hdr.t_teslim_ns = t;
    o.hdr.inst = inst;
    o.hdr.flags = ida::sensor::kBayrakGecerli;
    ida::sensor::MagOlcumu y;
    y.field = {2e-5F, -3e-6F, 4e-5F};
    o.yuk = y;
    return o;
}

std::size_t say_msg(const std::vector<std::uint32_t>& u, std::uint32_t msgid)
{
    std::size_t n = 0;
    for (std::uint32_t m : u) {
        n += m == msgid ? 1u : 0u;
    }
    return n;
}

}  // namespace

// #6 (A1-K108, K79): yalnız sim gecikmesi bulunan çalıştırma geçersiz işaretlenir.
int test_sim_gecikmesi()
{
    Denetci d;
    Kopru k{KopruAyari{}};
    DOGRULA(d, k.sayaclar().calistirma_gecersiz == 0u, "başta geçerli");
    k.sim_gecikmesi_bildir();
    DOGRULA(d, k.sayaclar().sim_gecikmesi == 1u, "ortak sayaç");
    DOGRULA(d, k.sayaclar().calistirma_gecersiz == kGecSimGecikmesi, "yalnız sim gecikmesi biti");
    return d.hata;
}

// #9 (A1-K34, K113): bir pencerede 20 manyetometre örneği (4 inst) ve dört daraltıcı komut:
// kritik bayt 520'yi, örnek sayısı 16'yı aşmaz; fazlası sırasıyla sonraki partiye kayar ve
// hiçbiri kaybolmaz; daraltıcı komutların baytları sayıma girer.
int test_parti_butcesi()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    (void)z.periyot();
    const std::size_t bas = z.sk.parti_uyeleri.size();
    // Pencere açık: dört daraltıcı tür (A1-K61) ve 20 örnek.
    (void)z.k.komut_basit(static_cast<std::uint8_t>(IDAW_CMD_TYPE_DISARM), 0, z.t);
    (void)z.k.komut_basit(static_cast<std::uint8_t>(IDAW_CMD_TYPE_SET_MODE), 3, z.t);
    (void)z.k.komut_fc(static_cast<std::uint8_t>(IDAW_FC_OP_DISARM), 255, z.t);
    (void)z.k.komut_fc(static_cast<std::uint8_t>(IDAW_FC_OP_ABORT), 255, z.t);
    std::vector<ida::sensor::Olcum> v;
    for (std::uint32_t j = 0; j < 20; ++j) {
        v.push_back(mag(100 + j / 4, z.t + 1'000'000 + static_cast<std::int64_t>(j / 4), static_cast<std::uint8_t>(j % 4)));
    }
    v.push_back(imu_olcumu(1, z.t + 1'000'000));   // iki IMU: partide 1 (A1-K34), ikincisi kayar
    v.push_back(imu_olcumu(2, z.t + 2'000'000));
    (void)z.periyot(v);
    (void)z.periyot();
    (void)z.periyot();
    std::size_t toplam = 0;
    std::size_t ilk = 0;
    for (std::size_t p = bas; p < z.sk.parti_uyeleri.size(); ++p) {
        const std::size_t n = say_msg(z.sk.parti_uyeleri[p], MAVLINK_MSG_ID_IDA_MAG);
        if (p == bas) {
            ilk = n;
        }
        toplam += n;
        DOGRULA(d, n <= 16u, "partide ≤ 16 örnek (A1-K34): " + s(n));
    }
    DOGRULA(d, toplam == 20u, "hiçbir örnek kaybolmadı (" + s(toplam) + ")");
    DOGRULA(d, say_msg(z.sk.parti_uyeleri[bas], MAVLINK_MSG_ID_IDA_IMU) == 1u &&
                   say_msg(z.sk.parti_uyeleri[bas + 1], MAVLINK_MSG_ID_IDA_IMU) == 1u,
            "partide 1 IMU; ikincisi sonraki partide");
    DOGRULA(d, ilk < 20u, "bir kısmı kaydı (ilk partide " + s(ilk) + ")");
    DOGRULA(d, z.k.sayaclar().kritik_bayt_en_buyuk <= 520u,
            "kritik bayt ≤ B_krit (en büyük " + s(z.k.sayaclar().kritik_bayt_en_buyuk) + ")");
    DOGRULA(d, z.k.sayaclar().kritik_erteleme >= 20u - ilk, "kayanlar sayıldı");
    DOGRULA(d, say_msg(z.sk.parti_uyeleri[bas], MAVLINK_MSG_ID_IDA_CMD_SIMPLE) == 2u &&
                   say_msg(z.sk.parti_uyeleri[bas], MAVLINK_MSG_ID_IDA_CMD_FC) == 2u,
            "dört daraltıcı kendi partisinde (kaymaz)");
    return d.hata;
}

// #10 (A1-K61, K119): aynı türden iki daraltıcı arasında pencere açılışından bağımsız en az
// 5 ms (duvar) bulunur. İlki pencerenin son sınırında gider; ikincisi yeni pencere açılınca
// değil, 5 ms dolunca.
int test_daraltici_kovasi()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    (void)z.periyot();
    for (int j = 0; j < 3; ++j) {
        z.sinir_ilerle();
    }
    const std::int64_t t0 = z.t;
    const std::size_t bas = z.sk.komutlar.size();
    (void)z.k.komut_basit(static_cast<std::uint8_t>(IDAW_CMD_TYPE_DISARM), 0, z.t);
    (void)z.k.komut_basit(static_cast<std::uint8_t>(IDAW_CMD_TYPE_DISARM), 0, z.t);
    alisveris(z.k, z.sk, z.t);
    DOGRULA(d, z.sk.komutlar.size() == bas + 1u, "ilk daraltıcı hemen gitti");
    // Pencereyi kapat (yeni pencere açılır): ikinci daraltıcı henüz gitmemeli.
    z.sinir_ilerle();
    (void)z.k.parti_kapat(z.t, z.t);
    alisveris(z.k, z.sk, z.t);
    DOGRULA(d, z.sk.komutlar.size() == bas + 1u, "pencere açılışı kovayı yenilemedi");
    std::int64_t t_ikinci = -1;
    for (int j = 0; j < 6 && t_ikinci < 0; ++j) {
        z.sinir_ilerle();
        if (z.sk.komutlar.size() == bas + 2u) {
            t_ikinci = z.t;
        }
    }
    DOGRULA(d, t_ikinci >= t0 + 5'000'000, "ikinci daraltıcı ≥ 5 ms sonra (" + s(static_cast<std::uint64_t>(t_ikinci - t0)) + " ns)");
    return d.hata;
}

// #12 (A1-K29, K123): batch_seq 2³²'de sarar, 0 atlanmaz; olayların t_decide doğrulaması ve
// yuvalar sarmadan sonra da doğru.
int test_batch_seq_sarmasi()
{
    Denetci d;
    KopruAyari ka;
    ka.ilk_batch_seq = 0xFFFFFFF8u;
    SahteDuzenek z(ka);
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    EmniyetAtama a;
    a.fis = {true, true};
    a.pim = {false, false};
    z.k.emniyet_ata(a);
    std::uint64_t acma = 0;
    std::uint32_t acma_seq = 1;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        std::uint8_t n_ev = 0;
        if (tf.seq == 0u) {
            acma = z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, z.k.dis_durum().emniyet.emniyet_revizyonu);
            acma_seq = tf.seq;
            n_ev = 1;
        }
        std::array<std::uint16_t, 8> p{};
        p.fill(static_cast<std::uint16_t>(1500u + (tf.seq & 0x3Fu)));
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_ARMED), p, n_ev);
    };
    std::vector<std::uint32_t> seqler;
    std::vector<std::uint64_t> a_adim;
    for (int n = 0; n < 20; ++n) {
        (void)z.periyot();
        seqler.push_back(z.k.son_parti());
        a_adim.push_back(z.sinir + kLactAdim);
    }
    bool ardisik = true;
    bool sifir = false;
    for (std::size_t j = 1; j < seqler.size(); ++j) {
        ardisik = ardisik && seqler[j] == seqler[j - 1] + 1u;
        sifir = sifir || seqler[j] == 0u;
    }
    DOGRULA(d, ardisik && sifir, "TF'ler 0 dahil birer artıyor");
    std::uint32_t hata_adim = 0;
    for (std::size_t j = 0; j + 2 < seqler.size(); ++j) {
        if (z.sinirlar[a_adim[j]].pwm_us[0] != static_cast<double>(1500u + (seqler[j] & 0x3Fu))) {
            ++hata_adim;
        }
    }
    DOGRULA(d, hata_adim == 0u, "sarmadan sonra yanıtlar kendi a_n'inde uygulandı (" + s(hata_adim) + " hata)");
    DOGRULA(d, acma_seq == 0u, "olay batch_seq = 0 partisinde gönderildi");
    std::uint8_t sonuc = 0;
    for (const auto& r : z.sk.sonuclar) {
        if (r.event_id == acma) {
            sonuc = r.result;
        }
    }
    DOGRULA(d, sonuc == IDA_EVR_APPLIED, "batch_seq = 0'daki olay doğru t_decide ile uygulandı (" + s(sonuc) + ")");
    DOGRULA(d, z.k.sayaclar().bag_ret == 0u && z.k.sayaclar().parti_kacirildi == 0u, "bağ reddi ve kaçırma yok");
    return d.hata;
}
