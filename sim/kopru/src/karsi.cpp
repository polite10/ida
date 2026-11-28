// Süreç içi karşı taraf (karsi.hpp). Platform döngüsü sim/sil'deki koşucuyla aynı biçimdedir
// (çerçeve çerçeve rx, poll_tx, take / arka plan / step|tick / put_out, PARAM_SAVE kaydı).
#include "ida/kopru/karsi.hpp"

#include "ida/kopru/kopru.hpp"
#include "ida/port.h"
#include "ida_link/tel.h"
#include "ida_link/ust.h"
#include "mavlink_ic.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <random>
#include <thread>

namespace ida::kopru {

struct KarsiTaraf::Ic {
    std::unique_ptr<Tasima> tasima;
    KarsiAyari a;
    KarsiIstatistik ist;
    std::string hata;

    // Çekirdek (A4-K98): 64 KB, 64 B hizalı.
    struct Silici {
        void operator()(void* p) const { ::operator delete(p, std::align_val_t{64}); }
    };
    std::unique_ptr<void, Silici> bellek{::operator new(65536, std::align_val_t{64})};
    ida_core_t* cekirdek() { return static_cast<ida_core_t*>(bellek.get()); }
    std::unique_ptr<ida_link_ust_t> l = std::make_unique<ida_link_ust_t>();
    std::unique_ptr<ida_in_t> girdi = std::make_unique<ida_in_t>();
    std::unique_ptr<ida_out_t> cikti = std::make_unique<ida_out_t>();
    ida_link_platform_t plat{};
    ida_link_alici_t on_alici{};       // çerçeve sınırlarını bulmak için (aynı tel katmanı)

    // Platform durumu.
    std::uint32_t boot_count = 0;
    bool ilk_init = true;
    std::uint32_t nonce_deneme = 0;
    std::int64_t saat = 0;             // SIL: protokolün kabul ettiği son TF'nin zamanı
    std::int64_t son_now_w = 0;        // SIL: bu çekirdek ömründe verilen son now_w (A4-K09)
    std::chrono::steady_clock::time_point bas = std::chrono::steady_clock::now();
    std::vector<std::uint8_t> blok;
    std::uint32_t blok_gen = 0;
    std::uint8_t pstore = 0, pfail = 0;
    std::uint32_t pgen = 0;
    bool yeniden_baslat_istendi = false;
    std::vector<std::uint8_t> gidecek;
    std::vector<std::uint8_t> bekleyen;  // yarım çerçeve baytları (çerçeve sınırına kadar)

    std::int64_t simdi()
    {
        if (a.rol == KarsiRol::sil) {
            return saat;
        }
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - bas).count();
    }

    static ida_wtime_t p_zaman(void* ctx)
    {
        ida_wtime_t w{};
        w.ns = static_cast<Ic*>(ctx)->simdi();
        return w;
    }
    static bool p_gonder(void* ctx, ida_link_sinif_t, const std::uint8_t* c, std::size_t n)
    {
        auto* i = static_cast<Ic*>(ctx);
        i->gidecek.insert(i->gidecek.end(), c, c + n);
        return true;
    }
    static void p_temizle(void* ctx) { static_cast<Ic*>(ctx)->gidecek.clear(); }
    static std::uint32_t p_nonce(void* ctx)
    {
        auto* i = static_cast<Ic*>(ctx);
        std::uint32_t v = 0;
        if (i->a.rol == KarsiRol::sil) {
            const std::string etiket =
                "ida/a1/nonce_board/" + std::to_string(i->boot_count) + "/" + std::to_string(i->nonce_deneme);
            for (std::uint64_t n = 0; v == 0; ++n) {
                v = d7_u32(i->a.a1_tohum, etiket, n);
            }
        } else {
            static std::random_device rd;
            while (v == 0) {
                v = static_cast<std::uint32_t>(rd());
            }
        }
        ++i->nonce_deneme;
        return v;
    }
    static void p_yeniden(void* ctx) { static_cast<Ic*>(ctx)->yeniden_baslat_istendi = true; }
    static void p_blok(void* ctx, std::uint32_t* crc, std::uint32_t* len, std::uint32_t* gen)
    {
        const auto* i = static_cast<const Ic*>(ctx);
        if (i->blok.empty()) {
            *crc = 0;
            *len = 0;
            *gen = 0;
            return;
        }
        *crc = crc32_iso_hdlc(i->blok.data(), i->blok.size());
        *len = static_cast<std::uint32_t>(i->blok.size());
        *gen = i->blok_gen;
    }

