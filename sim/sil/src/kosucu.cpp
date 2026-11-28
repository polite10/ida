// kosucu.cpp — kartsız koşucu (ida/sil/kosucu.hpp).
//
// Döngü (bir bağlantı): soketten oku → baytları çerçeve sınırlarında böl (ön alıcı: ust'ün
// alıcısıyla aynı tel katmanı ve aynı bayt akışı) → her çerçeve için: TF ise saat = t_sim_ns →
// ida_link_ust_rx → poll_tx → take / N_bg arka plan / step|tick / put_out döngüsü → çıkanları
// yaz. Çekirdek çağrısı tek iş parçacığında ve okuma yapılmadan yürür: take ile put_out
// arasında rx çağrılmaz, bu sürede gelen baytlar işletim sisteminin soket tamponunda bekler
// (W3-K10).
#include "ida/sil/kosucu.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>

#include "depo.hpp"
#include "ida_link/tel.h"
#include "ida_link/ust.h"
#include "mavlink_ic.hpp"
#include "tasima.hpp"
#include "turetme.hpp"

#ifndef IDA_SIL_GIT
#define IDA_SIL_GIT 0u
#endif

namespace ida::sil {

namespace {

using Saat = std::chrono::steady_clock;

std::uint64_t gecen_ns(Saat::time_point bas)
{
    const auto d = std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - bas).count();
    return d > 0 ? static_cast<std::uint64_t>(d) : 0u;
}

ida_wtime_t duvar(std::int64_t ns)
{
    ida_wtime_t w;
    w.ns = ns;
    return w;
}

// Çekirdek örneğinin alanı (A4-K98). Her koşucu örneğinin kendi alanı vardır (A1-K101: bir
// süreçte birden çok örnek); boy ve hizalama ida_core_info ile denetlenir.
struct alignas(kCekirdekHizasi) CekirdekAlani {
    unsigned char b[kCekirdekAlani];
};

constexpr std::size_t kAlimTamponu = 64u * 1024u;
constexpr int kOkumaBeklemesiMs = 100;

}  // namespace

// ---- Süre istatistiği ----------------------------------------------------------------------

void SureIstatistigi::ekle(std::uint64_t ns)
{
    if (kova.empty()) {
        kova.assign(kKova, 0u);
    }
    en_az_ns = (sayi == 0u) ? ns : std::min(en_az_ns, ns);
    en_cok_ns = std::max(en_cok_ns, ns);
    ++sayi;
    toplam_ns += ns;
    const std::uint64_t us = ns / 1000u;
    const std::size_t k = us >= kKova - 1u ? kKova - 1u : static_cast<std::size_t>(us);
    ++kova[k];
}

double SureIstatistigi::yuzdelik_us(double p) const
{
    if (sayi == 0u || kova.empty()) {
        return 0.0;
    }
    const double hedef = static_cast<double>(sayi) * p / 100.0;
    std::uint64_t birikim = 0;
    for (std::size_t k = 0; k < kova.size(); ++k) {
        birikim += kova[k];
        if (static_cast<double>(birikim) >= hedef) {
            return static_cast<double>(k);
        }
    }
    return static_cast<double>(kova.size() - 1u);
}

const char* cikis_aciklamasi(Cikis c)
{
    switch (c) {
    case Cikis::Tamam:
        return "olağan bitiş";
    case Cikis::Kullanim:
        return "komut satırı hatası";
    case Cikis::Cekirdek:
        return "çekirdek kurulamadı";
    case Cikis::GuvenliBekleme:
        return "güvenli bekleme (çekirdek iç tutarlılığı)";
    case Cikis::Baglanamadi:
        return "bağlantı kurulamadı";
    }
    return "bilinmeyen";
}

// ---- İç durum ------------------------------------------------------------------------------

struct Kosucu::Ic {
    Ayarlar ayar;
    Kancalar kanca;
    std::atomic<bool> dur{false};

