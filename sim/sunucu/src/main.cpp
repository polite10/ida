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
        "  --senaryo z1|bos|<dosya.toml>  yerleşik senaryo (varsayılan z1) ya da A5 senaryo dosyası\n"
        "                           (depo köküne göre göreli; kip, süre ve tohum dosyadan)\n"
        "  --kok <dizin>            depo kökü (senaryo dosyası için; varsayılan .)\n"
        "  --ariza <k1,k2,…|yok>    z1 itici arızası kanalları (varsayılan 4,0)\n"
        "  --acil <s> [--acil-kalk <s>]  sanal acil durdurma\n"
        "  --csv <yol>              kontrol adımı başına CSV\n"
        "  --gunluk <0..2>          stderr günlüğü (varsayılan 1)\n"
        "çıkış kodu: 0 tamam, 1 komut satırı, 2 çalıştırma hatası, 3 geçersiz senaryo (doğrulama raporu\n"
        "standart hataya), 4 senaryo bu dilimde yürütülemez\n");
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
            a.kip_verildi = true;
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
            a.karsi_verildi = true;
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
            a.tohum_verildi = true;
        } else if (s == "--sure") {
            a.sure_s = std::strtod(deger().c_str(), nullptr);
            a.sure_verildi = true;
        } else if (s == "--kok") {
            a.depo_koku = deger();
        } else if (s == "--senaryo") {
            const std::string v = deger();
            if (v == "z1" || v == "bos") {
                a.senaryo = v;
            } else if (v.size() > 5 && v.compare(v.size() - 5, 5, ".toml") == 0) {
                a.senaryo_dosyasi = v;
            } else {
                std::fprintf(stderr, "ida_sim: bilinmeyen senaryo %s (z1, bos ya da .toml dosyası)\n", v.c_str());
                return 1;
            }
        } else if (s == "--ariza") {
            const std::string v = deger();
            a.ariza_verildi = true;
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
    if (!a.senaryo_dosyasi.empty() && !a.seri.empty()) {
        a.karsi_verildi = true;   // --seri karşı tarafı seri yapar
    }
    // Senaryo dosyasında kip dosyadandır: kip ve karşı taraf denetimi çalıştırıcıda yapılır.
    if (a.senaryo_dosyasi.empty() && a.kip == ida::sunucu::Kip::hil && !karsi_verildi && a.seri.empty()) {
        std::fprintf(stderr, "ida_sim: hil kipi --seri <aygıt> ya da --karsi pty ister\n");
        return 1;
    }
    if (a.senaryo_dosyasi.empty() && a.kip == ida::sunucu::Kip::hil &&
        (a.karsi == ida::sunucu::Karsi::ic || a.karsi == ida::sunucu::Karsi::tcp)) {
        std::fprintf(stderr, "ida_sim: hil kipinde karşı taraf pty ya da seri olmalı\n");
        return 1;
    }
    if (a.senaryo_dosyasi.empty() && a.kip == ida::sunucu::Kip::sil &&
        (a.karsi == ida::sunucu::Karsi::pty || a.karsi == ida::sunucu::Karsi::seri)) {
        std::fprintf(stderr, "ida_sim: sil kipinde karşı taraf ic ya da tcp olmalı\n");
        return 1;
    }
    a.csv_uret = !a.csv_yolu.empty();
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const ida::kopru::Sayaclar& c = s.sayac;
    using ida::sunucu::HataTuru;
    if (s.hata_turu == HataTuru::senaryo_gecersiz) {
        std::fprintf(stderr, "ida_sim: %s\n%s\n", s.hata.c_str(), s.dogrulama_raporu.c_str());
        return 3;
    }
    if (s.hata_turu == HataTuru::senaryo_yurutulemez || s.hata_turu == HataTuru::kullanim) {
        std::fprintf(stderr, "ida_sim: %s\n", s.hata.c_str());
        return s.hata_turu == HataTuru::kullanim ? 1 : 4;
    }
    if (a.gunluk >= 2) {
        std::fprintf(stderr, "ida_sim: başlangıç O_b derinliği %.17g m\n", s.denge_derinligi_m);
    }
    if (a.gunluk >= 1 && !a.senaryo_dosyasi.empty()) {
        std::fprintf(stderr, "ida_sim: senaryo %s, davranış özeti %s\n", a.senaryo_dosyasi.c_str(),
                     s.davranis_ozeti.c_str());
        for (const auto& [dosya, ozet] : s.kaynak_ozetleri) {
            std::fprintf(stderr, "ida_sim:   kaynak %s %s\n", dosya.c_str(), ozet.c_str());
        }
        for (const std::string& u : s.senaryo_uyarilari) {
            std::fprintf(stderr, "ida_sim:   uyarı: %s\n", u.c_str());
        }
        std::size_t uyg = 0, red = 0, yok = 0;
        for (const auto& o : s.olay_sonuclari) {
            uyg += o.sonuc == "uygulandi" ? 1u : 0u;
            red += o.sonuc == "reddedildi" ? 1u : 0u;
            yok += o.sonuc == "uygulanmadi" ? 1u : 0u;
        }
        std::fprintf(stderr,
                     "ida_sim:   olaylar: uygulandı %zu, reddedildi %zu, uygulanmadı %zu; atama çelişkisi %zu, "
                     "gevşetme reddi %zu; görev %s\n",
                     uyg, red, yok, s.atama_celiskileri.size(), s.gevsetme_retleri.size(), s.gorev_durumu.c_str());
    }
    if (a.gunluk >= 1) {
        std::fprintf(stderr,
                     "ida_sim: %s; parti %llu, kaçırılan %llu, geç yanıt %llu, yinelenen %llu, alan/bağ reddi %llu/%llu, "
                     "oturum %llu, kopma %llu, hazırlık %llu, çalıştırma geçersiz 0x%x; karttan seq: sıra dışı %llu, "
                     "kayıp %llu (çalıştırma oturumunda %llu/%llu)\n",
                     s.tamam ? "tamam" : ("HATA: " + s.hata).c_str(),
                     static_cast<unsigned long long>(c.parti_gonderilen),
                     static_cast<unsigned long long>(c.parti_kacirildi),
                     static_cast<unsigned long long>(c.yanit_ret_gec),
                     static_cast<unsigned long long>(c.yanit_ret_yinelenen),
                     static_cast<unsigned long long>(c.alan_ret), static_cast<unsigned long long>(c.bag_ret),
                     static_cast<unsigned long long>(c.oturum_kurulan), static_cast<unsigned long long>(c.oturum_kopma),
                     static_cast<unsigned long long>(s.hazirlik_oturumu), c.calistirma_gecersiz,
                     static_cast<unsigned long long>(c.rx_sira_disi), static_cast<unsigned long long>(c.rx_kayip),
                     static_cast<unsigned long long>(s.zaman.rx_sira_disi_kosu),
                     static_cast<unsigned long long>(s.zaman.rx_kayip_kosu));
        for (std::uint32_t j = 0; j < c.eszamansiz_kayit_n && j < c.eszamansiz_kayit.size(); ++j) {
            const auto& e = c.eszamansiz_kayit[j];
            std::fprintf(stderr,
                         "ida_sim: tanı: A1-K79'a sayılan eşzamansız güvenli çıktı: sınır %llu (%.4f s), out_seq %u, "
                         "act_flags 0x%x, önceki uygulanan out_seq %u, oturumda olağan yanıt alınmıştı %d\n",
                         static_cast<unsigned long long>(e.adim), 1.25e-3 * static_cast<double>(e.adim), e.out_seq,
                         static_cast<unsigned>(e.act_flags), e.onceki_out_seq, e.ilk_yanit_alindi ? 1 : 0);
        }
        if (a.senaryo == "z1" && a.senaryo_dosyasi.empty()) {
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
            if (!s.zaman.sira_boslugu_t_ns.empty()) {
                std::fprintf(stderr, "ida_sim: hil: oturum içi ilk sıra boşlukları (sim zamanı, s):");
                for (std::int64_t t : s.zaman.sira_boslugu_t_ns) {
                    std::fprintf(stderr, " %.4f", 1e-9 * static_cast<double>(t));
                }
                std::fprintf(stderr, "\n");
            }
            std::fprintf(stderr, "ida_sim: hil döngü %s", s.evre_ozeti.c_str());
            if (s.kart.fc_var) {
                std::fprintf(stderr, "ida_sim: kart atış kontrol durumu (son IDA_FC_STATUS): state %u, inhibit_mask 0x%x\n",
                             static_cast<unsigned>(s.kart.fc_state), static_cast<unsigned>(s.kart.fc_inhibit_mask));
            }
        }
    }
    return s.tamam ? 0 : 2;
}
