// tests/kabul/kopru — kart olaylarının uygulanması ve yetkili durum (EMNİYET).
//
// Vaka 1: yetkili durum izin vermiyorken SAFETY_OPEN ve FIRE fırlatıcı durumunu değiştirmez
//         (A1-K86, K88, K89); her girdi tek başına yeterli engeldir.
// Vaka 2: uygun durumda zincir (A1-K86, K88, K106, K118).
// Vaka 3: tekillik (A1-K81, K82).
// Vaka 4: son geçerlilik ve emniyet revizyonu (A1-K84, K106).
// Vaka 5: fizik sınırındaki sıra (A1-K85; iskelet §3.2 "Kısıtlama hâkimdir").
//
// "Fırlatıcı durumu" = tüplerin dolu bitleri (tup_dolu) + süreli izinler (izin_acik).
#include <tuple>
#include <utility>

#include "ortak.hpp"

namespace kabul {
namespace {

constexpr int kApplied = w::kSonucApplied;
constexpr int kDuplicate = w::kSonucDuplicate;
constexpr int kRejExpired = w::kSonucRejExpired;
constexpr int kRejSafe = w::kSonucRejSafe;
constexpr int kRejTubeEmpty = w::kSonucRejTubeEmpty;
constexpr int kRejSession = w::kSonucRejSession;

bool iki_tup_dolu(const Duzenek& d) {
    return d.k.tup_dolu(0) && d.k.tup_dolu(1);
}

bool iki_izin_kapali(const Duzenek& d) {
    return !d.k.izin_acik(0) && !d.k.izin_acik(1);
}

// Tek olay gönderir (sıradaki TF; olağan yanıtla) ve sonucunu bekler.
int tek_olay(Duzenek& d, const OlayPlani& p, const YanitPlani& y = YanitPlani{},
             std::uint64_t* id = nullptr) {
    const GidenOlaylar g = olaylarla_yanitla(d, {p}, y);
    if (id != nullptr) {
        *id = g.olaylar.at(0).event_id;
    }
    return sonuc_kodu(d, g.olaylar.at(0).event_id);
}

// ---------------------------------------------------------------- Vaka 1

VAKA("A1-K86.acil_durdurma_tek_engel") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum ve izin veren duruma geçiş");
    KONTROL(izin_ac(d, 0) >= 0, "ön koşul: tüp 0 izni açılabilmeli");
    KONTROL(izin_ac(d, 1, 450'000'000u) >= 0, "ön koşul: tüp 1 izni açılabilmeli");
    KONTROL(d.k.izin_acik(0) && d.k.izin_acik(1), "ön koşul: iki izin açık");
    const std::uint32_t r0 = rev(d);
    d.emniyet(true, {}, {}, {}, {});
    const std::uint64_t B = d.b();
    d.adim();
    KONTROL(d.emniyet_b(B).acil_durdurma, "acil durdurma atandığı sınırda etkin");
    KONTROL(!d.emniyet_b(B).izin_acik[0] && !d.emniyet_b(B).izin_acik[1],
            "A1-K89: acil durdurma iki tüpün iznini aynı sınırda kapatır");
    KONTROL_ESIT(d.emniyet_b(B).emniyet_revizyonu, r0 + 1u,
                 "A1-K106 md.2: iki tüpü kapatan tek kısıtlayan atama tam bir artış");
    KONTROL(emniyet_esitle(d), "kart güncel revizyonu almalı");
    const std::uint32_t r1 = rev(d);

    // SAFETY_OPEN: damga güncel, fiş takılı, pim çıkık; tek engel acil durdurma.
    std::uint64_t id = 0;
    KONTROL_ESIT(tek_olay(d, acma(0), YanitPlani{}, &id), kRejSafe,
                 "A1-K86: acil durdurmada SAFETY_OPEN REJ_SAFE");
    KONTROL(iki_izin_kapali(d), "A1-K86: reddedilen SAFETY_OPEN izni açmaz");
    KONTROL(iki_tup_dolu(d), "tüpler dolu kalır");
    // FIRE (izin de kapalı; acil durdurma izinleri kapattığı için yalnız acil durdurma ile
    // FIRE kurulamaz).
    KONTROL_ESIT(tek_olay(d, atis(1)), kRejSafe, "A1-K88: acil durdurmada FIRE REJ_SAFE");
    KONTROL(iki_tup_dolu(d), "A1-K88: reddedilen FIRE tüpü boşaltmaz");
    KONTROL(iki_izin_kapali(d), "izinler kapalı kalır");
    KONTROL_ESIT(rev(d), r1, "A1-K106: reddedilen olay revizyonu artırmaz");
    KONTROL(d.k.acil_durdurma(), "acil durdurma etkin kalır");
}

VAKA("A1-K86.fis_cikik_tek_engel") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: tüp 0 izni açılabilmeli");
    d.emniyet({}, false, {}, {}, {});  // tüp 0 fişi çıkarıldı
    const std::uint64_t B = d.b();
    d.adim();
    KONTROL(!d.emniyet_b(B).izin_acik[0], "A1-K89: fişin çıkması tüpün iznini kapatır");
    KONTROL(emniyet_esitle(d), "eşitleme");
    const std::uint32_t r1 = rev(d);
    KONTROL_ESIT(tek_olay(d, acma(0)), kRejSafe,
                 "A1-K86: tüp 0 fişi çıkıkken SAFETY_OPEN(0) REJ_SAFE (tek engel)");
    KONTROL(!d.k.izin_acik(0), "izin 0 kapalı kalır");
    KONTROL_ESIT(tek_olay(d, atis(0)), kRejSafe, "A1-K88: fiş çıkıkken FIRE(0) REJ_SAFE");
    KONTROL(iki_tup_dolu(d), "tüpler dolu kalır");
    KONTROL_ESIT(rev(d), r1, "reddedilen olaylar revizyonu artırmaz");
    // Denetim: engel tüpe özgüdür; tüp 1 için aynı durumda izin açılır.
    KONTROL_ESIT(tek_olay(d, acma(1)), kApplied, "denetim: tüp 1 SAFETY_OPEN uygulanır");
    KONTROL(d.k.izin_acik(1) && !d.k.izin_acik(0), "yalnız tüp 1 izni açık");
}

