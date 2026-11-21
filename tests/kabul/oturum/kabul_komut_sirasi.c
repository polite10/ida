/*
 * Komut sırası kabul testleri (W3-oturum kabul md.2).
 *
 * Dayanılan kurallar: A1-K63 (D-17: daraltıcı mandala alındığı anda, kendisinden önce
 * alınmış ve hiçbir çağrıya girmemiş genişleticiler — açık partide, bekleyen partide, erteleme
 * tamponunda — atılır ve REJECTED/NARROWED alır; sonuç nihaidir; son 32 kimlik tutulur ve
 * yinelemesi yeniden işlenmez; kimlikler last_cmd_id'ye girer), A1-K64 (alanı bozuk komut
 * LINK_BAD_FIELD, mandala ve çekirdeğe girmez), A1-K66, A1-K67 (platform sonuçları zorunlu
 * sınıfta; COALESCED), A1-K110 (sınıflar), A4-K46 (mandal, bir sonraki çağrı, coalesced),
 * A4-K102 (bekleyen parti, birleştirme, varış sırası), A4-K104 (genişletici listesi), A4-K105
 * (erteleme), W3-K10 (take ile put_out arasında rx çağrılmaz; ihlal oturumu PROTOCOL ile
 * bitirir), W3-K11 (nihai kimlik geçerli daraltıcı olarak gelirse daraltıcı uygulanır, başka
 * her biçimde NARROWED yinelenir).
 *
 * "Bekleyen parti" (A4-K102) burada kapanmış ama henüz take ile alınmamış partidir: W3-K10
 * gereği çağrı sürerken rx olmadığından parti yalnız take'ten önce kapanıp bekleyebilir.
 * Her senaryo kart (ROL_KART, saat sabit) ve SIL rolünde koşar.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_oturum.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- Yardımcı iddialar ------------------------------------------------------------------ */

/* A1-K63, K67, K110: platformun attığı genişletici tam bir REJECTED/NARROWED sonucu almış,
   sonuç zorunlu-güvenilir sınıfta ve ACK türünde (A4-K43: tür komut türüne bağlıdır). */
static void atildi(uint32_t id)
{
    const ko_kimlik_t *k = ko_kimlik(id);
    char ileti[160];
    (void)snprintf(ileti, sizeof ileti,
                   "cmd_id %u: daraltıcıdan önce alınmış genişletici REJECTED/NARROWED almalı "
                   "(A1-K63)", (unsigned)id);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1u, k->n_sonuc, ileti);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDAW_ACK_REJECTED, k->sonuc[0].ack, ileti);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDAW_REASON_NARROWED, k->sonuc[0].reason, ileti);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDAW_RES_KIND_ACK, k->sonuc[0].kind, ileti);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_LINK_SINIF_ZORUNLU, k->sonuc[0].sinif,
                                    "platform sonucu zorunlu-güvenilir sınıfta (A1-K67, K110)");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(ko_sess(), k->sonuc[0].sess, ileti);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, k->girdi, "atılan genişletici çağrıya girmiş");
}

static void hic_girmedi(uint32_t id)
{
    char ileti[160];
    (void)snprintf(ileti, sizeof ileti,
                   "cmd_id %u hiçbir çekirdek çağrısının girdisine girmemeli (A1-K63)", (unsigned)id);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, ko_kimlik(id)->girdi, ileti);
}

static void sonucsuz(uint32_t id)
{
    char ileti[160];
    (void)snprintf(ileti, sizeof ileti,
                   "cmd_id %u: nötr komut daraltıcıdan etkilenmez, platform sonucu yok (A1-K63)",
                   (unsigned)id);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, ko_kimlik(id)->n_sonuc, ileti);
}

static void bir_kez_girdi(uint32_t id)
{
    char ileti[160];
    (void)snprintf(ileti, sizeof ileti, "cmd_id %u tam bir çağrıya girmeli", (unsigned)id);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1u, ko_kimlik(id)->girdi, ileti);
}

/* `bas`tan sonraki ilk çağrı (A4-K46: mandal bir sonraki çağrıda verilir). */
static const ko_cagri_t *ilk_cagri(size_t bas)
{
    TEST_ASSERT_TRUE_MESSAGE(ko_cagri_boy() > bas, "beklenen çekirdek çağrısı olmadı");
    return ko_cagri(bas);
}