    std::unique_ptr<CekirdekAlani> alan;
    ida_core_t* cekirdek = nullptr;
    ida_core_info_t bilgi{};
    std::unique_ptr<ida_link_ust_t> l;
    std::unique_ptr<ida_in_t> girdi;
    std::unique_ptr<ida_out_t> cikti;
    ida_link_platform_t plat{};
    ida_link_alici_t on_alici{};   // çerçeve sınırları ve TF zamanı (yalnız gözlem)
    std::unique_ptr<ida_ping360_t> ping_kopya;   // A4-K14 damgalı Ping360 satırı

    Depo depo;
    bool ilk_init = true;
    std::int64_t saat = 0;          // platform saati: sim zamanı, ns (A4-K11); ömür başı 0
    std::int64_t son_now_w = 0;     // bu çekirdek ömründe verilen son now_w (A4-K09)
    std::uint32_t boot_count = 0;
    bool boot_ok = false;
    std::uint32_t nonce_deneme = 0; // A1-K114 <deneme>; her açılışta 0
    std::uint32_t bg_dilim = 0;     // A4-K109
    std::uint8_t pstore = static_cast<std::uint8_t>(IDA_PSTORE_IDLE);
    std::uint8_t pfail = static_cast<std::uint8_t>(IDA_PSFAIL_NONE);
    std::uint32_t pgen = 0;         // A4 3.6: SIL'de init'te 0
    bool yeniden_baslat_istendi = false;
    std::uint32_t izlenen_oturum = 0;
    bool ilk_tf_bekleniyor = true;

    tasima::Soket soket;
    std::vector<std::uint8_t> gidecek;
    std::vector<std::uint8_t> alim;
    bool kopuk = true;
    bool geri_donus = false;
    bool guvenli = false;
    bool cekirdek_hatasi = false;

    Istatistik ist;

    Ic(Ayarlar a, Kancalar k)
        : ayar(std::move(a)),
          kanca(std::move(k)),
          alan(std::make_unique<CekirdekAlani>()),
          l(std::make_unique<ida_link_ust_t>()),
          girdi(std::make_unique<ida_in_t>()),
          cikti(std::make_unique<ida_out_t>()),
          ping_kopya(std::make_unique<ida_ping360_t>()),
          depo(ayar.depo_yolu, ayar.onceki_acilis),
          alim(kAlimTamponu)
    {
        cekirdek = reinterpret_cast<ida_core_t*>(alan->b);
        plat.ctx = this;
        plat.zaman = &Ic::p_zaman;
        plat.gonder = &Ic::p_gonder;
        plat.kuyruk_temizle = &Ic::p_temizle;
        plat.nonce = &Ic::p_nonce;
        plat.yeniden_baslat = &Ic::p_yeniden_baslat;
        plat.kalici_blok = &Ic::p_kalici_blok;
    }

    void gunluk(int duzey, const std::string& s) const
    {
        if (duzey <= ayar.gunluk) {
            std::fprintf(stderr, "ida_sil: %s\n", s.c_str());
        }
    }

    // ---- Platform işlevleri (ida_link_platform_t) ----

    static ida_wtime_t p_zaman(void* ctx)
    {
        return duvar(static_cast<Ic*>(ctx)->saat);
    }

    // Çerçeve çıkış tamponuna eklenir; çerçeve işlemesi bitince sokete yazılır. Bağlantı
    // yoksa ya da koptuysa false (ZORUNLU sınıfta modül oturumu bitirir; A1-K110).
    static bool p_gonder(void* ctx, ida_link_sinif_t sinif, const std::uint8_t* cerceve,
                         std::size_t n)
    {
        (void)sinif;
        Ic* ic = static_cast<Ic*>(ctx);
        if (ic->kopuk) {
            ic->l->say[IDA_LINK_SAY_TX_DROPPED] += 1u;
            return false;
        }
        ic->gidecek.insert(ic->gidecek.end(), cerceve, cerceve + n);
        ic->l->say[IDA_LINK_SAY_TX_FRAMES] += 1u;
        return true;
    }

    // A1-K22 (1): yazımı başlamamış çerçeveler silinir (tampon yalnız bu çerçevenin
    // işlenmesinde üretilenleri tutar; önceki çerçevelerinki yazılmıştır).
    static void p_temizle(void* ctx)
    {
        static_cast<Ic*>(ctx)->gidecek.clear();
    }