VAKA("A1-K86.pim_takili_tek_engel") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: tüp 0 izni açılabilmeli");
    d.emniyet({}, {}, {}, true, {});  // tüp 0 pimi takıldı
    const std::uint64_t B = d.b();
    d.adim();
    KONTROL(!d.emniyet_b(B).izin_acik[0], "A1-K89: pimin takılması tüpün iznini kapatır");
    KONTROL(emniyet_esitle(d), "eşitleme");
    const std::uint32_t r1 = rev(d);
    KONTROL_ESIT(tek_olay(d, acma(0)), kRejSafe,
                 "A1-K86: tüp 0 pimi takılıyken SAFETY_OPEN(0) REJ_SAFE (tek engel)");
    KONTROL(!d.k.izin_acik(0), "izin 0 kapalı kalır");
    KONTROL_ESIT(tek_olay(d, atis(0)), kRejSafe, "A1-K88: pim takılıyken FIRE(0) REJ_SAFE");
    KONTROL(iki_tup_dolu(d), "tüpler dolu kalır");
    KONTROL_ESIT(rev(d), r1, "reddedilen olaylar revizyonu artırmaz");
    KONTROL_ESIT(tek_olay(d, acma(1)), kApplied, "denetim: tüp 1 SAFETY_OPEN uygulanır");
}

VAKA("A1-K88.izin_kapali_tek_engel") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(iki_izin_kapali(d), "ön koşul: izin hiç açılmadı");
    const std::uint32_t r1 = rev(d);
    // Acil durdurma yok, fiş takılı, pim çıkık, tüp dolu, damga güncel, yanıt ARMED ve
    // IS_SAFE = 0: tek engel izin.
    KONTROL_ESIT(tek_olay(d, atis(0)), kRejSafe, "A1-K88: izin kapalıyken FIRE REJ_SAFE");
    KONTROL(iki_tup_dolu(d), "A1-K88: tüp dolu kalır");
    KONTROL(iki_izin_kapali(d), "FIRE izin açmaz");
    KONTROL_ESIT(rev(d), r1, "reddedilen FIRE revizyonu artırmaz");
}

VAKA("A1-K86.izin_suresi_doldu") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    constexpr std::int64_t kHold = 60 * kMs;  // 3.9 aralığının alt ucu
    const std::int64_t ta = izin_ac(d, 0, static_cast<std::uint32_t>(kHold));
    KONTROL(ta >= 0, "ön koşul: izin açılır");
    const std::uint64_t A = Duzenek::sinir(ta);
    const std::uint64_t E = Duzenek::sinir(ta + kHold);
    const std::uint32_t r_once = rev(d);
    d.ilerle_kadar(ta + kHold + 4 * kP);
    for (std::uint64_t b = A; b < E; ++b) {
        KONTROL(d.emniyet_b(b).izin_acik[0],
                "A1-K86: izin t_apply + hold_ns'e kadar açık; sınır " << b << " (t_apply sınırı "
                                                                       << A << ")");
    }
    // EMNİYET okuması: "t_apply + hold_ns'e kadar açık" — t_apply + hold_ns anında başlayan
    // adımda izin kapalıdır.
    KONTROL(!d.emniyet_b(E).izin_acik[0],
            "A1-K86: izin t_apply + hold_ns sınırında kapanır (sınır " << E << ")");
    KONTROL_ESIT(d.emniyet_b(E).emniyet_revizyonu, r_once + 1u,
                 "A1-K106 md.5: süresi dolan izin tek artış");
    KONTROL(emniyet_esitle(d), "eşitleme");
    KONTROL_ESIT(tek_olay(d, atis(0)), kRejSafe,
                 "A1-K88: izin süresi dolduktan sonra (güncel damgayla) FIRE REJ_SAFE");
    KONTROL(iki_tup_dolu(d), "tüp dolu kalır");
}

VAKA("A1-K88.tup_bos") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin");
    KONTROL_ESIT(tek_olay(d, atis(0)), kApplied, "ön koşul: ilk FIRE uygulanır");
    KONTROL(!d.k.tup_dolu(0) && d.k.tup_dolu(1), "ön koşul: tüp 0 boş, tüp 1 dolu");
    KONTROL(d.k.izin_acik(0), "izin 0 hâlâ açık (tek engel tüpün boş olması)");
    const std::uint32_t r1 = rev(d);
    KONTROL_ESIT(tek_olay(d, atis(0)), kRejTubeEmpty, "A1-K88: boş tüpe FIRE REJ_TUBE_EMPTY");
    KONTROL(!d.k.tup_dolu(0) && d.k.tup_dolu(1), "tüp 1 dolu kalır");
    KONTROL_ESIT(rev(d), r1, "revizyon değişmez");
}

// ---------------------------------------------------------------- Vaka 2