static void mandal_var(const ko_cagri_t *c, uint32_t bit_dizini, uint32_t id)
{
    char ileti[160];
    (void)snprintf(ileti, sizeof ileti,
                   "daraltıcı %u bir sonraki çağrının mandalında (bit %u) olmalı (A1-K63, A4-K46)",
                   (unsigned)id, (unsigned)bit_dizini);
    TEST_ASSERT_TRUE_MESSAGE((c->narrow.mask & (1u << bit_dizini)) != 0u, ileti);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(id, c->narrow.cmd_id[bit_dizini], ileti);
}

static void bozuk_yok(void)
{
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, ko_bozuk_cerceve(), "kartın çözülemeyen çerçevesi");
}

/* ---- (a) Açık parti: her genişletici biçimi; nötrler etkilenmez -------------------------- */
/* A1-K63 (a), A4-K104 (genişletici listesi), A4-K46. */
static void acik_parti(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x1201u);

    /* (i) ARM, SET_MODE(GUIDED), FAILSAFE_RESET + nötr HEARTBEAT → DISARM */
    ko_simple(100u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple(101u, IDAW_CMD_TYPE_SET_MODE, IDA_MODE_GUIDED);
    ko_simple(102u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_simple(103u, IDAW_CMD_TYPE_FAILSAFE_RESET, 0u);
    size_t c0 = ko_cagri_boy();
    ko_simple(104u, IDAW_CMD_TYPE_DISARM, 0u);
    atildi(100u);
    atildi(101u);
    atildi(103u);
    sonucsuz(102u);
    ko_tf(2u);
    ko_pompala();
    mandal_var(ilk_cagri(c0), 0u, 104u);
    TEST_ASSERT_TRUE_MESSAGE(ko_cagri_komut_iceriyor(ilk_cagri(c0), 102u),
                             "nötr komut partisinin çağrısına girmeli (A1-K63, A4-K16)");

    /* (ii) FC_ARM, FC_FIRE, FC_RESET, FC_TEST_ENTER → FC_ABORT */
    ko_fc(110u, IDAW_FC_OP_ARM, 0u, 0u);
    ko_fc(111u, IDAW_FC_OP_FIRE, 1u, 0u);
    ko_fc(112u, IDAW_FC_OP_RESET, 0u, 0u);
    ko_fc(113u, IDAW_FC_OP_TEST_ENTER, 1u, 0u);
    c0 = ko_cagri_boy();
    ko_fc(114u, IDAW_FC_OP_ABORT, 255u, 0u);
    atildi(110u);
    atildi(111u);
    atildi(112u);
    atildi(113u);
    ko_tf(3u);
    ko_pompala();
    mandal_var(ilk_cagri(c0), 3u, 114u);

    /* (iii) SET_MODE(AUTO) + nötr FC_TEST_EXIT → SET_MODE(STABILIZE) */
    ko_simple(120u, IDAW_CMD_TYPE_SET_MODE, IDA_MODE_AUTO);
    ko_fc(121u, IDAW_FC_OP_TEST_EXIT, 0u, 0u);
    c0 = ko_cagri_boy();
    ko_simple(122u, IDAW_CMD_TYPE_SET_MODE, IDA_MODE_STABILIZE);
    atildi(120u);
    sonucsuz(121u);
    ko_tf(4u);
    ko_pompala();
    mandal_var(ilk_cagri(c0), 1u, 122u);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_MODE_STABILIZE, ilk_cagri(c0)->narrow.mode,
                                    "NARROW_MODE hedef kipi (A1 §3.6, A4-K46)");
    TEST_ASSERT_TRUE(ko_cagri_komut_iceriyor(ilk_cagri(c0), 121u));

    /* (iv) ARM → FC_DISARM */
    ko_simple(130u, IDAW_CMD_TYPE_ARM, 0u);
    c0 = ko_cagri_boy();
    ko_fc(131u, IDAW_FC_OP_DISARM, 255u, 0u);
    atildi(130u);
    ko_tf(5u);
    ko_pompala();
    mandal_var(ilk_cagri(c0), 2u, 131u);

    /* Sonda: atılanlar hiçbir çağrıya girmedi, sonuçları tek kaldı (çekirdek sonucu yok). */
    const uint32_t atilan[] = {100u, 101u, 103u, 110u, 111u, 112u, 113u, 120u, 130u};
    for (size_t i = 0; i < sizeof atilan / sizeof atilan[0]; ++i) {
        atildi(atilan[i]);
    }
    bir_kez_girdi(102u);
    bir_kez_girdi(121u);
    bozuk_yok();
}