    // A1-K114, A5-K118: ida/a1/nonce_board/<boot_count>/<deneme>, tohum --a1-seed.
    static std::uint32_t p_nonce(void* ctx)
    {
        Ic* ic = static_cast<Ic*>(ctx);
        const std::string etiket = turetme::nonce_board_etiketi(ic->boot_count, ic->nonce_deneme);
        ic->nonce_deneme += 1u;
        return turetme::u32_sifirsiz(ic->ayar.a1_tohum, etiket);
    }

    // A1-K17: SIL'de çekirdek yeni boot_count ve SIL_REBOOT ile yeniden kurulur. Modülün
    // içinden (rx sırasında) çağrılır; yeniden kurulum rx döndükten sonra yapılır.
    static void p_yeniden_baslat(void* ctx)
    {
        static_cast<Ic*>(ctx)->yeniden_baslat_istendi = true;
    }

    static void p_kalici_blok(void* ctx, std::uint32_t* crc, std::uint32_t* len,
                              std::uint32_t* gen)
    {
        static_cast<const Ic*>(ctx)->depo.kalici_blok(crc, len, gen);
    }

    // ---- Kurulum ----

    // Çekirdeği init eder ve modülü kurar (açılış, REBOOT, sim zamanının geri başlaması).
    bool kur(std::uint8_t reset_cause)
    {
        ida_core_info(&bilgi);
        ist.core_size = bilgi.core_size;
        if (bilgi.core_size == 0u || bilgi.core_size > kCekirdekAlani || bilgi.core_align == 0u ||
            kCekirdekHizasi % bilgi.core_align != 0u ||
            static_cast<unsigned>(bilgi.port_major) != IDA_PORT_VERSION_MAJOR ||
            static_cast<unsigned>(bilgi.port_minor) != IDA_PORT_VERSION_MINOR) {
            gunluk(1, "çekirdek alana sığmıyor ya da port sürümü uyuşmuyor: boy " +
                          std::to_string(bilgi.core_size) + " B, hiza " +
                          std::to_string(bilgi.core_align) + ", alan " +
                          std::to_string(kCekirdekAlani) + " B");
            return false;
        }
        // W4-K10: her çekirdek ömrü (açılış, REBOOT, sim zamanının geri başlaması) platform
        // saatini 0'dan başlatır (kart yeniden açılışının karşılığı); init'in now_w'si ilk TF'yi
        // aşmaz (A4-K11) ve TF'siz bir oturum sonu çağrısında saat geri gitmez (A4-K09).
        saat = 0;
        son_now_w = 0;
        const Depo::Acilis a = depo.acilis_ayir();
        boot_count = a.ok ? a.boot_count : 0u;
        boot_ok = a.ok;
        if (!a.ok) {
            gunluk(1, "açılış sayacı ayrılamadı: boot_count_ok = 0 (olay kimliği kapalı)");
        }

        ida_init_in_t ii{};
        ii.port_major = static_cast<std::uint16_t>(IDA_PORT_VERSION_MAJOR);
        ii.port_minor = static_cast<std::uint16_t>(IDA_PORT_VERSION_MINOR);
        ii.now_w = duvar(saat);   // A4-K11: ilk TF'nin sim zamanını aşmaz
        ii.boot_count = boot_count;
        ii.boot_count_ok = boot_ok;
        ii.reset_cause = reset_cause;
        ii.reset_flags = 0u;
        ii.consecutive_faults = 0u;
        const auto& b = depo.blok();
        if (b.has_value()) {
            ii.param_src = static_cast<std::uint8_t>(IDA_PSRC_FLASH);
            ii.param_blob = b->veri.data();
            ii.param_len = static_cast<std::uint32_t>(b->veri.size());
        } else {
            ii.param_src = static_cast<std::uint8_t>(IDA_PSRC_NONE);
            ii.param_blob = nullptr;
            ii.param_len = 0u;
        }
        const ida_rc_t rc = ida_core_init(cekirdek, &ii);
        ist.init_rc = static_cast<std::uint8_t>(rc);
        ist.boot_count = boot_count;
        ist.boot_count_ok = boot_ok;
        if (rc != IDA_RC_OK && rc != IDA_RC_PARAM_DEFAULTS) {
            gunluk(1, "ida_core_init başarısız: rc " + std::to_string(static_cast<int>(rc)));
            return false;
        }

        ida_link_ust_ayar_t ua{};
        ua.rol = IDA_LINK_UST_ROL_SIL;
        ua.boot_count = boot_count;
        ua.boot_count_ok = boot_ok;
        ua.reset_cause = reset_cause;
        ua.reset_flags = 0u;
        ua.consecutive_faults = 0u;
        ua.param_src = ii.param_src;
        ua.init_rc = static_cast<std::uint8_t>(rc);
        ua.cekirdek = bilgi;
        ua.fw_version = kSurumSayisi;
        ua.fw_git = static_cast<std::uint32_t>(IDA_SIL_GIT);
        if (ilk_init) {
            ua.prev_sess = ayar.prev_sess;            // A1-K114: sınama girdisi
            ua.sessions_since_boot = ayar.oturum_sayisi;
            ua.last_cmd_id = ayar.last_cmd_id;
        }
        ua.t_bag_ns = IDA_LINK_UST_T_BAG_MIN_NS;     // SIL'de kullanılmaz (A1-K100)
        ua.esc_alt_us = ayar.esc_alt_us;
        ua.esc_notr_us = ayar.esc_notr_us;
        ua.esc_ust_us = ayar.esc_ust_us;
        ua.hata_kaydi_var = false;

        nonce_deneme = 0u;
        bg_dilim = 0u;
        pstore = static_cast<std::uint8_t>(IDA_PSTORE_IDLE);
        pfail = static_cast<std::uint8_t>(IDA_PSFAIL_NONE);
        // A4 3.6, W4-K10 (a): SIL'de init sonrasında param_gen = 0; saklı bloğun kuşağı yalnız
        // kalici_blok bilgisinde (BOARD_HELLO, STATUS) taşınır.
        pgen = 0u;
        yeniden_baslat_istendi = false;
        izlenen_oturum = 0u;
        ilk_tf_bekleniyor = true;
        if (ida_link_ust_kur(l.get(), &ua, &plat) != IDA_LINK_TAMAM) {
            gunluk(1, "ida_link_ust_kur başarısız (ESC değerleri alt < nötr < üst olmalı)");
            return false;
        }
        ilk_init = false;
        gunluk(2, "çekirdek kuruldu: boot_count " + std::to_string(boot_count) +
                      (boot_ok ? "" : " (geçersiz)") + ", init_rc " +
                      std::to_string(static_cast<int>(rc)) + ", reset_cause " +
                      std::to_string(static_cast<int>(reset_cause)) + ", çekirdek " +
                      std::to_string(bilgi.core_size) + " B");
        return true;
    }