    bool kur(std::uint8_t reset_cause)
    {
        ida_core_info_t bilgi{};
        ida_core_info(&bilgi);
        if (bilgi.core_size > 65536u || bilgi.core_align > 64u) {
            hata = "çekirdek 64 KB alana sığmıyor";
            return false;
        }
        std::memset(bellek.get(), 0, 65536);
        ++boot_count;
        if (a.rol == KarsiRol::sil) {
            // W4-K10 (ida_sil ile aynı): her çekirdek ömrü platform saatini 0'dan başlatır.
            saat = 0;
            son_now_w = 0;
        }
        ida_init_in_t ii{};
        ii.port_major = static_cast<std::uint16_t>(IDA_PORT_VERSION_MAJOR);
        ii.port_minor = static_cast<std::uint16_t>(IDA_PORT_VERSION_MINOR);
        ii.now_w.ns = simdi();
        ii.boot_count = boot_count;
        ii.boot_count_ok = true;
        ii.reset_cause = reset_cause;
        ii.param_src = static_cast<std::uint8_t>(blok.empty() ? IDA_PSRC_NONE : IDA_PSRC_SIL);
        ii.param_blob = blok.empty() ? nullptr : blok.data();
        ii.param_len = static_cast<std::uint32_t>(blok.size());
        const ida_rc_t rc = ida_core_init(cekirdek(), &ii);
        if (rc != IDA_RC_OK && rc != IDA_RC_PARAM_DEFAULTS) {
            hata = "ida_core_init: " + std::to_string(static_cast<int>(rc));
            return false;
        }
        ist.boot_count = boot_count;
        ist.init_rc = static_cast<std::uint8_t>(rc);
        ida_link_ust_ayar_t ua{};
        ua.rol = a.rol == KarsiRol::sil ? IDA_LINK_UST_ROL_SIL : IDA_LINK_UST_ROL_KART;
        ua.boot_count = boot_count;
        ua.boot_count_ok = true;
        ua.reset_cause = reset_cause;
        ua.param_src = ii.param_src;
        ua.init_rc = static_cast<std::uint8_t>(rc);
        ua.cekirdek = bilgi;
        ua.fw_version = 0x000100u;
        ua.t_bag_ns = a.t_bag_ns;
        ua.esc_alt_us = a.esc_alt_us;
        ua.esc_notr_us = a.esc_notr_us;
        ua.esc_ust_us = a.esc_ust_us;
        nonce_deneme = 0;
        pstore = static_cast<std::uint8_t>(IDA_PSTORE_IDLE);
        pfail = 0;
        // W4-K10 (a): SIL'de init sonrasında param_gen = 0 (ida_sil ile aynı).
        pgen = a.rol == KarsiRol::sil || blok.empty() ? 0u : blok_gen;
        yeniden_baslat_istendi = false;
        plat.ctx = this;
        plat.zaman = &Ic::p_zaman;
        plat.gonder = &Ic::p_gonder;
        plat.kuyruk_temizle = &Ic::p_temizle;
        plat.nonce = &Ic::p_nonce;
        plat.yeniden_baslat = &Ic::p_yeniden;
        plat.kalici_blok = &Ic::p_blok;
        if (ida_link_ust_kur(l.get(), &ua, &plat) != IDA_LINK_TAMAM) {
            hata = "ida_link_ust_kur";
            return false;
        }
        (void)ida_link_alici_kur(&on_alici, IDA_LINK_ROL_KART);
        ilk_init = false;
        return true;
    }

