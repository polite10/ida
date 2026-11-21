// tests/kabul/kopru — sunucu düzeyindeki vakalar: ida::sunucu::kos() (SIL, TCP, senaryo "bos")
// ile sahte kart. Fiziğe giden darbe genişliği sunucu.hpp'deki Satir::pwm_fiz'dir ("aynı adımda
// fiziğin uyguladığı (engel ve arızadan sonra)"); her kontrol adımında bir satır.
//
// Vaka 6 (W3-K1, iskelet §5): acil durdurma etkinken kartın komutu ne olursa olsun fiziğe giden
// değer nötrdür; acil durdurma kalkınca eski komut kendiliğinden geri gelmez (EMNİYET okuması).
// Vaka 8 (A1-K56): gerçek sensör kümesi + köprü zinciriyle aynı pencerede acil durdurma darbesi.
// Vaka 9: acil durdurma etkinken ≥ 100 000 rastgele kart çerçevesi; nötr dışı değer yok.
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "ida/kopru/param_blogu.hpp"
#include "ida/kopru/tasima.hpp"
#include "ida/port.h"
#include "ida/sunucu/sunucu.hpp"
#include "ortak.hpp"
#include "rastgele.hpp"

namespace kabul {
namespace {

constexpr std::int32_t kNotr = 1500;
constexpr std::uint16_t kTamItki = 1850;

// Hazırlık oturumunda (A1-K19) alınan PARAM_SET'lerden A5 bloğunun CRC'si: varsayılandan farklı
// değerler adla parametre_blogu'na verilir (param_blogu.hpp, açık arayüz).
std::uint32_t blok_crc(const std::vector<GelenKomut>& ayarlar) {
    std::vector<std::pair<std::string, double>> degisen;
    const std::uint16_t n = ida_core_param_count();
    for (const auto& k : ayarlar) {
        for (std::uint16_t i = 0; i < n; ++i) {
            ida_param_desc_t pd{};
            if (ida_core_param_desc(i, &pd) != IDA_RC_OK || pd.id != k.param_id) {
                continue;
            }
            std::uint32_t def_bits = 0;
            std::memcpy(&def_bits, &pd.def.v, sizeof def_bits);
            if (def_bits == k.value_bits) {
                break;
            }
            double v = 0.0;
            if (pd.type == IDA_PT_F32) {
                float f = 0.0F;
                std::memcpy(&f, &k.value_bits, sizeof f);
                v = static_cast<double>(f);
            } else if (pd.type == IDA_PT_I32) {
                std::int32_t x = 0;
                std::memcpy(&x, &k.value_bits, sizeof x);
                v = static_cast<double>(x);
            } else {
                v = static_cast<double>(k.value_bits);
            }
            degisen.emplace_back(pd.name, v);
            break;
        }
    }
    return ida::kopru::parametre_blogu(degisen).crc;
}

class SunucuKosusu {
public:
    using Yanit = std::function<void(SahteKart&, const GelenTf&, std::int64_t)>;

    explicit SunucuKosusu(ida::sunucu::Ayar a) : ayar(std::move(a)), kart(kart_ayari()) {}

    static SahteKart::Ayar kart_ayari() {
        SahteKart::Ayar k;
        k.rol = w::kRolSil;
        k.blok_crc = ida::kopru::parametre_blogu({}).crc;
        return k;
    }