    // ---- Çağrılar ----

    // PARAM_SAVE kabul edildiyse blok kalıcı depoya yazılır; sonuç sonraki çağrının
    // plat.param_store / param_fail / param_gen alanlarındadır (A4-K83; BUSY görünmeden).
    void kayitlari_isle(const ida_out_t& out)
    {
        const unsigned n = std::min<unsigned>(out.n_results, IDA_PORT_MAX_RESULTS);
        for (unsigned i = 0; i < n; ++i) {
            const ida_cmd_result_t& r = out.results[i];
            if (static_cast<unsigned>(r.kind) != IDA_RES_PARAM_SAVE ||
                static_cast<unsigned>(r.ack) != IDA_ACK_ACCEPTED) {
                continue;
            }
            if (ida_link_ust_durum(l.get()) != IDA_LINK_UST_RUNNING) {
                pstore = static_cast<std::uint8_t>(IDA_PSTORE_FAIL);   // oturum bitti (A4-K83)
                pfail = static_cast<std::uint8_t>(IDA_PSFAIL_ABORTED);
            } else {
                const Depo::KayitSonucu s = depo.kaydet(r.save.blob, r.save.len, r.save.gen);
                pstore = s.store;
                pfail = s.fail;
            }
            pgen = r.save.gen;
            if (static_cast<unsigned>(pstore) == IDA_PSTORE_OK) {
                ++ist.kayit_ok;
            } else {
                ++ist.kayit_fail;
            }
            gunluk(2, "parametre kaydı: gen " + std::to_string(pgen) + ", param_store " +
                          std::to_string(static_cast<int>(pstore)) + ", param_fail " +
                          std::to_string(static_cast<int>(pfail)));
        }
    }