    void kayitlar(const ida_out_t& out)
    {
        for (unsigned k = 0; k < std::min<unsigned>(out.n_results, IDA_PORT_MAX_RESULTS); ++k) {
            const ida_cmd_result_t& r = out.results[k];
            if (r.kind != IDA_RES_PARAM_SAVE || r.ack != IDA_ACK_ACCEPTED || r.save.blob == nullptr) {
                continue;
            }
            blok.assign(r.save.blob, r.save.blob + r.save.len);
            blok_gen = r.save.gen;
            pstore = static_cast<std::uint8_t>(IDA_PSTORE_OK);
            pgen = r.save.gen;
            ++ist.kayit_ok;
        }
    }

    void cagrilar()
    {
        for (;;) {
            const ida_link_cagri_t tur = ida_link_ust_take(l.get(), girdi.get());
            if (tur == IDA_LINK_CAGRI_YOK) {
                return;
            }
            ida_in_t& in = *girdi;
            in.plat.param_store = pstore;
            in.plat.param_fail = pfail;
            in.plat.param_gen = pgen;
            in.plat.prev_overrun = false;
            in.plat.tx_stalled = false;
            in.plat.prev_call_ns = 0u;
            std::uint32_t dilim = 0;
            if (tur != IDA_LINK_CAGRI_OTURUM_SONU) {
                for (std::uint32_t k = 0; k < a.n_bg; ++k) {
                    const ida_bg_rc_t b = ida_core_background(cekirdek(), in.now_w);
                    if (b == IDA_BG_IDLE || b == IDA_BG_MORE) {
                        ++dilim;
                    }
                }
            }
            in.bg_slices = dilim;
            if (a.rol == KarsiRol::sil) {
                // A4-K09: saat bir çekirdek ömründe azalmaz; A4-K14: SIL'de alım damgası =
                // çağrının now_w'si (ida_sil'in alim_damgalari'yla aynı; sol #13).
                if (in.now_w.ns < son_now_w) {
                    in.now_w.ns = son_now_w;
                }
                son_now_w = in.now_w.ns;
                alim_damgalari(in);
            }
            if (a.cagri_kancasi) {
                a.cagri_kancasi(static_cast<int>(tur), in);
            }
            const bool step_gecerli = ida_link_ust_cagri_gecerli(l.get(), &in, tur);
            if (!step_gecerli) {
                in.imu_present = 0;   // W5-K70: step zaman çerçevesi ister; tick'e düşer
            }
            const ida_rc_t rc = (tur == IDA_LINK_CAGRI_STEP && step_gecerli) ? ida_core_step(cekirdek(), &in, cikti.get())
                                                           : ida_core_tick(cekirdek(), &in, cikti.get());
            if (a.cikti_kancasi) {
                a.cikti_kancasi(static_cast<int>(tur), in, *cikti);
            }
            ida_link_ust_put_out(l.get(), &in, rc, cikti.get());
            switch (tur) {
            case IDA_LINK_CAGRI_STEP: ++ist.cagri_step; break;
            case IDA_LINK_CAGRI_TICK_TF: ++ist.cagri_tick; break;
            case IDA_LINK_CAGRI_IZGARA: ++ist.cagri_izgara; break;
            default: ++ist.cagri_oturum_sonu; break;
            }
            // Reddedilen örneklerden VALID taşıyanlar (kabul kapısının zaman ve sıra koşulları;
            // A4-K33). VALID = 0 örnekler (ör. çözümsüz GNSS) tanım gereği reddedilir.
            if (cikti->status.cnt.sample_rejected != ist.sample_rejected) {
                const std::uint32_t artis = cikti->status.cnt.sample_rejected - ist.sample_rejected;
                std::uint32_t gecersiz = 0;
                for (unsigned q = 0; q < in.n_samples; ++q) {
                    gecersiz += (in.samples[q].hdr.flags & IDA_HF_VALID) == 0u ? 1u : 0u;
                }
                ist.gecerli_red += artis > gecersiz ? artis - gecersiz : 0u;
            }
            ist.sample_rejected = cikti->status.cnt.sample_rejected;
            ist.sample_future = cikti->status.cnt.sample_future;
            ist.imu_rejected = cikti->status.cnt.imu_rejected;
            kayitlar(*cikti);
        }
    }