VAKA("A1-K88.zincir_fire_yalniz_hedef_tup") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    const std::uint32_t r0 = rev(d);
    const std::uint64_t applied0 = olay_sayaci(d, w::kSonucApplied);

    // (1) İzin: SAFETY_OPEN(0), uygulama anı a_n (A1-K83).
    GidenOlaylar g = olaylarla_yanitla(d, {acma(0, 450'000'000u)});
    const std::uint64_t A = uygulama_siniri(g.tf);
    d.ilerle_kadar(static_cast<std::int64_t>(A) * kH);
    KONTROL(!d.emniyet_b(A - 1).izin_acik[0], "a_n'den önce izin kapalı");
    KONTROL(d.emniyet_b(A).izin_acik[0], "A1-K83, K86: izin a_n sınırında açılır");
    KONTROL(!d.emniyet_b(A).izin_acik[1], "öteki tüpün izni kapalı");
    auto r = sonuc_bekle(d, g.olaylar.at(0).event_id);
    KONTROL(r && r->result == w::kSonucApplied, "SAFETY_OPEN APPLIED");
    KONTROL(r && r->t_apply_ns == g.tf.t + kLact, "A1-K83: t_apply = a_n = t_n + L_act");
    KONTROL_ESIT(rev(d), r0, "A1-K106: uygulanan SAFETY_OPEN revizyonu artırmaz");

    // (2) FIRE(0): yalnız tüp 0 boşalır.
    g = olaylarla_yanitla(d, {atis(0)});
    const std::uint64_t A2 = uygulama_siniri(g.tf);
    d.ilerle_kadar(static_cast<std::int64_t>(A2) * kH);
    KONTROL(d.emniyet_b(A2 - 1).dolu[0], "a_n'den önce tüp 0 dolu");
    KONTROL(!d.emniyet_b(A2).dolu[0], "A1-K88: FIRE a_n sınırında tüp 0'ı boşaltır");
    KONTROL(d.emniyet_b(A2).dolu[1], "A1-K88: öteki tüp dolu kalır");
    r = sonuc_bekle(d, g.olaylar.at(0).event_id);
    KONTROL(r && r->result == w::kSonucApplied, "FIRE APPLIED");
    KONTROL(r && r->t_apply_ns == g.tf.t + kLact, "FIRE t_apply = a_n");
    KONTROL_ESIT(rev(d), r0, "A1-K106: FIRE revizyonu artırmaz");

    // (3) Aynı tüpe ikinci FIRE (yeni olay kimliği).
    KONTROL_ESIT(tek_olay(d, atis(0)), kRejTubeEmpty, "aynı tüpe ikinci FIRE reddedilir");
    KONTROL(!d.k.tup_dolu(0) && d.k.tup_dolu(1), "tüp 1 dolu kalır");
    KONTROL_ESIT(olay_sayaci(d, w::kSonucApplied), applied0 + 2u, "olay_sonuc_APPLIED = 2");
    KONTROL(olay_sayaci(d, w::kSonucRejTubeEmpty) >= 1u, "olay_sonuc_REJ_TUBE_EMPTY sayılır");
}

// A1-K118: izin açan olay, aynı batch_seq'in tutarlı yanıtı olmadan uygulanmaz.
VAKA("A1-K118.fire_tutarsiz_yanit") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    struct Durum {
        const char* ad;
        YanitPlani y;
        bool kesin;   // true: sonuç kesin REJ_SAFE (A1-K118); false: sonuç yok ya da REJ_SAFE
    };
    std::vector<Durum> durumlar;
    {
        YanitPlani y;
        y.flags = w::kThrValid;  // ARMED = 0
        durumlar.push_back({"ARMED = 0", y, true});
    }
    {
        YanitPlani y;
        y.flags = static_cast<std::uint16_t>(w::kThrValid | w::kArmed | w::kIsSafe);
        y.pwm = 1500;  // V4: THR_VALID ∧ IS_SAFE ⇒ nötr
        durumlar.push_back({"IS_SAFE = 1", y, true});
    }
    {
        YanitPlani y;
        y.pwm = 2500;  // V2: ESC aralığı dışında → yanıt reddedilir
        durumlar.push_back({"yanıt V2'de reddedildi", y, true});
    }
    {
        YanitPlani y;
        y.flags = 0;
        y.core_rc = w::kRcImuRejected;  // 4.6a: IMU_REJECTED'te yalnız SAFETY_CLOSE
        durumlar.push_back({"core_rc = IMU_REJECTED", y, false});
    }
    {
        YanitPlani y;
        y.flags = static_cast<std::uint16_t>(w::kThrValid | w::kIsSafe | w::kSrcTick | w::kArmed);
        y.pwm = 1500;  // tick: SRC_TICK = 1
        durumlar.push_back({"SRC_TICK = 1", y, false});
    }
    for (const auto& dr : durumlar) {
        KONTROL(izin_ac(d, 0, 200'000'000u) >= 0, "ön koşul: izin (" << dr.ad << ")");
        const GidenOlaylar g = olaylarla_yanitla(d, {atis(0)}, dr.y);
        const std::uint64_t id = g.olaylar.at(0).event_id;
        parti_ilerle(d, 8);
        const auto rr = d.kart.sonuc(id);
        if (dr.kesin) {
            KONTROL(rr && rr->result == w::kSonucRejSafe,
                    "A1-K118: " << dr.ad << " → FIRE kalıcı REJ_SAFE; gözlenen "
                                << (rr ? static_cast<int>(rr->result) : -1));
        } else {
            KONTROL(!rr || rr->result == w::kSonucRejSafe,
                    "A1-K118/K109: " << dr.ad << " → FIRE uygulanmaz; gözlenen "
                                     << (rr ? static_cast<int>(rr->result) : -1));
        }
        KONTROL(d.k.tup_dolu(0), "A1-K118: " << dr.ad << " → tüp 0 dolu kalır");
    }
    // Denetim: tutarlı yanıtla aynı FIRE uygulanır.
    KONTROL(izin_ac(d, 0) >= 0, "denetim izni");
    KONTROL_ESIT(tek_olay(d, atis(0)), kApplied, "denetim: tutarlı yanıtla FIRE uygulanır");
    KONTROL(!d.k.tup_dolu(0), "denetim: tüp 0 boşaldı");
}

VAKA("A1-K118.yanit_gelmedi") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin");
    YanitPlani y;
    y.gonder = false;  // olay gider, aynı partinin IDA_ACTUATOR'ı hiç gelmez
    const GidenOlaylar g = olaylarla_yanitla(d, {atis(0)}, y);
    const std::uint64_t id = g.olaylar.at(0).event_id;
    parti_ilerle(d, 8);
    const auto rr = d.kart.sonuc(id);
    KONTROL(rr && rr->result == w::kSonucRejSafe,
            "A1-K118: yanıt gelmediyse FIRE kalıcı REJ_SAFE; gözlenen "
                << (rr ? static_cast<int>(rr->result) : -1));
    KONTROL(d.k.tup_dolu(0), "tüp 0 dolu kalır");
    // SAFETY_OPEN için aynı kural.
    const GidenOlaylar g2 = olaylarla_yanitla(d, {acma(1)}, y);
    parti_ilerle(d, 8);
    const auto r2 = d.kart.sonuc(g2.olaylar.at(0).event_id);
    KONTROL(r2 && r2->result == w::kSonucRejSafe,
            "A1-K118: yanıt gelmediyse SAFETY_OPEN kalıcı REJ_SAFE; gözlenen "
                << (r2 ? static_cast<int>(r2->result) : -1));
    KONTROL(!d.k.izin_acik(1), "izin 1 kapalı kalır");
}