/* ---- (b) Bekleyen parti (kapanmış, take edilmemiş) ------------------------------------ */
/* A1-K63 (b), A4-K102, A4-K105, A4-K46. */
static void bekleyen_parti(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x1202u);
    ko_simple(200u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple(201u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_fc(202u, IDAW_FC_OP_FIRE, 1u, 0u);
    ko_tf(2u); /* parti 2 kapandı; take yapılmadı: bekleyen */
    size_t c0 = ko_cagri_boy();
    ko_simple(203u, IDAW_CMD_TYPE_SET_MODE, IDA_MODE_DEPTH_HOLD); /* açık parti 3 */
    atildi(200u);
    atildi(202u);
    sonucsuz(201u);
    ko_pompala();
    const ko_cagri_t *c = ilk_cagri(c0);
    TEST_ASSERT_TRUE_MESSAGE(c->tf.present && c->tf.seq == 2u,
                             "bekleyen parti 2 sıradaki çağrı olmalı (A4-K16, A1-K32)");
    TEST_ASSERT_TRUE_MESSAGE(ko_cagri_komut_iceriyor(c, 201u), "nötr komut partisiyle gider");
    mandal_var(c, 1u, 203u);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, c->narrow.mode);
    ko_tf(3u);
    ko_pompala();
    hic_girmedi(200u);
    hic_girmedi(202u);
    bir_kez_girdi(201u);
    bozuk_yok();
}

/* Birleştirilmiş bekleyen girdi: iki kapanmış parti take'ten önce birleşir (A4-K102); ikisindeki
   genişleticiler de atılır. */
static void birlesik_bekleyen(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x120Au);
    ko_simple(210u, IDAW_CMD_TYPE_ARM, 0u);
    ko_tf(2u);
    ko_fc(211u, IDAW_FC_OP_TEST_ENTER, 0u, 0u);
    ko_simple(212u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_tf(3u);
    size_t c0 = ko_cagri_boy();
    ko_fc(213u, IDAW_FC_OP_DISARM, 255u, 0u);
    atildi(210u);
    atildi(211u);
    sonucsuz(212u);
    ko_pompala();
    mandal_var(ilk_cagri(c0), 2u, 213u);
    ko_tf(4u);
    ko_pompala();
    hic_girmedi(210u);
    hic_girmedi(211u);
    bir_kez_girdi(212u);
    bozuk_yok();
}

/* ---- (c) Erteleme tamponu ------------------------------------------------------------- */
/* A1-K63 (c), A4-K105 (8'den fazlası ertelenir), A4-K102 (varış sırası). Üç parti take'ten
   önce kapanır ve birleşir: 4 + 4 + 2 = 10 daraltıcı olmayan komut; ilk 8'i çağrıya girer,
   ARM (9.) ve HEARTBEAT (10.) ertelenir. Çağrı bitince (W3-K10: araya rx girmez) DISARM gelir. */