    static void alim_damgalari(ida_in_t& in)
    {
        const ida_wtime_t w = in.now_w;
        if (in.tf.present) {
            in.tf.t_rx = w;
        }
        if (in.imu_present) {
            in.imu.hdr.t_rx = w;
        }
        const std::size_t n_ornek = std::min<std::size_t>(in.n_samples, IDA_PORT_MAX_SAMPLES);
        for (std::size_t k = 0; k < n_ornek; ++k) {
            in.samples[k].hdr.t_rx = w;
        }
        const std::size_t n_bas = std::min<std::size_t>(in.n_det_batch, IDA_PORT_MAX_DET_BATCH);
        for (std::size_t k = 0; k < n_bas; ++k) {
            in.det_batch_t_rx[k] = w;
        }
        const std::size_t n_kayit = std::min<std::size_t>(in.n_det, IDA_PORT_MAX_DET_IN);
        for (std::size_t k = 0; k < n_kayit; ++k) {
            in.det_t_rx[k] = w;
        }
        // Ping360 satırı: süreç içi karşı tarafta sonar yok (W4-sonar); işaretçi değişmez.
    }

    std::uint32_t kapanan_parti() const
    {
        std::uint32_t say[IDA_LINK_SAY_N] = {};
        ida_link_ust_sayaclar(l.get(), say);
        return say[IDA_LINK_SAY_BATCH_CLOSED];
    }

    void cerceve_sonrasi()
    {
        if (yeniden_baslat_istendi) {
            yeniden_baslat_istendi = false;
            ++ist.yeniden_kurulum;
            gidecek.clear();   // A1-K17
            saat = 0;
            if (!kur(static_cast<std::uint8_t>(IDA_RESET_SIL_REBOOT))) {
                return;
            }
        }
        ida_link_ust_poll_tx(l.get());
        cagrilar();
    }

    // Baytları çerçeve çerçeve modüle verir; TF'nin zamanı SIL saatine girer.
    void baytlar(const std::uint8_t* p, std::size_t n)
    {
        bekleyen.insert(bekleyen.end(), p, p + n);
        std::size_t bas_i = 0;
        std::size_t konum = 0;
        while (konum < bekleyen.size()) {
            std::size_t tuk = 0;
            ida_link_cerceve_t c;
            const ida_link_rc_t rc =
                ida_link_al(&on_alici, bekleyen.data() + konum, bekleyen.size() - konum, &tuk, &c);
            konum += tuk;
            if (rc != IDA_LINK_CERCEVE) {
                break;
            }
            // SIL (ida_sil ile aynı; sol #13): çerçeve alım anındaki saatle modüle verilir; saat
            // yalnız modül TF'yi kabul edip partiyi kapattıysa (batch_closed arttıysa) TF'nin
            // zamanına ilerler (DUP/OLD/eski oturum saati ilerletmez). Çağrı girdisindeki alım
            // damgaları çağrının now_w'sidir (alim_damgalari).
            const bool tf_mi = c.msgid == MAVLINK_MSG_ID_IDA_TIME_FRAME && a.rol == KarsiRol::sil;
            const std::uint32_t kapanan_once = tf_mi ? kapanan_parti() : 0u;
            ida_link_ust_rx(l.get(), bekleyen.data() + bas_i, konum - bas_i, ida_wtime_t{simdi()});
            bas_i = konum;
            if (tf_mi && kapanan_parti() != kapanan_once) {
                mavlink_ida_time_frame_t tf{};
                if (ida_link_coz(&c, c.msgid, &tf, sizeof tf) == IDA_LINK_TAMAM && tf.t_sim_ns >= 0 &&
                    tf.t_sim_ns <= static_cast<std::int64_t>(IDA_TIME_MAX_NS) && tf.t_sim_ns > saat) {
                    saat = tf.t_sim_ns;
                }
            }
            cerceve_sonrasi();
        }
        // Çerçeveye ait olmayan baytlar modüle de verilir (sayaçlar için); yarım çerçeve bekler.
        if (konum == bekleyen.size() && bas_i < konum) {
            // Kalan: tamamlanmamış aday olabilir; ön alıcı onu tutuyor, modüle de verilmeli.
            ida_link_ust_rx(l.get(), bekleyen.data() + bas_i, konum - bas_i, ida_wtime_t{simdi()});
            bas_i = konum;
        }
        bekleyen.erase(bekleyen.begin(), bekleyen.begin() + static_cast<std::ptrdiff_t>(bas_i));
    }