    // A4-K14: SIL'de zaman çerçevesinin, örneklerin, algılama kayıtlarının ve parti
    // başlıklarının alım damgası çağrının now_w'sidir. Modül damgaları alım anındaki platform
    // saatiyle koyar (üyeler partiyi kapatan TF'den önce gelir); çağrı girdisi burada
    // tamamlanır. Ping360 satırı modülün belleğindedir (const): kopyası damgalanır.
    void alim_damgalari(ida_in_t& in)
    {
        const ida_wtime_t w = in.now_w;
        if (in.tf.present) {
            in.tf.t_rx = w;
        }
        if (in.imu_present) {
            in.imu.hdr.t_rx = w;
        }
        const std::size_t n_ornek = std::min<std::size_t>(in.n_samples, IDA_PORT_MAX_SAMPLES);
        for (std::size_t i = 0; i < n_ornek; ++i) {
            in.samples[i].hdr.t_rx = w;
        }
        const std::size_t n_bas = std::min<std::size_t>(in.n_det_batch, IDA_PORT_MAX_DET_BATCH);
        for (std::size_t i = 0; i < n_bas; ++i) {
            in.det_batch_t_rx[i] = w;
        }
        const std::size_t n_kayit = std::min<std::size_t>(in.n_det, IDA_PORT_MAX_DET_IN);
        for (std::size_t i = 0; i < n_kayit; ++i) {
            in.det_t_rx[i] = w;
        }
        if (in.ping360 != nullptr) {
            *ping_kopya = *in.ping360;
            ping_kopya->hdr.t_rx = w;
            in.ping360 = ping_kopya.get();
        }
    }

    void cagrilari_yap()
    {
        while (!guvenli) {
            const ida_link_cagri_t tur = ida_link_ust_take(l.get(), girdi.get());
            if (tur == IDA_LINK_CAGRI_YOK) {
                return;
            }
            // --- çağrı penceresi açık (W3-K10): rx çağrılmaz ---
            const auto bas = Saat::now();
            ida_in_t& in = *girdi;
            in.plat.param_store = pstore;
            in.plat.param_fail = pfail;
            in.plat.param_gen = pgen;
            in.plat.prev_overrun = false;   // SIL: süre çekirdeğe verilmez (A1-K102)
            in.plat.tx_stalled = false;
            in.plat.prev_call_ns = 0u;      // A4 3.6: 0 = ölçülmedi
            // A4-K09: platform saati bir çekirdek ömründe azalmaz. Olağan akışta now_w zaten
            // azalmaz (TF zamanı ya da son TF'nin zamanı); bu yalnız korumadır.
            if (in.now_w.ns < son_now_w) {
                in.now_w.ns = son_now_w;
            }
            son_now_w = in.now_w.ns;
            alim_damgalari(in);             // A4-K14: SIL'de alım damgası = çağrının now_w'si
            if (tur != IDA_LINK_CAGRI_OTURUM_SONU) {
                for (std::uint32_t i = 0; i < ayar.n_bg; ++i) {   // A1-K121 (iii), A4-K95
                    const ida_bg_rc_t b = ida_core_background(cekirdek, in.now_w);
                    ++ist.arka_plan;
                    if (b == IDA_BG_IDLE || b == IDA_BG_MORE) {
                        ++bg_dilim;
                    }
                }
            }
            in.bg_slices = bg_dilim;       // A4-K109
            if (kanca.cagri_icinde) {
                kanca.cagri_icinde(static_cast<int>(tur), in);
            }
            const bool step_gecerli = ida_link_ust_cagri_gecerli(l.get(), &in, tur);
            if (!step_gecerli) {
                in.imu_present = 0;   // W5-K70: step zaman çerçevesi ister; tick'e düşer
            }
            const auto cbas = Saat::now();
            const ida_rc_t rc = (tur == IDA_LINK_CAGRI_STEP && step_gecerli)
                                    ? ida_core_step(cekirdek, &in, cikti.get())
                                    : ida_core_tick(cekirdek, &in, cikti.get());
            ist.cekirdek.ekle(gecen_ns(cbas));
            bg_dilim = 0u;
            if (kanca.cagri_sonrasi) {
                kanca.cagri_sonrasi(static_cast<int>(tur), in, *cikti);
            }
            ida_link_ust_put_out(l.get(), &in, rc, cikti.get());
            // --- çağrı penceresi kapandı ---
            ist.tur.ekle(gecen_ns(bas));
            const auto tur_i = static_cast<std::size_t>(tur);
            if (tur_i < sizeof ist.cagri / sizeof ist.cagri[0]) {
                ist.cagri[tur_i] += 1u;
            }
            kayitlari_isle(*cikti);
            if (rc != IDA_RC_NOT_INIT && !cikti->health.core_ok) {
                // A4-K89: iç tutarlılık kaybı mandallıdır. Kartta gözetim beslenmez; burada
                // platform güvenli beklemeye girer: oturum SAFE_STATE ile biter, çekirdek bir
                // daha çağrılmaz, koşucu çıkar (kısıtlayan yön).
                guvenli = true;
                gunluk(1, "health.core_ok = 0: güvenli bekleme");
                ida_link_ust_guvenli_bekleme(l.get());
            }
        }
    }