// ---------------------------------------------------------------- Vaka 3

VAKA("A1-K81.yinelenen_olay") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    // (a) Uygulanmış SAFETY_OPEN yinelenir: DUPLICATE + özgün t_apply; izin süresi uzamaz.
    constexpr std::int64_t kHold = 200 * kMs;
    const GidenOlaylar g = olaylarla_yanitla(d, {acma(0, static_cast<std::uint32_t>(kHold))});
    const mavlink_ida_event_t e1 = g.olaylar.at(0);
    const auto r1 = sonuc_bekle(d, e1.event_id);
    KONTROL(r1 && r1->result == w::kSonucApplied, "ön koşul: SAFETY_OPEN APPLIED");
    const std::int64_t t1 = r1 ? r1->t_apply_ns : 0;
    parti_ilerle(d, 4);
    yinele(d, e1);
    parti_ilerle(d, 6);
    KONTROL_ESIT(d.kart.sonuc_sayisi(e1.event_id), std::size_t{2},
                 "A1-K81: yinelemeye tek saklı sonuç");
    if (d.kart.sonuc_sayisi(e1.event_id) >= 2) {
        const mavlink_ida_event_result_t* ikinci = nullptr;
        int n = 0;
        for (const auto& s : d.kart.sonuclar) {
            if (s.event_id == e1.event_id && ++n == 2) {
                ikinci = &s;
            }
        }
        KONTROL(ikinci != nullptr && ikinci->result == w::kSonucDuplicate,
                "A1-K81: APPLIED saklıysa DUPLICATE");
        KONTROL(ikinci != nullptr && ikinci->t_apply_ns == t1, "A1-K81: özgün t_apply");
    }
    const std::uint64_t E = Duzenek::sinir(t1 + kHold);
    d.ilerle_kadar(t1 + kHold + kP);
    KONTROL(d.emniyet_b(E - 1).izin_acik[0], "izin t1 + hold'a kadar açık");
    KONTROL(!d.emniyet_b(E).izin_acik[0], "A1-K81: yineleme izin süresini uzatmaz");

    // (b) Uygulanmış FIRE yinelenir: yeniden işlenmez (REJ_TUBE_EMPTY değil DUPLICATE).
    KONTROL(emniyet_esitle(d), "eşitleme");
    KONTROL(izin_ac(d, 1, 450'000'000u) >= 0, "ön koşul: izin 1");
    const GidenOlaylar gf = olaylarla_yanitla(d, {atis(1)});
    const mavlink_ida_event_t e2 = gf.olaylar.at(0);
    const auto rf = sonuc_bekle(d, e2.event_id);
    KONTROL(rf && rf->result == w::kSonucApplied, "ön koşul: FIRE(1) APPLIED");
    KONTROL(!d.k.tup_dolu(1) && d.k.tup_dolu(0), "yalnız tüp 1 boşaldı");
    yinele(d, e2);
    parti_ilerle(d, 6);
    bool dup_goruldu = false;
    bool baska = false;
    int n2 = 0;
    for (const auto& s : d.kart.sonuclar) {
        if (s.event_id != e2.event_id || ++n2 < 2) {
            continue;
        }
        dup_goruldu = dup_goruldu || (s.result == w::kSonucDuplicate &&
                                      rf.has_value() && s.t_apply_ns == rf->t_apply_ns);
        baska = baska || s.result != w::kSonucDuplicate;
    }
    KONTROL(dup_goruldu && !baska, "A1-K81: FIRE yinelemesi DUPLICATE + özgün t_apply");
    KONTROL(d.k.tup_dolu(0), "tüp 0 dolu kalır");

    // (c) Reddedilmiş olay kimliği sonradan uygulanmaz (kalıcı ret; A1-K86, K81).
    d.emniyet({}, false, {}, {}, {});
    KONTROL(emniyet_esitle(d), "eşitleme (fiş 0 çıkık)");
    const GidenOlaylar gr = olaylarla_yanitla(d, {acma(0)});
    mavlink_ida_event_t e3 = gr.olaylar.at(0);
    KONTROL_ESIT(sonuc_kodu(d, e3.event_id), kRejSafe, "ön koşul: REJ_SAFE");
    d.emniyet({}, true, {}, {}, {});
    KONTROL(emniyet_esitle(d), "eşitleme (fiş 0 takılı)");
    e3.safety_rev = d.kart.son_rev();  // aynı kimlik, güncel damga
    e3.batch_seq = d.kart.partiler.back().tf.seq;
    e3.t_decide_ns = d.kart.partiler.back().tf.t;
    yinele(d, e3);
    parti_ilerle(d, 6);
    bool uygulandi = false;
    for (const auto& s : d.kart.sonuclar) {
        if (s.event_id == e3.event_id && s.result != w::kSonucRejSafe) {
            uygulandi = true;
        }
    }
    KONTROL(!uygulandi, "A1-K86: reddedilen kimlik kalıcı REJ_SAFE; yeniden gelişte de");
    KONTROL(!d.k.izin_acik(0), "A1-K86: kalıcı ret: izin 0 açılmaz");
}