static void erteleme(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x1203u);
    for (uint32_t i = 0; i < 4u; ++i) {
        ko_simple(300u + i, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    }
    ko_tf(2u);
    for (uint32_t i = 4; i < 8u; ++i) {
        ko_simple(300u + i, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    }
    ko_tf(3u);
    ko_simple(308u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple(309u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_tf(4u);
    long k = ko_tek_cagri();
    TEST_ASSERT_TRUE_MESSAGE(k >= 0, "birleştirilmiş parti için çağrı olmalı");
    const ko_cagri_t *c = ko_cagri((size_t)k);
    /* Önkoşul (A4-K102, K105): varış sırasıyla ilk 8 komut. */
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(8u, c->n_cmds, "önkoşul: çağrıya 8 komut (A4-K105)");
    for (uint32_t i = 0; i < 8u; ++i) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(300u + i, c->cmds[i].cmd_id,
                                         "önkoşul: varış sırasıyla ilk 8 komut (A4-K102)");
    }
    hic_girmedi(308u);
    hic_girmedi(309u);
    size_t c0 = ko_cagri_boy();
    ko_simple(310u, IDAW_CMD_TYPE_DISARM, 0u);
    atildi(308u);
    sonucsuz(309u);
    ko_pompala();
    ko_tf(5u);
    ko_pompala();
    mandal_var(ilk_cagri(c0), 0u, 310u);
    hic_girmedi(308u);
    bir_kez_girdi(309u);
    bozuk_yok();
}

/* ---- (d) Atılan kimliğin yinelemesi --------------------------------------------------- */
/* A1-K63 (nihai sonuç; son 32 kimlik; "bu açılışta"), W3-K11 (daraltıcı dışındaki her
   biçimde yeniden işlenmez, NARROWED yinelenir). */
static void yineleme(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x1204u);
    ko_simple(500u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple(501u, IDAW_CMD_TYPE_DISARM, 0u);
    atildi(500u);
    ko_tf(2u);
    ko_pompala();

    /* Aynı biçimle yineleme (mandal tüketildikten sonra). */
    ko_simple(500u, IDAW_CMD_TYPE_ARM, 0u);
    ko_tf(3u);
    ko_pompala();
    hic_girmedi(500u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, ko_sonuc_say(500u, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED),
                                     "yinelemeye aynı REJECTED/NARROWED yeniden gönderilir (A1-K63)");

    /* Başka (daraltıcı olmayan) biçimle aynı kimlik: yine işlenmez (W3-K11). */
    ko_simple(500u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_tf(4u);
    ko_pompala();
    hic_girmedi(500u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3u, ko_sonuc_say(500u, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED),
                                     "nihai kimlik başka biçimde de yeniden işlenmez (W3-K11)");

    /* Yeni oturumda (aynı açılış) yineleme. */
    ko_session_close(0x1204u, IDAW_CLOSE_REASON_RUN_END);
    ko_pompala();
    (void)ko_el_sikis(UINT32_C(0x52000001), 0x1205u);
    ko_tf(5u);
    ko_pompala();
    ko_simple(500u, IDAW_CMD_TYPE_ARM, 0u);
    ko_tf(6u);
    ko_pompala();
    hic_girmedi(500u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(4u, ko_sonuc_say(500u, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED),
                                     "nihailik açılış boyunca sürer (A1-K63 'bu açılışta')");
    TEST_ASSERT_EQUAL_UINT16(4u, ko_kimlik(500u)->n_sonuc);
    bozuk_yok();
}

/* W3-K11: nihai kimlik biçimce geçerli bir daraltıcı olarak gelirse daraltıcı uygulanır
   (A4-K46, D-20: daraltıcı yineleme denetimlerinden muaftır). */
static void nihai_kimlik_daraltici(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x120Bu);
    ko_simple(520u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple(521u, IDAW_CMD_TYPE_DISARM, 0u);
    atildi(520u);
    ko_tf(2u);
    ko_pompala();
    size_t c0 = ko_cagri_boy();
    ko_fc(520u, IDAW_FC_OP_ABORT, 255u, 0u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, ko_sonuc_say(520u, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED),
                                     "daraltıcı olarak gelen nihai kimliğe NARROWED yinelenmez (W3-K11)");
    ko_tf(3u);
    ko_pompala();
    mandal_var(ilk_cagri(c0), 3u, 520u);
    bozuk_yok();
}

/* A1-K63 "Bu kimlikler last_cmd_id'ye de girer", A1-K66: atılan kimlik en yeniyse sonraki
   BOARD_HELLO'nun last_cmd_id'si odur. */
static void last_cmd_id(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x1206u);
    ko_simple(610u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple(609u, IDAW_CMD_TYPE_DISARM, 0u); /* emülatör sırası; kimlikler benzersiz */
    atildi(610u);
    ko_tf(2u);
    ko_pompala();
    ko_session_close(0x1206u, IDAW_CLOSE_REASON_RUN_END);
    ko_pompala();
    size_t g0 = ko_gunluk_boy();
    ko_sim_hello(UINT32_C(0x53000001));
    ko_pompala();
    mavlink_ida_board_hello_t bh;
    TEST_ASSERT_TRUE_MESSAGE(ko_board_hello_bul(g0, UINT32_C(0x53000001), &bh, NULL),
                             "SIM_HELLO yanıtlanmalı (A1-K13)");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(610u, bh.last_cmd_id,
                                     "nihai NARROWED kimlik last_cmd_id'ye girer (A1-K63, K66)");
    bozuk_yok();
}

/* A1-K63: mandaldan SONRA alınan genişletici mandalla aynı çağrıya düşer (platform atmaz;
   çekirdek A4-K45 ile reddeder). */
static void sonra_gelen_genisletici(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x1207u);
    size_t c0 = ko_cagri_boy();
    ko_simple(140u, IDAW_CMD_TYPE_DISARM, 0u);
    ko_simple(141u, IDAW_CMD_TYPE_ARM, 0u);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, ko_kimlik(141u)->n_sonuc,
                                     "mandaldan sonra gelen genişletici platformca atılmaz");
    ko_tf(2u);
    ko_pompala();
    const ko_cagri_t *c = ilk_cagri(c0);
    mandal_var(c, 0u, 140u);
    TEST_ASSERT_TRUE_MESSAGE(ko_cagri_komut_iceriyor(c, 141u),
                             "mandaldan sonra gelen genişletici mandalla aynı çağrıya girer (A1-K63)");
    bozuk_yok();
}

/* Daraltıcının çekirdeğe ulaşması: dört tür aynı çağrıda; birleştirme (COALESCED).
   A1-K63, A1-K67, A4-K46. */
static void mandal(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x1208u);
    ko_simple(150u, IDAW_CMD_TYPE_DISARM, 0u);
    ko_tf(2u); /* bekleyen; mandal tüketilmedi */
    ko_simple(151u, IDAW_CMD_TYPE_DISARM, 0u);
    const ko_kimlik_t *k = ko_kimlik(150u);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1u, k->n_sonuc, "üzerine yazılan daraltıcıya sonuç (A1-K67)");
    TEST_ASSERT_EQUAL_UINT8(IDAW_ACK_ACCEPTED, k->sonuc[0].ack);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDAW_REASON_COALESCED, k->sonuc[0].reason,
                                     "üzerine yazılan daraltıcı COALESCED (A1-K67)");
    TEST_ASSERT_EQUAL_UINT8(IDA_LINK_SINIF_ZORUNLU, k->sonuc[0].sinif);
    size_t c0 = ko_cagri_boy();
    ko_pompala();
    const ko_cagri_t *c = ilk_cagri(c0);
    mandal_var(c, 0u, 151u);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, c->narrow.coalesced[0], "coalesced sayısı (A4-K46)");
    hic_girmedi(150u);

    ko_tf(3u);
    ko_pompala();
    c0 = ko_cagri_boy();
    ko_simple(160u, IDAW_CMD_TYPE_DISARM, 0u);
    ko_simple(161u, IDAW_CMD_TYPE_SET_MODE, IDA_MODE_STATION_KEEP);
    ko_fc(162u, IDAW_FC_OP_DISARM, 255u, 0u);
    ko_fc(163u, IDAW_FC_OP_ABORT, 255u, 0u);
    ko_tf(4u);
    ko_pompala();
    c = ilk_cagri(c0);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(IDA_NARROW_DEFINED, c->narrow.mask, "dört mandal biti (A4-K46)");
    mandal_var(c, 0u, 160u);
    mandal_var(c, 1u, 161u);
    mandal_var(c, 2u, 162u);
    mandal_var(c, 3u, 163u);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_STATION_KEEP, c->narrow.mode);
    for (uint32_t id = 160u; id <= 163u; ++id) {
        TEST_ASSERT_FALSE_MESSAGE(ko_cagri_komut_iceriyor(c, id), "daraltıcı cmds[]'e konmaz (A4-K46)");
    }
    bozuk_yok();
}