    // Çıkış tamponunu sokete yazar.
    void bosalt()
    {
        if (gidecek.empty()) {
            return;
        }
        // Karşı taraf okumazken yazım bekler; durdur() bu beklemeyi keser (bağlantı bırakılır).
        if (!kopuk && !soket.yaz(gidecek.data(), gidecek.size(), &dur)) {
            kopuk = true;
            gunluk(2, dur.load() ? "yazma durduruldu" : "yazma başarısız: bağlantı koptu");
        }
        gidecek.clear();
    }

    // Bir çerçeve ust'e verildikten sonra: REBOOT, zamanlayıcılar, çağrılar, gönderim.
    void cerceve_sonrasi()
    {
        if (yeniden_baslat_istendi) {
            yeniden_baslat_istendi = false;
            ++ist.yeniden_kurulum;
            gunluk(2, "REBOOT: çekirdek yeniden kuruluyor");
            gidecek.clear();   // A1-K17: başka çerçeve gönderilmez
            if (!kur(static_cast<std::uint8_t>(IDA_RESET_SIL_REBOOT))) {
                cekirdek_hatasi = true;
                return;
            }
        }
        ida_link_ust_poll_tx(l.get());
        cagrilari_yap();
        bosalt();
    }

    void rx_ver(const std::uint8_t* p, std::size_t n)
    {
        if (n == 0u) {
            return;
        }
        if (ida_link_ust_cagri_acik(l.get())) {
            ++ist.pencere_ihlali;   // yapı gereği olmaz (W3-K10); sınamada 0 beklenir
        }
        ida_link_ust_rx(l.get(), p, n, duvar(saat));
    }

    std::uint32_t kapanan_parti() const
    {
        std::uint32_t say[IDA_LINK_SAY_N] = {};
        ida_link_ust_sayaclar(l.get(), say);
        return say[IDA_LINK_SAY_BATCH_CLOSED];
    }

