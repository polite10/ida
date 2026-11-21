// ida_sim — başsız sim sunucusu (W4-kopru). Kullanım: sim/sunucu/BENIOKU.md.
#include "ida/sunucu/sunucu.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void kullanim()
{
    std::printf(
        "kullanım: ida_sim [seçenekler]\n"
        "  --kip sil|hil            zaman yöneticisi kipi (varsayılan sil)\n"
        "  --karsi ic|tcp|pty|seri  karşı taraf (sil: ic, tcp; hil: pty, seri; varsayılan ic / seri)\n"
        "  --a1-port <port>         tcp: dinlenecek port (0: boş port)\n"
        "  --sil <yol>              tcp: başlatılacak ida_sil programı\n"
        "  --sil-depo <yol>         tcp: koşucunun kalıcı depo dosyası\n"
        "  --seri <aygıt>           hil: seri aygıt (ör. /dev/cu.usbmodem…, COM5)\n"
        "  --tohum <sayı>           ana tohum (varsayılan 1)\n"
        "  --sure <s>               sim süresi, saniye (varsayılan 90)\n"
        "  --senaryo z1|bos         yerleşik senaryo (varsayılan z1)\n"
        "  --ariza <k1,k2,…|yok>    z1 itici arızası kanalları (varsayılan 4,0)\n"
        "  --acil <s> [--acil-kalk <s>]  sanal acil durdurma\n"
        "  --csv <yol>              kontrol adımı başına CSV\n"
        "  --gunluk <0..2>          stderr günlüğü (varsayılan 1)\n");
}

}  // namespace