/* A1-K64: alanı bozuk daraltıcı biçimler LINK_BAD_FIELD alır, mandala ve çekirdeğe girmez. */
static void bozuk_alanli(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x1209u);
    size_t c0 = ko_cagri_boy();
    ko_fc(170u, IDAW_FC_OP_DISARM, 0u, 0u);    /* tube 255 olmalı */
    ko_fc(171u, IDAW_FC_OP_ABORT, 255u, 1u);   /* arg0 ≠ 0 */
    ko_simple(172u, IDAW_CMD_TYPE_SET_MODE, 1u); /* hedef 2–7 dışında */
    ko_simple(173u, IDAW_CMD_TYPE_SET_MODE, 8u);
    ko_tf(2u);
    ko_pompala();
    for (uint32_t id = 170u; id <= 173u; ++id) {
        const ko_kimlik_t *k = ko_kimlik(id);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(1u, k->n_sonuc, "dönüşüm hatasına tek sonuç (A1-K64, K67)");
        TEST_ASSERT_EQUAL_UINT8(IDAW_ACK_REJECTED, k->sonuc[0].ack);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDAW_REASON_LINK_BAD_FIELD, k->sonuc[0].reason,
                                         "LINK_BAD_FIELD (A1-K64)");
        TEST_ASSERT_EQUAL_UINT8(IDA_LINK_SINIF_ZORUNLU, k->sonuc[0].sinif);
        hic_girmedi(id);
    }
    for (size_t i = c0; i < ko_cagri_boy(); ++i) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, ko_cagri(i)->narrow.mask,
                                       "bozuk daraltıcı mandala alınmaz (A1-K64)");
    }
    bozuk_yok();
}