    bool kos(const Yanit& yanit) {
        std::string hata;
        {
            auto dinleyici = ida::kopru::tcp_dinle(0, hata);
            if (!dinleyici) {
                std::fprintf(stderr, "port seçilemedi: %s\n", hata.c_str());
                return false;
            }
            ayar.a1_port = dinleyici->port();
        }
        ayar.kip = ida::sunucu::Kip::sil;
        ayar.karsi = ida::sunucu::Karsi::tcp;
        ayar.sil_yolu.clear();
        ayar.csv_yolu.clear();
        ayar.gunluk = 0;
        kart.tf_geldi = [this, &yanit](const GelenTf& tf, std::int64_t t) {
            yanit(kart, tf, t);
            durum_gonder(tf, t);
        };
        kart.komut_geldi = [this](const GelenKomut& k, std::int64_t t) { komut_yanitla(k, t); };
        std::atomic<bool> bitti{false};
        std::thread is([this, &bitti] {
            sonuc = ida::sunucu::kos(ayar);
            bitti = true;
        });
        std::vector<std::uint8_t> tampon(1u << 16);
        // SIL'de her yeni TCP bağlantısı yeni bir akıştır (A1-K99): sim bağlantıyı kapatırsa
        // sahte kart (aynı açılışta) yeniden bağlanır.
        while (!bitti) {
            std::unique_ptr<ida::kopru::Tasima> tas;
            for (int i = 0; i < 4000 && !tas && !bitti; ++i) {
                tas = ida::kopru::tcp_baglan(ayar.a1_port, hata);
                if (!tas) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
            if (!tas) {
                break;
            }
            ++baglanti;
            kart.giden.clear();
            while (true) {
                std::size_t okunan = 0;
                const auto s = tas->oku(tampon.data(), tampon.size(), 2000, okunan);
                if (s == ida::kopru::OkumaSonucu::kapandi) {
                    break;
                }
                if (s == ida::kopru::OkumaSonucu::tamam) {
                    kart.al(tampon.data(), okunan, t_son());
                    while (!kart.giden.empty()) {
                        const auto& g = kart.giden.front();
                        if (!g.bayt.empty() && !tas->yaz(g.bayt.data(), g.bayt.size())) {
                            break;
                        }
                        kart.giden.pop_front();
                    }
                } else if (bitti) {
                    break;
                }
            }
        }
        is.join();
        if (!sonuc.tamam) {
            std::fprintf(stderr, "kos: tamam değil: %s\n", sonuc.hata.c_str());
        }
        return sonuc.tamam;
    }

    // Son oturumun partileri (hazırlık oturumu hariç).
    std::vector<const Parti*> son_oturum() const {
        std::vector<const Parti*> v;
        for (const auto& p : kart.partiler) {
            if (p.tf.sess == kart.sess()) {
                v.push_back(&p);
            }
        }
        return v;
    }

    ida::sunucu::Ayar ayar;
    ida::sunucu::Sonuc sonuc;
    SahteKart kart;
    std::uint64_t hazirlik = 0;
    std::uint64_t baglanti = 0;

private:
    std::int64_t t_son() const { return kart.partiler.empty() ? 0 : kart.partiler.back().tf.t; }

    void durum_gonder(const GelenTf& tf, std::int64_t t) {
        mavlink_ida_status_t s{};
        s.boot_count = kart.boot_count();
        s.param_gen = 1;
        s.param_blob_crc = kart.ayar().blok_crc;
        s.batch_seq = tf.seq;
        s.sess = kart.sess();
        s.mode = 1;
        s.param_ok = 1;
        s.events_ok = 1;
        s.health = 1;
        s.link_state = w::kLinkRunning;
        s.param_store = 2;  // IDA_PSTORE_OK
        kart.gonder(MAVLINK_MSG_ID_IDA_STATUS, &s, sizeof s, t);
    }

    void komut_yanitla(const GelenKomut& k, std::int64_t t) {
        if (k.msgid != MAVLINK_MSG_ID_IDA_CMD_SIMPLE && k.msgid != MAVLINK_MSG_ID_IDA_CMD_PARAM_SET) {
            return;
        }
        mavlink_ida_cmd_result_t r{};
        r.cmd_id = k.cmd_id;
        r.sess = k.sess;
        r.kind = 1;  // ACK
        r.ack = 1;   // ACCEPTED
        if (k.msgid == MAVLINK_MSG_ID_IDA_CMD_PARAM_SET) {
            ayarlar_.push_back(k);
            r.param_id = k.param_id;
        } else if (k.type == 14) {  // PARAM_SAVE
            ++hazirlik;
            const std::uint32_t crc = blok_crc(ayarlar_);
            kart.ayar_degistir().blok_crc = crc;
            r.kind = 3;  // PARAM_SAVE
            r.save_gen = 2;
            r.save_len = static_cast<std::uint16_t>(24u + 8u * ayarlar_.size());
            r.save_crc = crc;
        }
        kart.gonder(MAVLINK_MSG_ID_IDA_CMD_RESULT, &r, sizeof r, t);
    }

    std::vector<GelenKomut> ayarlar_;
};

SunucuKosusu::Yanit tam_itki() {
    return [](SahteKart& k, const GelenTf& tf, std::int64_t t) {
        k.yanit_gonder(k.yanit(tf, kTamItki, 0), t);
    };
}

bool hepsi(const ida::sunucu::Satir& s, std::int32_t deger) {
    for (const auto p : s.pwm_fiz) {
        if (p != deger) {
            return false;
        }
    }
    return true;
}

void ozet(const SunucuKosusu& k) {
    const auto& s = k.sonuc.sayac;
    std::printf("satır=%zu estop_adim=%llu ihlal_estop=%llu hazırlık=%llu alan_ret=%llu "
                "bag_ret=%llu oturum_kurulan=%llu\n",
                k.sonuc.satirlar.size(), static_cast<unsigned long long>(k.sonuc.estop_adim),
                static_cast<unsigned long long>(k.sonuc.ihlal_estop),
                static_cast<unsigned long long>(k.sonuc.hazirlik_oturumu),
                static_cast<unsigned long long>(s.alan_ret), static_cast<unsigned long long>(s.bag_ret),
                static_cast<unsigned long long>(s.oturum_kurulan));
    std::printf("yanit_ret: yinelenen=%llu gec=%llu oturum=%llu eski_uretim=%llu; kacirildi=%llu "
                "batch_incomplete=%llu rx_crc_err=%llu\n",
                static_cast<unsigned long long>(s.yanit_ret_yinelenen),
                static_cast<unsigned long long>(s.yanit_ret_gec),
                static_cast<unsigned long long>(s.yanit_ret_oturum),
                static_cast<unsigned long long>(s.yanit_ret_eski_uretim),
                static_cast<unsigned long long>(s.parti_kacirildi),
                static_cast<unsigned long long>(s.batch_incomplete),
                static_cast<unsigned long long>(s.rx_crc_err));
}

ida::sunucu::Ayar bos_senaryo(double sure, double acil, double kalk) {
    ida::sunucu::Ayar a;
    a.senaryo = "bos";
    a.sure_s = sure;
    a.acil_durdurma_s = acil;
    a.acil_kalkar_s = kalk;
    a.tohum = 7;
    return a;
}

VAKA("W3-K1.acil_durdurmada_itki_notr") {
    SunucuKosusu k(bos_senaryo(0.6, 0.2, 0.4));
    KONTROL(k.kos(tam_itki()), "kos tamamlanır");
    ozet(k);
    int once = 0, sirada = 0, sonra = 0;
    for (const auto& s : k.sonuc.satirlar) {
        if (s.t_s > 0.1 && s.t_s < 0.195) {
            ++once;
            KONTROL(hepsi(s, kTamItki), "düzenek: acil durdurmadan önce tam itki fiziğe gider (t=" << s.t_s << ")");
        }
        if (s.estop != 0u) {
            ++sirada;
            KONTROL(hepsi(s, kNotr),
                    "W3-K1, iskelet §5: acil durdurma etkinken fiziğe giden değer nötr (t=" << s.t_s
                        << ", pwm_fiz[0]=" << s.pwm_fiz[0] << ")");
        }
        if (s.t_s > 0.43 && s.t_s < 0.6) {
            ++sonra;
            KONTROL(hepsi(s, kTamItki),
                    "acil durdurma kalkınca kalkıştan sonra uygulanan yeni komut fiziğe gider (t=" << s.t_s << ")");
        }
    }
    KONTROL(once > 10 && sirada > 30 && sonra > 10,
            "yeterli satır: önce " << once << ", acil " << sirada << ", sonra " << sonra);
    KONTROL_ESIT(k.sonuc.ihlal_estop, 0u, "Sonuc::ihlal_estop");
}

VAKA("W3-K1.acil_kalkinca_eski_komut_donmez") {
    // Kart t < 180 ms partilerine tam itki, sonra THR_VALID = 0 (IMU_REJECTED) verir: son geçerli
    // komut a_n = 185 ms'de uygulanır, valid_for 100 ms ile 285 ms'ye kadar geçerlidir. Acil
    // durdurma 200–230 ms. EMNİYET okuması: kalktıktan sonra yalnız kalkıştan sonra uygulanan
    // yeni bir komut itki verir; acil durdurmadan önceki komut kendiliğinden geri gelmez.
    SunucuKosusu k(bos_senaryo(0.4, 0.2, 0.23));
    const SunucuKosusu::Yanit y = [](SahteKart& kart, const GelenTf& tf, std::int64_t t) {
        if (tf.t < 180 * kMs) {
            kart.yanit_gonder(kart.yanit(tf, kTamItki, 0), t);
        } else {
            kart.yanit_gonder(kart.gecersiz_yanit(tf, 0), t);
        }
    };
    KONTROL(k.kos(y), "kos tamamlanır");
    ozet(k);
    int once = 0, sonra = 0;
    for (const auto& s : k.sonuc.satirlar) {
        if (s.t_s > 0.1 && s.t_s < 0.199) {
            ++once;
            KONTROL(hepsi(s, kTamItki), "düzenek: acil durdurmadan önce eski komut yürürlükte (t=" << s.t_s << ")");
        }
        if (s.estop != 0u) {
            KONTROL(hepsi(s, kNotr), "acil durdurma etkinken nötr (t=" << s.t_s << ")");
        }
        if (s.estop == 0u && s.t_s > 0.236 && s.t_s < 0.284) {
            ++sonra;
            KONTROL(hepsi(s, kNotr),
                    "EMNİYET okuması: acil durdurma kalktıktan sonra acil öncesi komut geri gelmez (t="
                        << s.t_s << ", pwm_fiz[0]=" << s.pwm_fiz[0] << ", pwm_yur[0]=" << s.pwm_yur[0]
                        << ")");
        }
    }
    KONTROL(once > 10 && sonra > 5, "yeterli satır: önce " << once << ", sonra " << sonra);
}

VAKA("A1-K56.sunucu_acil_darbe") {
    // Basma 301,4 ms, kalkma 304,1 ms: ikisi de (300, 305] penceresinde.
    SunucuKosusu k(bos_senaryo(0.45, 0.3014, 0.3041));
    KONTROL(k.kos(tam_itki()), "kos tamamlanır");
    ozet(k);
    const auto partiler = k.son_oturum();
    const Parti* p1 = nullptr;
    const Parti* p2 = nullptr;
    bool tek = true;
    for (std::size_t i = 0; i < partiler.size(); ++i) {
        tek = tek && partiler[i]->emniyet.size() <= 1u;
        if (p1 == nullptr && partiler[i]->tf.t >= 301'400'000) {
            p1 = partiler[i];
            p2 = (i + 1 < partiler.size()) ? partiler[i + 1] : nullptr;
        }
    }
    for (const Parti* p : partiler) {
        if (p->tf.t >= 290 * kMs && p->tf.t <= 330 * kMs) {
            std::printf("parti t=%lld emniyet=%zu", static_cast<long long>(p->tf.t), p->emniyet.size());
            for (const auto& s : p->emniyet) {
                std::printf(" [t_meas=%lld estop=%u rev=%u]", static_cast<long long>(s.t_meas_ns),
                            s.estop, s.safety_rev);
            }
            std::printf("\n");
        }
    }
    KONTROL(tek, "A1-K34: partide en çok bir IDA_SAFETY");
    KONTROL(p1 != nullptr && p2 != nullptr, "partiler alındı");
    if (p1 != nullptr && p2 != nullptr) {
        KONTROL(p1->tf.t == 305 * kMs, "düzenek: pencere (300, 305] ms; gözlenen " << p1->tf.t);
        KONTROL(p1->emniyet.size() == 1u && p1->emniyet.front().estop == 1u,
                "A1-K56: aynı pencerede basılıp kalkan acil durdurma o partide basılı bildirilir");
        KONTROL(p2->emniyet.size() == 1u && p2->emniyet.front().estop == 0u,
                "A1-K56: kalkma sonraki partide bildirilir");
    }
}

VAKA("W3-K1.rastgele_cerceveler_acil") {
    SunucuKosusu k(bos_senaryo(1.2, 0.05, -1.0));
    auto rc = std::make_shared<RastgeleCerceve>(0xACD0F00Du, false);
    auto sayi = std::make_shared<std::uint64_t>(0);
    const SunucuKosusu::Yanit y = [rc, sayi](SahteKart& kart, const GelenTf& tf, std::int64_t t) {
        kart.yanit_gonder(kart.yanit(tf, kTamItki, 0), t);
        if (tf.run_mode == w::kRunModePrep || tf.t < 60 * kMs) {
            return;  // hazırlık oturumu (A1-K19) bozulmaz; çerçeveler çalıştırma oturumunda
        }
        for (int i = 0; i < 500; ++i) {
            auto b = rc->uret(kart, tf);
            if (!b.empty()) {
                kart.ham_gonder(std::move(b), t);
                ++*sayi;
            }
        }
    };
    const bool tamam = k.kos(y);
    ozet(k);
    std::printf("rastgele çerçeve=%llu\n", static_cast<unsigned long long>(*sayi));
    KONTROL(tamam || !k.sonuc.satirlar.empty(), "kos çökmeden döner");
    KONTROL(*sayi >= 100'000u, "en az 100 000 çerçeve: " << *sayi);
    std::uint64_t ihlal = 0;
    int acil = 0;
    for (const auto& s : k.sonuc.satirlar) {
        if (s.t_s > 0.051) {
            KONTROL(s.estop != 0u, "acil durdurma etkin kalır (t=" << s.t_s << ")");
            ++acil;
            if (!hepsi(s, kNotr)) {
                ++ihlal;
            }
        }
    }
    KONTROL(acil > 100, "yeterli satır: " << acil);
    KONTROL_ESIT(ihlal, 0u, "W3-K1: acil durdurma etkinken nötr dışı fizik girdisi yok");
    KONTROL_ESIT(k.sonuc.ihlal_estop, 0u, "Sonuc::ihlal_estop");
}

}  // namespace
}  // namespace kabul

int main(int argc, char** argv) {
    return kabul::calistir(argc, argv);
}