VAKA("A1-K81.oturum_sonrasi_ayni_kimlik") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    const GidenOlaylar g = olaylarla_yanitla(d, {acma(0, 450'000'000u)});
    const mavlink_ida_event_t e1 = g.olaylar.at(0);
    const auto r1 = sonuc_bekle(d, e1.event_id);
    KONTROL(r1 && r1->result == w::kSonucApplied, "ön koşul: APPLIED");
    KONTROL(d.k.izin_acik(0), "ön koşul: izin açık");
    const std::uint32_t r0 = rev(d);
    const std::uint64_t B = d.b();
    const std::uint16_t sess1 = d.k.sess();
    d.kart.oturum_kapat_gonder(w::kKapanisShutdown, d.t_sonraki());
    d.adim();
    // Sahte kart yeni el sıkışmayı aynı taşımada hemen yanıtlayabilir; bu yüzden oturumun
    // bittiği oturum kimliğinin değişmesinden ya da KOSUYOR dışı durumdan anlaşılır.
    KONTROL(d.k.durum() != ida::kopru::OturumDurumu::kosuyor || d.k.sess() != sess1,
            "A1-K21: SESSION_CLOSE oturumu bitirir");
    KONTROL(!d.emniyet_b(B).izin_acik[0], "A1-K23: oturum sonunda izinler kapanır");
    KONTROL_ESIT(d.emniyet_b(B).emniyet_revizyonu, r0 + 1u, "A1-K106 md.4: oturum sonu tek artış");
    KONTROL(d.yeniden_kur(), "yeni oturum kurulur");
    KONTROL(emniyet_esitle(d), "eşitleme");
    const bool yeniden_baslatildi = d.kart.boot_count() != 1u;

    // (a) Aynı kimlik, eski oturumun yüküyle (eski sess): V1'de reddedilir, etkisiz.
    yinele(d, e1);
    // (b) Aynı kimlik, yeni oturumun alanlarıyla (sess, batch_seq, t_decide, güncel damga).
    mavlink_ida_event_t e1b = e1;
    e1b.sess = d.kart.sess();
    e1b.batch_seq = d.kart.partiler.back().tf.seq;
    e1b.t_decide_ns = d.kart.partiler.back().tf.t;
    e1b.safety_rev = d.kart.son_rev();
    yinele(d, e1b);
    parti_ilerle(d, 8);
    KONTROL(!d.k.izin_acik(0), "A1-K81: oturumdan sonra aynı kimlik izni yeniden açmaz");
    const int beklenen = yeniden_baslatildi ? kRejSession : kDuplicate;
    int sira = 0;
    int sonraki = 0;
    bool yanlis = false;
    for (const auto& s : d.kart.sonuclar) {
        if (s.event_id != e1.event_id || ++sira == 1) {
            continue;  // ilk sonuç özgün APPLIED
        }
        ++sonraki;
        if (s.result != beklenen) {
            yanlis = true;
        }
        if (!yeniden_baslatildi && r1 && s.t_apply_ns != r1->t_apply_ns) {
            yanlis = true;
        }
    }
    KONTROL_ESIT(d.kart.sonuc_sayisi(e1.event_id), std::size_t{2},
                 "ilk APPLIED + yeni oturumdaki geçerli yineleme için tek sonuç (eski sess'li "
                 "yineleme V1'de atılır)");
    KONTROL(sonraki >= 1 && !yanlis,
            "A1-K81/K82: yeni oturumda aynı kimlik "
                << (yeniden_baslatildi ? "REJ_SESSION (kart yeniden başlatıldı)"
                                       : "DUPLICATE + özgün t_apply"));
}

VAKA("A1-K82.farkli_boot_ayni_sayac") {
    SahteKart::Ayar ka;
    ka.boot_count = 5;
    Duzenek d(ida::kopru::CalistirmaKipi::hil_1x, nullptr, &ka);
    KONTROL(hazirla(d), "kurulum");
    // Eski açılışın ilk olayı: (5 << 32) | 1.
    const GidenOlaylar g = olaylarla_yanitla(d, {acma(0, 60'000'000u)});
    const std::uint64_t eski = g.olaylar.at(0).event_id;
    KONTROL_ESIT(eski, (std::uint64_t{5} << 32) | 1u, "olay kimliği düzeni (A4-K61)");
    KONTROL_ESIT(sonuc_kodu(d, eski), kApplied, "ön koşul: APPLIED");
    // Kart sıfırlanır (planlanmamış): yeni açılış 6, olay sayacı yeniden 1'den.
    const std::uint16_t sess1 = d.k.sess();
    d.kart.sifirlan(d.t_sonraki());
    d.adim();
    KONTROL(d.k.durum() != ida::kopru::OturumDurumu::kosuyor || d.k.sess() != sess1,
            "A1-K23: KOSUYOR'da BOARD_HELLO oturumu bitirir");
    KONTROL(d.yeniden_kur(), "yeni oturum");
    KONTROL(emniyet_esitle(d), "eşitleme");
    KONTROL_ESIT(d.kart.boot_count(), 6u, "yeni açılış");
    // Yeni açılışın ilk olayı aynı sayaçla: (6 << 32) | 1 — farklı olaydır, kendi koşuluyla
    // değerlendirilir.
    const GidenOlaylar g2 = olaylarla_yanitla(d, {acma(1, 450'000'000u)});
    const std::uint64_t yeni = g2.olaylar.at(0).event_id;
    KONTROL_ESIT(yeni, (std::uint64_t{6} << 32) | 1u, "aynı sayaç, yeni açılış");
    KONTROL_ESIT(sonuc_kodu(d, yeni), kApplied,
                 "iskelet §3.2, A4-K61: yeni açılışın olayı eski açılışınkiyle karışmaz");
    KONTROL(d.k.izin_acik(1), "izin 1 açık");
    // Eski açılışın kimliğini taşıyan olaylar REJ_SESSION.
    OlayPlani p = acma(0);
    p.degistir = [](mavlink_ida_event_t& e) {
        e.event_id = (std::uint64_t{5} << 32) | (e.event_id & 0xFFFFFFFFu);
    };
    KONTROL_ESIT(tek_olay(d, p), kRejSession, "A1-K82: eski boot_count'lu SAFETY_OPEN");
    KONTROL(!d.k.izin_acik(0), "izin 0 kapalı kalır");
    OlayPlani f = atis(1);
    f.degistir = [](mavlink_ida_event_t& e) {
        e.event_id = (std::uint64_t{7} << 32) | (e.event_id & 0xFFFFFFFFu);
    };
    KONTROL_ESIT(tek_olay(d, f), kRejSession, "A1-K82: başka boot_count'lu FIRE");
    KONTROL(iki_tup_dolu(d), "tüpler dolu kalır");
}