/* W1 fark incelemesi bulgu 1 / D-17 (yalnız kart rolü; ızgara tick'i): TF 0 ms, ARM 1 ms
   açık partide, DISARM 3 ms mandalda, ızgara tick'i (A1-K117: E + T_tf_gec = 6 ms), TF 7 ms.
   ARM hiçbir çağrıya girmez. */
static void izgara_d17(void)
{
    ko_hazir(IDA_LINK_UST_ROL_KART, 0x120Cu);
    ko_tf(2u);
    ko_pompala();
    ko_bekle_ms(1u);
    ko_simple(180u, IDAW_CMD_TYPE_ARM, 0u);
    ko_bekle_ms(2u);
    size_t c0 = ko_cagri_boy();
    ko_simple(181u, IDAW_CMD_TYPE_DISARM, 0u);
    atildi(180u);
    ko_bekle_ms(4u);
    bool izgara = false;
    for (size_t i = c0; i < ko_cagri_boy(); ++i) {
        izgara = izgara || ko_cagri(i)->tur == IDA_LINK_CAGRI_IZGARA;
    }
    ko_bilgi("D-17 senaryosu: TF(3)'ten önce ızgara tick'i %s", izgara ? "oldu" : "olmadı");
    ko_tf(3u);
    ko_pompala();
    hic_girmedi(180u);
    TEST_ASSERT_TRUE_MESSAGE(ko_komutu_tasiyan_cagri(c0, 181u) >= 0, "DISARM çekirdeğe ulaşmalı");
    TEST_ASSERT_EQUAL_UINT32(1u, ko_sonuc_say(180u, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED));
    bozuk_yok();
}

/* W3-K10 ihlal vakası: ARM → TF → take → rx(DISARM) → put_out. Modül baytları işler ama oturumu
   PROTOCOL nedeniyle bitirir; sonraki çağrı oturum sonu çağrısıdır (A1-K107; daraltıcıdır,
   A4-K73), yani ARM'ın etkisi kalmaz. */