    bool yaz()
    {
        if (gidecek.empty() || !tasima) {
            gidecek.clear();
            return tasima && tasima->acik();
        }
        const bool ok = tasima->yaz(gidecek.data(), gidecek.size());
        gidecek.clear();
        return ok;
    }
};

KarsiTaraf::KarsiTaraf(std::unique_ptr<Tasima> tasima, const KarsiAyari& ayar) : ic_(std::make_unique<Ic>())
{
    ic_->tasima = std::move(tasima);
    ic_->a = ayar;
    ic_->blok = ayar.param_blok;
    ic_->blok_gen = ayar.param_gen;
    ic_->boot_count = ayar.onceki_acilis;
}

KarsiTaraf::~KarsiTaraf() = default;

bool KarsiTaraf::kur() { return ic_->kur(static_cast<std::uint8_t>(IDA_RESET_SIL_START)); }

bool KarsiTaraf::pompala()
{
    Ic& i = *ic_;
    if (!i.tasima) {
        return false;
    }
    std::uint8_t tampon[4096];
    for (;;) {
        std::size_t n = 0;
        const OkumaSonucu r = i.tasima->oku(tampon, sizeof tampon, 0, n);
        if (r == OkumaSonucu::tamam) {
            i.baytlar(tampon, n);
            (void)i.yaz();
            continue;
        }
        if (r == OkumaSonucu::kapandi) {
            ida_link_ust_akis_kapandi(i.l.get());
            i.cagrilar();
            i.gidecek.clear();
            return false;
        }
        break;
    }
    ida_link_ust_poll_tx(i.l.get());
    i.cagrilar();
    return i.yaz();
}

void KarsiTaraf::calistir(const std::function<bool()>& dur)
{
    Ic& i = *ic_;
    std::uint8_t tampon[4096];
    while (!dur()) {
        if (!i.tasima) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        std::size_t n = 0;
        const OkumaSonucu r = i.tasima->oku(tampon, sizeof tampon, 200, n);
        if (r == OkumaSonucu::tamam) {
            i.baytlar(tampon, n);
        } else if (r == OkumaSonucu::kapandi) {
            ida_link_ust_akis_kapandi(i.l.get());
            i.cagrilar();
            i.gidecek.clear();
            i.tasima.reset();
            continue;
        }
        ida_link_ust_poll_tx(i.l.get());
        i.cagrilar();
        (void)i.yaz();
    }
}

void KarsiTaraf::tasima_degistir(std::unique_ptr<Tasima> t)
{
    ida_link_ust_akis_kapandi(ic_->l.get());
    ic_->cagrilar();
    ic_->gidecek.clear();
    ic_->bekleyen.clear();
    (void)ida_link_alici_kur(&ic_->on_alici, IDA_LINK_ROL_KART);
    ic_->tasima = std::move(t);
}

const KarsiIstatistik& KarsiTaraf::istatistik() const { return ic_->ist; }
std::string KarsiTaraf::hata() const { return ic_->hata; }
std::uint16_t KarsiTaraf::sess() const { return ida_link_ust_sess(ic_->l.get()); }

}  // namespace ida::kopru