int main(int argc, char** argv)
{
    ida::sunucu::Ayar a;
    bool karsi_verildi = false;
    for (int i = 1; i < argc; ++i) {
        const std::string s = argv[i];
        auto deger = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "ida_sim: %s değer ister\n", s.c_str());
                std::exit(1);
            }
            return argv[++i];
        };
        if (s == "--kip") {
            const std::string v = deger();
            if (v == "sil") {
                a.kip = ida::sunucu::Kip::sil;
            } else if (v == "hil") {
                a.kip = ida::sunucu::Kip::hil;
            } else {
                std::fprintf(stderr, "ida_sim: bilinmeyen kip %s\n", v.c_str());
                return 1;
            }
        } else if (s == "--karsi") {
            const std::string v = deger();
            karsi_verildi = true;
            if (v == "ic") {
                a.karsi = ida::sunucu::Karsi::ic;
            } else if (v == "tcp") {
                a.karsi = ida::sunucu::Karsi::tcp;
            } else if (v == "pty") {
                a.karsi = ida::sunucu::Karsi::pty;
            } else if (v == "seri") {
                a.karsi = ida::sunucu::Karsi::seri;
            } else {
                std::fprintf(stderr, "ida_sim: bilinmeyen karşı taraf %s\n", v.c_str());
                return 1;
            }
        } else if (s == "--a1-port") {
            a.a1_port = static_cast<std::uint16_t>(std::strtoul(deger().c_str(), nullptr, 10));
        } else if (s == "--sil") {
            a.sil_yolu = deger();
        } else if (s == "--sil-depo") {
            a.sil_depo = deger();
        } else if (s == "--seri") {
            a.seri = deger();
            if (!karsi_verildi) {
                a.karsi = ida::sunucu::Karsi::seri;
            }
        } else if (s == "--tohum") {
            a.tohum = std::strtoull(deger().c_str(), nullptr, 0);
        } else if (s == "--sure") {
            a.sure_s = std::strtod(deger().c_str(), nullptr);
        } else if (s == "--senaryo") {
            a.senaryo = deger();
            if (a.senaryo != "z1" && a.senaryo != "bos") {
                std::fprintf(stderr, "ida_sim: bilinmeyen senaryo %s\n", a.senaryo.c_str());
                return 1;
            }
        } else if (s == "--ariza") {
            const std::string v = deger();
            a.z1.ariza_kanallari.clear();
            if (v != "yok") {
                std::size_t p = 0;
                while (p < v.size()) {
                    const std::size_t q = v.find(',', p);
                    const int k = std::atoi(v.substr(p, q == std::string::npos ? std::string::npos : q - p).c_str());
                    if (k < 0 || k > 7) {
                        std::fprintf(stderr, "ida_sim: kanal 0–7 olmalı\n");
                        return 1;
                    }
                    a.z1.ariza_kanallari.push_back(k);
                    p = q == std::string::npos ? v.size() : q + 1;
                }
            }
        } else if (s == "--acil") {
            a.acil_durdurma_s = std::strtod(deger().c_str(), nullptr);
        } else if (s == "--acil-kalk") {
            a.acil_kalkar_s = std::strtod(deger().c_str(), nullptr);
        } else if (s == "--csv") {
            a.csv_yolu = deger();
        } else if (s == "--gunluk") {
            a.gunluk = std::atoi(deger().c_str());
        } else if (s == "--yardim" || s == "-h") {
            kullanim();
            return 0;
        } else {
            std::fprintf(stderr, "ida_sim: bilinmeyen seçenek %s\n", s.c_str());
            kullanim();
            return 1;
        }
    }
    if (a.kip == ida::sunucu::Kip::hil && !karsi_verildi && a.seri.empty()) {
        std::fprintf(stderr, "ida_sim: hil kipi --seri <aygıt> ya da --karsi pty ister\n");
        return 1;
    }
    if (a.kip == ida::sunucu::Kip::hil && (a.karsi == ida::sunucu::Karsi::ic || a.karsi == ida::sunucu::Karsi::tcp)) {
        std::fprintf(stderr, "ida_sim: hil kipinde karşı taraf pty ya da seri olmalı\n");
        return 1;
    }
    if (a.kip == ida::sunucu::Kip::sil && (a.karsi == ida::sunucu::Karsi::pty || a.karsi == ida::sunucu::Karsi::seri)) {
        std::fprintf(stderr, "ida_sim: sil kipinde karşı taraf ic ya da tcp olmalı\n");
        return 1;
    }
    a.csv_uret = !a.csv_yolu.empty();
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const ida::kopru::Sayaclar& c = s.sayac;
    if (a.gunluk >= 1) {
        std::fprintf(stderr,
                     "ida_sim: %s; parti %llu, kaçırılan %llu, geç yanıt %llu, yinelenen %llu, alan/bağ reddi %llu/%llu, "
                     "oturum %llu, kopma %llu, hazırlık %llu, çalıştırma geçersiz 0x%x\n",
                     s.tamam ? "tamam" : ("HATA: " + s.hata).c_str(),
                     static_cast<unsigned long long>(c.parti_gonderilen),
                     static_cast<unsigned long long>(c.parti_kacirildi),
                     static_cast<unsigned long long>(c.yanit_ret_gec),
                     static_cast<unsigned long long>(c.yanit_ret_yinelenen),
                     static_cast<unsigned long long>(c.alan_ret), static_cast<unsigned long long>(c.bag_ret),
                     static_cast<unsigned long long>(c.oturum_kurulan), static_cast<unsigned long long>(c.oturum_kopma),
                     static_cast<unsigned long long>(s.hazirlik_oturumu), c.calistirma_gecersiz);
        if (a.senaryo == "z1") {
            std::fprintf(stderr, "ida_sim: z1 komutları: ARM %d (neden %d), SET_MODE %d, GUIDED_SP %d/%d, DISARM %d\n",
                         s.arm_ack, s.arm_neden, s.kip_ack, s.sp_ack, s.sp2_ack, s.disarm_ack);
        }
        if (a.kip == ida::sunucu::Kip::hil) {
            std::fprintf(stderr,
                         "ida_sim: hil: TF %llu, periyot en az/ort/en çok %.3f/%.3f/%.3f ms, TF geç yazım en çok %.3f ms, "
                         "sim gecikmesi %llu, son tarih %llu, gerçek zaman kaçırma %llu\n",
                         static_cast<unsigned long long>(s.zaman.tf_sayisi), 1e-6 * static_cast<double>(s.zaman.periyot_en_az_ns),
                         1e-6 * s.zaman.periyot_ort_ns, 1e-6 * static_cast<double>(s.zaman.periyot_en_cok_ns),
                         1e-6 * static_cast<double>(s.zaman.gec_yazim_en_cok_ns),
                         static_cast<unsigned long long>(s.zaman.sim_gecikmesi),
                         static_cast<unsigned long long>(c.son_tarih_kacirildi),
                         static_cast<unsigned long long>(s.zaman.gercek_zaman_kacirma));
            std::fprintf(stderr,
                         "ida_sim: hil: yanıt süresi (TF yazımı → yanıt alımı) %llu yanıt, p50/p99/en çok %.3f/%.3f/%.3f ms; "
                         "çalıştırma oturumunda karttan gelen: CRC %llu, atlanan bayt %llu, sıra boşluğu %llu "
                         "(toplam %llu/%llu/%llu); eşzamansız güvenli çıktı %llu (ilk parti öncesi, A1-K79 dışı: %llu)\n",
                         static_cast<unsigned long long>(s.zaman.yanit_sayisi), 1e-6 * static_cast<double>(s.zaman.yanit_p50_ns),
                         1e-6 * static_cast<double>(s.zaman.yanit_p99_ns), 1e-6 * static_cast<double>(s.zaman.yanit_en_cok_ns),
                         static_cast<unsigned long long>(s.zaman.rx_crc_kosu),
                         static_cast<unsigned long long>(s.zaman.rx_resync_kosu),
                         static_cast<unsigned long long>(s.zaman.rx_seq_gap_kosu),
                         static_cast<unsigned long long>(c.rx_crc_err), static_cast<unsigned long long>(c.rx_resync_bytes),
                         static_cast<unsigned long long>(c.rx_seq_gap),
                         static_cast<unsigned long long>(c.esz_guvenli_uygulanan),
                         static_cast<unsigned long long>(c.eszamansiz_ilk_parti_oncesi));
            if (s.kart.fc_var) {
                std::fprintf(stderr, "ida_sim: kart atış kontrol durumu (son IDA_FC_STATUS): state %u, inhibit_mask 0x%x\n",
                             static_cast<unsigned>(s.kart.fc_state), static_cast<unsigned>(s.kart.fc_inhibit_mask));
            }
        }
    }
    return s.tamam ? 0 : 2;
}