// ---------------------------------------------------------------- Vaka 4

VAKA("A1-K84.suresi_gecmis_olay") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    const std::uint64_t gec0 = d.k.sayaclar().olay_gec;
    // Olay TF(n)'ye bağlı; yanıt zamanında (olayı saymaz), olay 25 ms sonra gelir
    // (apply_by 20 ms; A1-K84).
    YanitPlani y;
    y.olaylari_say = false;
    y.olay_gecikme = 25 * kMs;
    GidenOlaylar g = olaylarla_yanitla(d, {acma(0)}, y);
    KONTROL_ESIT(sonuc_kodu(d, g.olaylar.at(0).event_id), kRejExpired,
                 "A1-K84: süresi geçmiş SAFETY_OPEN REJ_EXPIRED");
    KONTROL(!d.k.izin_acik(0), "A1-K84: süresi geçmiş olay izni açmaz");
    KONTROL(d.k.sayaclar().olay_gec > gec0, "A1-K83: geç olay sayılır");
    // FIRE: izin açıkken, süresi geçmiş.
    KONTROL(izin_ac(d, 1, 450'000'000u) >= 0, "ön koşul: izin 1");
    g = olaylarla_yanitla(d, {atis(1)}, y);
    KONTROL_ESIT(sonuc_kodu(d, g.olaylar.at(0).event_id), kRejExpired,
                 "A1-K84, K88: süresi geçmiş FIRE REJ_EXPIRED");
    KONTROL(iki_tup_dolu(d), "A1-K84: süresi geçmiş FIRE tüpü boşaltmaz");
}

VAKA("A1-K84.son_gecerlilik_siniri") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    // Sınır: olay sinir_isle(B−1)'den sonra, sinir_isle(B)'den önce gelir; B = t_n + apply_by.
    // t_apply = t_decide + apply_by → uygulanır (A1-K84 yalnız t_apply > ... ise reddeder).
    YanitPlani y;
    y.olaylari_say = false;
    y.olay_gecikme = 20 * kMs;
    GidenOlaylar g = olaylarla_yanitla(d, {acma(0)}, y);
    auto r = sonuc_bekle(d, g.olaylar.at(0).event_id);
    KONTROL(r && r->result == w::kSonucApplied,
            "A1-K84: t_apply = t_decide + apply_by_ns → uygulanır; gözlenen "
                << (r ? static_cast<int>(r->result) : -1));
    KONTROL(r && r->t_apply_ns == g.tf.t + 20 * kMs,
            "A1-K83: geç olay alımından sonraki ilk sınırda (t_n + 20 ms)");
    // Bir adım sonrası: reddedilir.
    y.olay_gecikme = 20 * kMs + kH;
    g = olaylarla_yanitla(d, {acma(1)}, y);
    r = sonuc_bekle(d, g.olaylar.at(0).event_id);
    KONTROL(r && r->result == w::kSonucRejExpired,
            "A1-K84: t_apply = t_decide + apply_by + h → REJ_EXPIRED; gözlenen "
                << (r ? static_cast<int>(r->result) : -1));
    KONTROL(!d.k.izin_acik(1), "izin 1 açılmaz");
}

VAKA("A1-K106.bayat_revizyon") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin 0");
    const std::uint64_t rev_ret0 = d.k.sayaclar().olay_rev_ret;
    // (a) Karar güncel damgayla verilir; uygulanmadan önce başka tüpte kısıtlama (pim 1
    // takılır) revizyonu artırır → bekleyen yetki düşer.
    GidenOlaylar g = olaylarla_yanitla(d, {atis(0)});
    d.emniyet({}, {}, {}, {}, true);
    KONTROL_ESIT(sonuc_kodu(d, g.olaylar.at(0).event_id), kRejSafe,
                 "A1-K106, K88: araya giren artıştan sonra FIRE REJ_SAFE");
    KONTROL(d.k.tup_dolu(0), "tüp 0 dolu kalır");
    KONTROL(d.k.izin_acik(0), "izin 0 açık kalır (yalnız damga bayat)");
    KONTROL(d.k.sayaclar().olay_rev_ret > rev_ret0, "olay_rev_ret sayılır");
    KONTROL(emniyet_esitle(d), "eşitleme");
    // (b) Damga güncelden eski ve yeni.
    for (const std::int64_t fark : {-1, +1, +5}) {
        OlayPlani p = atis(0);
        p.degistir = [fark](mavlink_ida_event_t& e) {
            e.safety_rev = static_cast<std::uint32_t>(static_cast<std::int64_t>(e.safety_rev) + fark);
        };
        KONTROL_ESIT(tek_olay(d, p), kRejSafe, "A1-K106: damga güncel + (" << fark << ") → REJ_SAFE");
        KONTROL(d.k.tup_dolu(0), "tüp 0 dolu kalır");
    }
    OlayPlani pa = acma(1);
    d.emniyet({}, {}, {}, {}, false);
    KONTROL(emniyet_esitle(d), "eşitleme");
    pa.degistir = [](mavlink_ida_event_t& e) { e.safety_rev = e.safety_rev - 1u; };
    KONTROL_ESIT(tek_olay(d, pa), kRejSafe, "A1-K106, K86: bayat damgalı SAFETY_OPEN REJ_SAFE");
    KONTROL(!d.k.izin_acik(1), "izin 1 kapalı kalır");
    // Denetim: güncel damgayla uygulanır.
    KONTROL_ESIT(tek_olay(d, acma(1)), kApplied, "denetim: güncel damgalı SAFETY_OPEN");
}