    // TF modüle verildikten sonra: modül partiyi kapattıysa (TF'yi kabul ettiyse) sim zamanı
    // platform saatine girer (A4-K11, A1-K121). Kabul kuralı (oturum, sess, DUP/OLD; A1-K25,
    // K31) yalnız modüldedir: kabul, modülün batch_closed sayacının artmasından okunur;
    // reddedilen TF saati ve zamanlayıcıları ilerletmez. Oturumun kabul edilen ilk TF'si
    // saatten küçükse sim zamanı geri başlamıştır: *geri = true.
    void tf_sonrasi(const ida_link_cerceve_t& c, std::uint32_t kapanan_once, bool* geri)
    {
        std::uint32_t say[IDA_LINK_SAY_N] = {};
        ida_link_ust_sayaclar(l.get(), say);
        if (say[IDA_LINK_SAY_SESSIONS] != izlenen_oturum) {
            izlenen_oturum = say[IDA_LINK_SAY_SESSIONS];
            ilk_tf_bekleniyor = true;
        }
        if (say[IDA_LINK_SAY_BATCH_CLOSED] == kapanan_once) {
            return;   // DUP, OLD, oturum dışı ya da eski oturum: modül attı
        }
        mavlink_ida_time_frame_t m{};
        if (ida_link_coz(&c, MAVLINK_MSG_ID_IDA_TIME_FRAME, &m, sizeof m) != IDA_LINK_TAMAM) {
            return;
        }
        const bool gecerli = m.t_sim_ns >= 0 && m.t_sim_ns <= IDA_TIME_MAX_NS;
        if (ilk_tf_bekleniyor) {
            ilk_tf_bekleniyor = false;
            if (gecerli && m.t_sim_ns < saat) {
                *geri = true;
                return;
            }
        }
        if (gecerli && m.t_sim_ns > saat) {
            saat = m.t_sim_ns;
        }
    }

    // Alınan baytları çerçeve çerçeve işler. false: bağlantı bırakılmalı.
    bool veri_isle(const std::uint8_t* p, std::size_t n)
    {
        std::size_t i = 0;
        while (i < n) {
            std::size_t t = 0;
            ida_link_cerceve_t c{};
            const ida_link_rc_t rc = ida_link_al(&on_alici, p + i, n - i, &t, &c);
            if (rc != IDA_LINK_CERCEVE || t == 0u || t > n - i) {
                rx_ver(p + i, n - i);   // çerçeve tamamlanmadı: yarım aday modülde bekler
                return true;
            }
            const bool tf = c.msgid == MAVLINK_MSG_ID_IDA_TIME_FRAME;
            const std::uint32_t kapanan_once = tf ? kapanan_parti() : 0u;
            rx_ver(p + i, t);
            i += t;
            if (tf) {
                bool geri = false;
                tf_sonrasi(c, kapanan_once, &geri);
                if (geri) {
                    // Bekleyen parti çekirdeğe verilmez: bağlantı kapanınca modül oturumu
                    // bitirir ve partiyi siler (A1-K22 (1)); çekirdek yeniden kurulur.
                    geri_donus = true;
                    return false;
                }
            }
            cerceve_sonrasi();
            if (kopuk || guvenli || cekirdek_hatasi || dur.load()) {
                return false;
            }
        }
        return true;
    }

    void baglanti_dongusu()
    {
        (void)ida_link_alici_kur(&on_alici, IDA_LINK_ROL_KART);
        kopuk = false;
        geri_donus = false;
        while (!dur.load() && !kopuk) {
            const long long n = soket.oku(alim.data(), alim.size(), kOkumaBeklemesiMs);
            if (n < 0) {
                kopuk = true;
                break;
            }
            if (n == 0) {
                continue;
            }
            if (!veri_isle(alim.data(), static_cast<std::size_t>(n))) {
                break;
            }
        }
        if (!guvenli && !cekirdek_hatasi) {
            // A1-K99, K22, K107: akışın kapanması oturumun bitişidir; oturum sonu çağrısı.
            ida_link_ust_akis_kapandi(l.get());
            cagrilari_yap();
        }
        bosalt();   // oturum sonu izi ya da SESSION_CLOSE(SAFE_STATE), en iyi çabayla
        soket.kapat();
        kopuk = true;
        gidecek.clear();
    }