static void pencere_ihlali(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x120Du);
    ko_simple(190u, IDAW_CMD_TYPE_ARM, 0u);
    ko_tf(2u);
    long k = ko_tut();
    TEST_ASSERT_TRUE(k >= 0);
    TEST_ASSERT_TRUE_MESSAGE(ko_cagri_komut_iceriyor(ko_cagri((size_t)k), 190u),
                             "önkoşul: ARM take edilen çağrıda");
    size_t g0 = ko_gunluk_boy();
    size_t c0 = ko_cagri_boy();
    ko_pencere_ihlali_izni(true);
    ko_simple(191u, IDAW_CMD_TYPE_DISARM, 0u);
    ko_pencere_ihlali_izni(false);
    ko_birak(true);
    /* Sonraki çağrı oturum sonu çağrısı. */
    const ko_cagri_t *c = ilk_cagri(c0);
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)IDA_LINK_CAGRI_OTURUM_SONU, (int)c->tur,
                                  "pencere ihlalinden sonraki çağrı oturum sonu çağrısı (W3-K10)");
    TEST_ASSERT_EQUAL_UINT8(IDA_LINKEV_SESSION_END, c->link_event);
    TEST_ASSERT_FALSE(c->tf.present);
    TEST_ASSERT_EQUAL_UINT8(0u, c->n_cmds);
    TEST_ASSERT_NOT_EQUAL_INT((int)IDA_LINK_UST_RUNNING, (int)ida_link_ust_durum(ko_l()));
    /* Oturum PROTOCOL nedeniyle bitti: IDA_SESSION_CLOSE(PROTOCOL) ve ihlalli çağrının sürekli
       çıktısı gönderilmez (ust.h: çağrı sürerken oturum bittiyse çıktı gönderilmez). */
    bool kapanis = false;
    for (size_t i = g0; i < ko_gunluk_boy(); ++i) {
        const ko_cerceve_t *f = ko_gunluk(i);
        TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(MAVLINK_MSG_ID_IDA_ACTUATOR, f->msgid,
                                             "oturumu biten çağrının IDA_ACTUATOR'ı gönderilmez");
        if (f->msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE) {
            mavlink_ida_session_close_t m;
            ko_coz(f, MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &m, sizeof m);
            TEST_ASSERT_EQUAL_UINT16(0x120Du, m.sess);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDAW_CLOSE_REASON_PROTOCOL, m.reason,
                                            "kapanış nedeni PROTOCOL (W3-K10)");
            kapanis = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(kapanis, "IDA_SESSION_CLOSE(PROTOCOL) gönderilmeli (W3-K10, A1-K22)");
    /* Oturum sonu çağrısından sonra ARM bir daha hiçbir çağrıya girmez. */
    TEST_ASSERT_EQUAL_UINT16(1u, ko_kimlik(190u)->girdi);
    bozuk_yok();
}

#define IKI_ROL(ad)                                                                          \
    static void test_##ad##_kart(void) { ad(IDA_LINK_UST_ROL_KART); }                         \
    static void test_##ad##_sil(void) { ad(IDA_LINK_UST_ROL_SIL); }

IKI_ROL(acik_parti)
IKI_ROL(bekleyen_parti)
IKI_ROL(birlesik_bekleyen)
IKI_ROL(erteleme)
IKI_ROL(yineleme)
IKI_ROL(nihai_kimlik_daraltici)
IKI_ROL(last_cmd_id)
IKI_ROL(sonra_gelen_genisletici)
IKI_ROL(mandal)
IKI_ROL(bozuk_alanli)
IKI_ROL(pencere_ihlali)

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_acik_parti_kart);
    RUN_TEST(test_acik_parti_sil);
    RUN_TEST(test_bekleyen_parti_kart);
    RUN_TEST(test_bekleyen_parti_sil);
    RUN_TEST(test_birlesik_bekleyen_kart);
    RUN_TEST(test_birlesik_bekleyen_sil);
    RUN_TEST(test_erteleme_kart);
    RUN_TEST(test_erteleme_sil);
    RUN_TEST(test_yineleme_kart);
    RUN_TEST(test_yineleme_sil);
    RUN_TEST(test_nihai_kimlik_daraltici_kart);
    RUN_TEST(test_nihai_kimlik_daraltici_sil);
    RUN_TEST(test_last_cmd_id_kart);
    RUN_TEST(test_last_cmd_id_sil);
    RUN_TEST(test_sonra_gelen_genisletici_kart);
    RUN_TEST(test_sonra_gelen_genisletici_sil);
    RUN_TEST(test_mandal_kart);
    RUN_TEST(test_mandal_sil);
    RUN_TEST(test_bozuk_alanli_kart);
    RUN_TEST(test_bozuk_alanli_sil);
    RUN_TEST(izgara_d17);
    RUN_TEST(test_pencere_ihlali_kart);
    RUN_TEST(test_pencere_ihlali_sil);
    return UNITY_END();
}