VAKA("A1-K106.sifir_revizyon") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    const std::uint64_t alan0 = d.k.sayaclar().alan_ret;
    OlayPlani p = acma(0);
    p.degistir = [](mavlink_ida_event_t& e) { e.safety_rev = 0; };
    const GidenOlaylar g = olaylarla_yanitla(d, {p});
    parti_ilerle(d, 8);
    const auto r = d.kart.sonuc(g.olaylar.at(0).event_id);
    KONTROL(!r || r->result != w::kSonucApplied, "A1-K106: sıfır damgalı SAFETY_OPEN uygulanmaz");
    KONTROL(!d.k.izin_acik(0), "izin 0 kapalı kalır");
    KONTROL(d.k.sayaclar().alan_ret > alan0, "Tablo 4.6 V4: alan_ret sayılır");
    KONTROL(izin_ac(d, 1, 450'000'000u) >= 0, "ön koşul: izin 1");
    OlayPlani f = atis(1);
    f.degistir = [](mavlink_ida_event_t& e) { e.safety_rev = 0; };
    const GidenOlaylar g2 = olaylarla_yanitla(d, {f});
    parti_ilerle(d, 8);
    const auto r2 = d.kart.sonuc(g2.olaylar.at(0).event_id);
    KONTROL(!r2 || r2->result != w::kSonucApplied, "A1-K106: sıfır damgalı FIRE uygulanmaz");
    KONTROL(iki_tup_dolu(d), "tüpler dolu kalır");
}

// ---------------------------------------------------------------- Vaka 5

// İki olayı aynı yanıtla gönderir; ikisinin sonucunu döndürür.
std::pair<int, int> iki_olay(Duzenek& d, const OlayPlani& a, const OlayPlani& b,
                             std::uint64_t* A = nullptr) {
    const GidenOlaylar g = olaylarla_yanitla(d, {a, b});
    if (A != nullptr) {
        *A = uygulama_siniri(g.tf);
    }
    const int ra = sonuc_kodu(d, g.olaylar.at(0).event_id);
    const int rb = sonuc_kodu(d, g.olaylar.at(1).event_id);
    return {ra, rb};
}

VAKA("A1-K85.close_open_ayni_sinir") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    // Tur 1: SAFETY_CLOSE, sonra SAFETY_OPEN (aynı tüp, aynı yuva, aynı damga).
    std::uint32_t r0 = rev(d);
    std::uint64_t A = 0;
    auto [c1, o1] = iki_olay(d, kapama(0), acma(0), &A);
    KONTROL_ESIT(c1, kApplied, "A1-K87: SAFETY_CLOSE uygulanır");
    KONTROL_ESIT(o1, kRejSafe, "A1-K85 (3): kısıtlayan önce; SAFETY_OPEN REJ_SAFE");
    KONTROL(!d.emniyet_b(A).izin_acik[0], "izin kapalı");
    KONTROL_ESIT(d.emniyet_b(A).emniyet_revizyonu, r0 + 1u, "A1-K106 md.3: tek artış");
    // Tur 2: geliş sırası ters.
    KONTROL(emniyet_esitle(d), "eşitleme");
    r0 = rev(d);
    auto [o2, c2] = iki_olay(d, acma(0), kapama(0), &A);
    KONTROL_ESIT(c2, kApplied, "ters sıra: SAFETY_CLOSE uygulanır");
    KONTROL_ESIT(o2, kRejSafe, "ters sıra: SAFETY_OPEN REJ_SAFE (geliş sırasından bağımsız)");
    KONTROL(!d.emniyet_b(A).izin_acik[0], "ters sıra: izin kapalı");
    KONTROL_ESIT(d.emniyet_b(A).emniyet_revizyonu, r0 + 1u, "ters sıra: tek artış");
    // Tur 3: izin açıkken yenileme + kapama (255 = iki tüp).
    KONTROL(emniyet_esitle(d), "eşitleme");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin 0");
    auto [o3, c3] = iki_olay(d, acma(0), kapama(255), &A);
    KONTROL_ESIT(c3, kApplied, "SAFETY_CLOSE(255) uygulanır");
    KONTROL_ESIT(o3, kRejSafe, "yenileme SAFETY_OPEN REJ_SAFE");
    KONTROL(!d.emniyet_b(A).izin_acik[0] && !d.emniyet_b(A).izin_acik[1],
            "A1-K87: iki tüpün izni kapalı");
}

void acil_ve_olay(bool olaylar_once) {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin 0");
    const std::uint32_t r0 = rev(d);
    YanitPlani y;
    if (!olaylar_once) {
        // Olaylar ve yanıt, acil durdurma ataması yapıldıktan sonra, a_n sınırından hemen önce
        // gelir: t_birak = t_n + L_act → sinir_isle(A − 1) ve atamadan sonra, sinir_isle(A)'dan
        // önce teslim.
        y.olay_gecikme = kLact;
        y.yanit_gecikme = kLact;
    }
    const GidenOlaylar g = olaylarla_yanitla(d, {atis(0), acma(1)}, y);
    const std::uint64_t A = uygulama_siniri(g.tf);
    d.sinir_sonrasi = [&d, A](std::uint64_t b) {
        if (b + 1u == A) {
            d.emniyet(true, {}, {}, {}, {});  // A'da uygulanır (A1-K85 adım 2)
        }
    };
    d.ilerle_kadar(static_cast<std::int64_t>(A) * kH);
    d.sinir_sonrasi = nullptr;
    const char* sira = olaylar_once ? "olaylar önce geldi" : "acil durdurma önce atandı";
    KONTROL(d.emniyet_b(A).acil_durdurma, "acil durdurma A'da etkin (" << sira << ")");
    KONTROL(d.emniyet_b(A).dolu[0], "A1-K85: aynı sınırda acil durdurma + FIRE → tüp dolu ("
                                        << sira << ")");
    KONTROL(!d.emniyet_b(A).izin_acik[0] && !d.emniyet_b(A).izin_acik[1],
            "izinler kapalı (" << sira << ")");
    KONTROL_ESIT(d.emniyet_b(A).emniyet_revizyonu, r0 + 1u, "A1-K106 md.2 tek artış (" << sira << ")");
    KONTROL_ESIT(sonuc_kodu(d, g.olaylar.at(0).event_id), kRejSafe, "FIRE REJ_SAFE (" << sira << ")");
    KONTROL_ESIT(sonuc_kodu(d, g.olaylar.at(1).event_id), kRejSafe,
                 "SAFETY_OPEN REJ_SAFE (" << sira << ")");
    KONTROL(iki_tup_dolu(d) && iki_izin_kapali(d), "sonra da değişmez (" << sira << ")");
}