    void uyu(std::uint32_t ms) const
    {
        const auto son = Saat::now() + std::chrono::milliseconds(ms);
        while (!dur.load() && Saat::now() < son) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    Cikis calistir()
    {
        switch (depo.yukle()) {
        case Depo::Yukleme::DosyaYok:
            gunluk(2, depo.dosyali() ? "depo dosyası yok: yeni depo" : "bellek içi depo");
            break;
        case Depo::Yukleme::Tamam:
            gunluk(2, "depo okundu");
            break;
        case Depo::Yukleme::BlokBozuk:
            gunluk(1, "depodaki parametre bloğu bozuk: varsayılanlar kullanılacak");
            break;
        case Depo::Yukleme::SayacBozuk:
            gunluk(1, "depodaki açılış sayacı bozuk: boot_count_ok = 0");
            break;
        case Depo::Yukleme::DosyaBozuk:
            gunluk(1, "depo dosyası tanınmadı: varsayılanlar, boot_count_ok = 0");
            break;
        }
        if (!kur(static_cast<std::uint8_t>(IDA_RESET_SIL_START))) {
            return Cikis::Cekirdek;
        }
        if (!tasima::ag_baslat()) {
            gunluk(1, "ağ kitaplığı başlatılamadı");
            return Cikis::Baglanamadi;
        }
        bool hic_baglandi = false;
        auto bekleme_basi = Saat::now();
        while (!dur.load()) {
            std::string hata;
            soket = tasima::Soket::baglan(ayar.a1_port, &hata);
            if (!soket.gecerli()) {
                const auto gecen = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       Saat::now() - bekleme_basi)
                                       .count();
                if (gecen >= static_cast<long long>(ayar.bekleme_ms)) {
                    gunluk(hic_baglandi ? 2 : 1, "bağlanılamadı (" + hata + "); çıkılıyor");
                    return hic_baglandi ? Cikis::Tamam : Cikis::Baglanamadi;
                }
                uyu(ayar.deneme_araligi_ms);
                continue;
            }
            hic_baglandi = true;
            ++ist.baglanti;
            gunluk(2, "bağlandı: 127.0.0.1:" + std::to_string(ayar.a1_port));
            baglanti_dongusu();
            gunluk(2, "bağlantı bitti");
            if (guvenli) {
                return Cikis::GuvenliBekleme;
            }
            if (cekirdek_hatasi) {
                return Cikis::Cekirdek;
            }
            if (geri_donus) {
                // A4-K11: sim zamanı geri başladı; bağlantı kapatıldı (oturum iki tarafta da
                // bitti, A1-K99), çekirdek yeni boot_count ile yeniden kurulur.
                gunluk(2, "sim zamanı geri başladı: çekirdek yeniden kuruluyor");
                ++ist.geri_donus;
                ++ist.yeniden_kurulum;
                geri_donus = false;
                if (!kur(static_cast<std::uint8_t>(IDA_RESET_SIL_REBOOT))) {
                    return Cikis::Cekirdek;
                }
            }
            bekleme_basi = Saat::now();
        }
        return Cikis::Tamam;
    }

    void olcum_yaz() const
    {
        const auto satir = [](const char* ad, const SureIstatistigi& s) {
            if (s.sayi == 0u) {
                std::fprintf(stderr, "ida_sil ölçüm: %s: çağrı yok\n", ad);
                return;
            }
            std::fprintf(stderr,
                         "ida_sil ölçüm: %s: n %llu, ort %.1f us, p50 %.0f us, p99 %.0f us, "
                         "en az %.1f us, en çok %.1f us\n",
                         ad, static_cast<unsigned long long>(s.sayi),
                         static_cast<double>(s.toplam_ns) / static_cast<double>(s.sayi) / 1000.0,
                         s.yuzdelik_us(50.0), s.yuzdelik_us(99.0),
                         static_cast<double>(s.en_az_ns) / 1000.0,
                         static_cast<double>(s.en_cok_ns) / 1000.0);
        };
        satir("çağrı turu (take..put_out)", ist.tur);
        satir("çekirdek (step/tick)", ist.cekirdek);
    }
};

// ---- Kosucu --------------------------------------------------------------------------------

Kosucu::Kosucu(Ayarlar ayar, Kancalar kancalar)
    : ic_(std::make_unique<Ic>(std::move(ayar), std::move(kancalar)))
{
}

Kosucu::~Kosucu() = default;

Cikis Kosucu::calistir()
{
    const Cikis c = ic_->calistir();
    if (ic_->ayar.olcum) {
        ic_->olcum_yaz();
    }
    return c;
}

void Kosucu::durdur()
{
    ic_->dur.store(true);
}

const Istatistik& Kosucu::istatistik() const
{
    return ic_->ist;
}

}  // namespace ida::sil