VAKA("A1-K85.acil_durdurma_ve_olay_ayni_sinir") {
    acil_ve_olay(true);
    acil_ve_olay(false);
}

VAKA("A1-K85.close_fire_ayni_sinir") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    for (int tur = 0; tur < 2; ++tur) {
        KONTROL(emniyet_esitle(d), "eşitleme");
        KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin 0");
        std::uint64_t A = 0;
        int rc = 0;
        int rf = 0;
        if (tur == 0) {
            std::tie(rc, rf) = iki_olay(d, kapama(0), atis(0), &A);
        } else {
            std::tie(rf, rc) = iki_olay(d, atis(0), kapama(255), &A);
        }
        const char* sira = (tur == 0) ? "CLOSE önce" : "FIRE önce";
        KONTROL_ESIT(rc, kApplied, "SAFETY_CLOSE uygulanır (" << sira << ")");
        KONTROL_ESIT(rf, kRejSafe, "A1-K85 (3): aynı sınırda CLOSE + FIRE → FIRE REJ_SAFE (" << sira << ")");
        KONTROL(d.emniyet_b(A).dolu[0], "tüp 0 dolu (" << sira << ")");
        KONTROL(!d.emniyet_b(A).izin_acik[0], "izin 0 kapalı (" << sira << ")");
    }
    KONTROL(iki_tup_dolu(d), "tüpler dolu");
}

VAKA("A1-K85.gevseme_olaya_yaramaz") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    d.emniyet(true, {}, {}, {}, {});
    KONTROL(emniyet_esitle(d), "eşitleme (acil durdurma etkin)");
    const std::uint32_t r1 = rev(d);
    // Olay TF(n)'de; acil durdurmanın kalkması (senaryonun gevşeten ataması) aynı uygulama
    // sınırında (a_n).
    const GidenOlaylar g = olaylarla_yanitla(d, {acma(0)});
    const std::uint64_t A2 = uygulama_siniri(g.tf);
    d.sinir_sonrasi = [&d, A2](std::uint64_t b) {
        if (b + 1u == A2) {
            d.emniyet(false, {}, {}, {}, {});
        }
    };
    d.ilerle_kadar(static_cast<std::int64_t>(A2) * kH);
    d.sinir_sonrasi = nullptr;
    KONTROL_ESIT(sonuc_kodu(d, g.olaylar.at(0).event_id), kRejSafe,
                 "A1-K85: olay aynı sınırdaki gevşetmeden yararlanmaz (REJ_SAFE)");
    KONTROL(!d.emniyet_b(A2).izin_acik[0], "izin 0 açılmaz");
    KONTROL(!d.emniyet_b(A2).acil_durdurma,
            "A1-K85 (4): revizyon 1–3'te artmadıysa gevşetme aynı sınırda uygulanır");
    KONTROL_ESIT(d.emniyet_b(A2).emniyet_revizyonu, r1 + 1u, "A1-K106 md.6: gevşetme tek artış");
}

VAKA("A1-K85.kisitlama_varken_gevseme_reddi") {
    // (i) Aynı sınırda SAFETY_CLOSE (adım 3, revizyon artar) + acil durdurmanın kalkması.
    {
        Duzenek d;
        KONTROL(hazirla(d), "kurulum");
        d.emniyet(true, {}, {}, {}, {});
        KONTROL(emniyet_esitle(d), "eşitleme");
        const std::uint32_t r1 = rev(d);
        const GidenOlaylar g = olaylarla_yanitla(d, {kapama(1)});
        const std::uint64_t A = uygulama_siniri(g.tf);
        d.sinir_sonrasi = [&d, A](std::uint64_t b) {
            if (b + 1u == A) {
                d.emniyet(false, {}, {}, {}, {});
            }
        };
        d.ilerle_kadar(static_cast<std::int64_t>(A) * kH + 40 * kH);
        d.sinir_sonrasi = nullptr;
        KONTROL_ESIT(sonuc_kodu(d, g.olaylar.at(0).event_id), kApplied, "SAFETY_CLOSE uygulanır");
        KONTROL(d.emniyet_b(A).acil_durdurma,
                "A1-K85 (4): revizyon adım 3'te arttı → aynı sınırdaki gevşetme reddedilir");
        bool kalkti = false;
        for (std::uint64_t b = A; b < A + 40u; ++b) {
            kalkti = kalkti || !d.emniyet_b(b).acil_durdurma;
        }
        KONTROL(!kalkti, "A1-K85 (4): reddedilen gevşetme yeniden denenmez");
        KONTROL_ESIT(d.emniyet_b(A).emniyet_revizyonu, r1 + 1u, "yalnız SAFETY_CLOSE artışı");
    }
    // (ii) Aynı atamada kısıtlayan (fiş 0 çıkar) ve gevşeten (pim 1 çıkar).
    {
        Duzenek d;
        KONTROL(d.kur(), "kurulum");
        d.ilerle(8);
        d.emniyet(false, true, true, false, true);  // pim 1 takılı
        KONTROL(emniyet_esitle(d), "eşitleme");
        const std::uint32_t r1 = rev(d);
        d.emniyet({}, false, {}, {}, false);
        const std::uint64_t B = d.b();
        d.ilerle(40);
        KONTROL(!d.emniyet_b(B).fis[0], "kısıtlayan atama uygulanır");
        bool cikti = false;
        for (std::uint64_t b = B; b < B + 40u; ++b) {
            cikti = cikti || !d.emniyet_b(b).pim[1];
        }
        KONTROL(!cikti, "A1-K85 (4): aynı sınırda revizyon arttı → gevşeten atama reddedilir, "
                        "yeniden denenmez");
        KONTROL_ESIT(d.emniyet_b(B).emniyet_revizyonu, r1 + 1u, "A1-K106 md.2: tek artış");
    }
}

}  // namespace
}  // namespace kabul
