/*
 * Rastgele komut dizileri (sabit tohum) — komut sırası ve oturum değişimi değişmezleri.
 *
 * Dayanılan kurallar: A1-K63 / A4-K105 (D-17), A1-K25, A1-K22 (1), A4-K74, A4-K46, A4-K16,
 * A1-K67, W3-K10 (take ile put_out arasında rx yok: düzenek her take'i hemen put_out ile
 * kapatır), W3-K11.
 *
 * Her adımdan sonra denetlenen değişmez: bir daraltıcı rx edildiği anda, aynı oturumda
 * kendisinden önce alınmış ve hiçbir çağrıya girmemiş her genişletici "yasak" olur; yasak
 * kimlik o andan sonra hiçbir çağrı girdisinde (cmds[] ya da mandal yuvası) görünmez ve
 * REJECTED/NARROWED alır. Ayrıca: eski oturumun çağrıya girmemiş komutları ve eski sess'le
 * gelen komutlar yasaktır (A4-K74, A1-K25); atılan kimliğin aynı biçimdeki yinelemesi yasak
 * kalır ve NARROWED yinelenir (A1-K63); hiçbir komut iki çağrıya girmez (A4-K16); her geçerli
 * daraltıcı ya bir çağrının mandalında görünür ya da COALESCED alır (A4-K46, A1-K67); son
 * oturumun atılmamış komutları çağrıya girer ya da LINK_DROPPED alır (A4-K105).
 *
 * Sim'in gönderim kuralları korunur: partide en çok 4 daraltıcı olmayan komut ve her daraltıcı
 * türden 1 (A1-K34, K61); komut kimlikleri benzersiz (A1-K65); kimlik yinelemesi yalnız atılan
 * kimliklerin son 32'si içinden (A1-K63).
 */
#include <stdio.h>
#include <string.h>

#include "kabul_oturum.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

enum { T_YOK = 0, T_GENIS = 1, T_NOTR = 2, T_DAR = 3 };

typedef struct {
    uint8_t  tur;
    uint8_t  bicim;      /* gönderim biçimi (yinelemede aynısı)                         */
    uint8_t  tube;
    uint16_t sess;
    bool     alindi;
    bool     yasak;
    bool     eski_sess;  /* eski sess'le gönderildi                                     */
    uint16_t yinelendi;  /* atıldıktan sonraki yineleme sayısı                          */
} meta_t;

static meta_t mt[KO_KIMLIK_AZAMI];
static uint32_t atilan[4096];
static size_t n_atilan;

#define ILK_KIMLIK 1000u
#define ADIM 900u

static uint32_t sonraki_id;
static uint16_t sess;
static uint16_t eski_sessler[16];
static size_t n_eski;
static uint32_t seq;
static uint32_t parti_notr;
static uint32_t parti_dar;   /* bit: kullanılan daraltıcı türü */
static uint32_t kapali_bekleyen;
static size_t denetlenen_cagri;
static uint32_t oturum_ilk_id;
static uint32_t nonce_sayac;

/* Genişletici biçimleri (A4-K104): 0 ARM, 1 SET_MODE GUIDED, 2 SET_MODE AUTO, 3 FAILSAFE_RESET,
   4 FC_ARM, 5 FC_FIRE, 6 FC_RESET, 7 FC_TEST_ENTER. Nötr: 8 HEARTBEAT, 9 FC_TEST_EXIT.
   Daraltıcı: 10 DISARM, 11 SET_MODE 2–5 (tube alanında kip), 12 FC_DISARM, 13 FC_ABORT. */
static void bicimle_gonder(uint16_t s, uint32_t id, uint8_t bicim, uint8_t tube)
{
    switch (bicim) {
    case 0: ko_simple_s(s, id, IDAW_CMD_TYPE_ARM, 0u); break;
    case 1: ko_simple_s(s, id, IDAW_CMD_TYPE_SET_MODE, IDA_MODE_GUIDED); break;
    case 2: ko_simple_s(s, id, IDAW_CMD_TYPE_SET_MODE, IDA_MODE_AUTO); break;
    case 3: ko_simple_s(s, id, IDAW_CMD_TYPE_FAILSAFE_RESET, 0u); break;
    case 4: ko_fc_s(s, id, IDAW_FC_OP_ARM, tube, 0u); break;
    case 5: ko_fc_s(s, id, IDAW_FC_OP_FIRE, tube, 0u); break;
    case 6: ko_fc_s(s, id, IDAW_FC_OP_RESET, tube, 0u); break;
    case 7: ko_fc_s(s, id, IDAW_FC_OP_TEST_ENTER, tube, 0u); break;
    case 8: ko_simple_s(s, id, IDAW_CMD_TYPE_HEARTBEAT, 0u); break;
    case 9: ko_fc_s(s, id, IDAW_FC_OP_TEST_EXIT, tube, 0u); break;
    case 10: ko_simple_s(s, id, IDAW_CMD_TYPE_DISARM, 0u); break;
    case 11: ko_simple_s(s, id, IDAW_CMD_TYPE_SET_MODE, tube); break;
    case 12: ko_fc_s(s, id, IDAW_FC_OP_DISARM, 255u, 0u); break;
    default: ko_fc_s(s, id, IDAW_FC_OP_ABORT, 255u, 0u); break;
    }
}

/* Yeni çağrıların girdileri: yasak kimlik yok; her kimlik en çok bir çağrıya girer. */
static void cagrilari_denetle(uint32_t tohum, uint32_t adim)
{
    for (size_t i = denetlenen_cagri; i < ko_cagri_boy(); ++i) {
        const ko_cagri_t *c = ko_cagri(i);
        TEST_ASSERT_TRUE_MESSAGE(c->n_cmds <= IDA_PORT_MAX_CMDS, "n_cmds ≤ 8 (A4-K105)");
        for (uint8_t j = 0; j < c->n_cmds; ++j) {
            uint32_t id = c->cmds[j].cmd_id;
            if (id < KO_KIMLIK_AZAMI && mt[id].yasak) {
                char ileti[200];
                (void)snprintf(ileti, sizeof ileti,
                               "tohum %u adım %u: yasak kimlik %u çağrı %u girdisinde "
                               "(A1-K63 / A4-K74 / A1-K25)",
                               (unsigned)tohum, (unsigned)adim, (unsigned)id, (unsigned)i);
                TEST_FAIL_MESSAGE(ileti);
            }
        }
        for (uint32_t b = 0; b < IDA_PORT_N_NARROW; ++b) {
            if ((c->narrow.mask & (1u << b)) != 0u) {
                uint32_t id = c->narrow.cmd_id[b];
                if (id < KO_KIMLIK_AZAMI && mt[id].tur != T_DAR && mt[id].yasak) {
                    TEST_FAIL_MESSAGE("yasak kimlik mandal yuvasında");
                }
            }
        }
    }
    denetlenen_cagri = ko_cagri_boy();
    for (uint32_t id = ILK_KIMLIK; id < sonraki_id; ++id) {
        if (ko_kimlik(id)->girdi > 1u) {
            char ileti[160];
            (void)snprintf(ileti, sizeof ileti, "tohum %u: kimlik %u birden çok çağrıya girdi (A4-K16)",
                           (unsigned)tohum, (unsigned)id);
            TEST_FAIL_MESSAGE(ileti);
        }
    }
}

static void pompala(void)
{
    ko_pompala();
    kapali_bekleyen = 0u;
}

static void tf_gonder(void)
{
    if (kapali_bekleyen >= 3u) {
        pompala(); /* en çok üç parti birleşsin (erteleme tamponu taşmasın) */
    }
    ko_tf_s(sess, seq);
    seq++;
    parti_notr = 0u;
    parti_dar = 0u;
    kapali_bekleyen++;
}

static void yeni_komut(ko_rng_t *r, uint8_t tur)
{
    uint32_t id = sonraki_id++;
    TEST_ASSERT_TRUE(id < KO_KIMLIK_AZAMI);
    meta_t *m = &mt[id];
    memset(m, 0, sizeof *m);
    m->tur = tur;
    m->sess = sess;
    m->tube = (uint8_t)ko_rng_aralik(r, 2u);
    if (tur == T_GENIS) {
        m->bicim = (uint8_t)ko_rng_aralik(r, 8u);
    } else if (tur == T_NOTR) {
        m->bicim = (uint8_t)(8u + ko_rng_aralik(r, 2u));
    } else {
        uint32_t t;
        do {
            t = ko_rng_aralik(r, 4u);
        } while ((parti_dar & (1u << t)) != 0u);
        parti_dar |= 1u << t;
        m->bicim = (uint8_t)(10u + t);
        if (m->bicim == 11u) {
            m->tube = (uint8_t)(2u + ko_rng_aralik(r, 4u)); /* daraltıcı kip 2–5 */
        }
    }

    /* Daraltıcıdan önce: atılması gereken genişleticiler (A1-K63). */
    uint32_t aday[256];
    size_t n_aday = 0;
    if (tur == T_DAR) {
        for (uint32_t k = oturum_ilk_id; k < id; ++k) {
            const meta_t *a = &mt[k];
            if (a->tur == T_GENIS && a->sess == sess && a->alindi && !a->yasak && !a->eski_sess &&
                ko_kimlik(k)->girdi == 0u && ko_kimlik(k)->n_sonuc == 0u && n_aday < 256u) {
                aday[n_aday++] = k;
            }
        }
    }
    bicimle_gonder(sess, id, m->bicim, m->tube);
    m->alindi = true;
    if (tur != T_DAR) {
        parti_notr++;
    }
    for (size_t i = 0; i < n_aday; ++i) {
        uint32_t k = aday[i];
        char ileti[200];
        (void)snprintf(ileti, sizeof ileti,
                       "kimlik %u: daraltıcı %u alınınca REJECTED/NARROWED almalı (A1-K63)",
                       (unsigned)k, (unsigned)id);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, ko_sonuc_say(k, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED),
                                         ileti);
        mt[k].yasak = true;
        if (n_atilan < sizeof atilan / sizeof atilan[0]) {
            atilan[n_atilan++] = k;
        }
    }
    if (tur == T_DAR) {
        /* Nötr bekleyenler etkilenmez. */
        for (uint32_t k = oturum_ilk_id; k < id; ++k) {
            if (mt[k].tur == T_NOTR && mt[k].sess == sess && !mt[k].eski_sess) {
                TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                    ko_sonuc_say(k, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED),
                    "nötr komut daraltıcıdan etkilenmez (A1-K63)");
            }
        }
    }
}

static void oturum_degistir(ko_rng_t *r)
{
    const uint16_t eski = sess;
    bool hello = ko_rng_aralik(r, 2u) == 0u;
    nonce_sayac++;
    const uint32_t nonce = UINT32_C(0x60000000) + nonce_sayac;
    if (hello) {
        ko_sim_hello(nonce); /* yeni nonce: oturum biter (A1-K13) */
    } else {
        ko_session_close(eski, IDAW_CLOSE_REASON_RUN_END);
    }
    /* Oturum bitti: eski oturumun çağrıya girmemiş daraltıcı olmayan komutları yasak
       (A4-K74, A1-K22 (1)); mandal silinmez. */
    for (uint32_t k = oturum_ilk_id; k < sonraki_id; ++k) {
        if (mt[k].sess == eski && mt[k].tur != T_DAR && ko_kimlik(k)->girdi == 0u) {
            mt[k].yasak = true;
        }
    }
    if (n_eski < sizeof eski_sessler / sizeof eski_sessler[0]) {
        eski_sessler[n_eski++] = eski;
    }
    pompala();
    sess = (uint16_t)(sess + 0x101u);
    (void)ko_el_sikis(nonce, sess);
    oturum_ilk_id = sonraki_id;
    parti_notr = 0u;
    parti_dar = 0u;
}

static void kos(ida_link_ust_rol_t rol, uint32_t tohum)
{
    memset(mt, 0, sizeof mt);
    n_atilan = 0u;
    sonraki_id = ILK_KIMLIK;
    sess = 0x4001u;
    n_eski = 0u;
    parti_notr = 0u;
    parti_dar = 0u;
    kapali_bekleyen = 0u;
    nonce_sayac = 0u;
    ko_hazir(rol, sess);
    seq = 2u;
    denetlenen_cagri = ko_cagri_boy();
    oturum_ilk_id = sonraki_id;

    ko_rng_t r;
    ko_rng_kur(&r, UINT64_C(0xA1063000) + tohum);
    for (uint32_t adim = 0; adim < ADIM; ++adim) {
        uint32_t w = ko_rng_aralik(&r, 100u);
        if (w < 24u) {
            if (parti_notr < 4u) {
                yeni_komut(&r, T_GENIS);
            }
        } else if (w < 36u) {
            if (parti_notr < 4u) {
                yeni_komut(&r, T_NOTR);
            }
        } else if (w < 46u) {
            if (parti_dar != 0x0Fu) {
                yeni_komut(&r, T_DAR);
            }
        } else if (w < 68u) {
            tf_gonder();
        } else if (w < 76u) {
            pompala();
        } else if (w < 81u) {
            (void)ko_tek_cagri(); /* tek take + put_out; ertelenenler tamponda kalabilir */
            kapali_bekleyen = 0u;
        } else if (w < 87u) {
            if (rol == IDA_LINK_UST_ROL_KART) {
                ko_bekle_ms(1u + ko_rng_aralik(&r, 3u));
                kapali_bekleyen = 0u;
            } else {
                pompala();
            }
        } else if (w < 93u) {
            if (n_atilan > 0u && parti_notr < 4u) {
                size_t pencere = n_atilan < 32u ? n_atilan : 32u;
                uint32_t k = atilan[n_atilan - 1u - ko_rng_aralik(&r, (uint32_t)pencere)];
                bicimle_gonder(sess, k, mt[k].bicim, mt[k].tube);
                mt[k].yinelendi++;
                parti_notr++;
            }
        } else if (w < 95u) {
            if (n_eski > 0u) {
                uint32_t id = sonraki_id++;
                meta_t *m = &mt[id];
                memset(m, 0, sizeof *m);
                m->tur = (uint8_t)(ko_rng_aralik(&r, 2u) == 0u ? T_GENIS : T_NOTR);
                m->bicim = (uint8_t)(m->tur == T_GENIS ? 0u : 8u);
                m->eski_sess = true;
                m->yasak = true; /* A1-K25 */
                m->sess = eski_sessler[ko_rng_aralik(&r, (uint32_t)n_eski)];
                bicimle_gonder(m->sess, id, m->bicim, 0u);
                m->alindi = true;
            }
        } else if (w < 97u) {
            oturum_degistir(&r);
        } else {
            pompala();
        }
        cagrilari_denetle(tohum, adim);
    }
    /* Boşalt. */
    pompala();
    tf_gonder();
    pompala();
    tf_gonder();
    pompala();
    cagrilari_denetle(tohum, ADIM);

    uint32_t n_dar = 0, n_atil = 0, n_yin = 0;
    for (uint32_t id = ILK_KIMLIK; id < sonraki_id; ++id) {
        const meta_t *m = &mt[id];
        const ko_kimlik_t *k = ko_kimlik(id);
        char ileti[200];
        if (m->yasak) {
            (void)snprintf(ileti, sizeof ileti, "tohum %u: yasak kimlik %u çağrıya girdi",
                           (unsigned)tohum, (unsigned)id);
            TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, k->girdi, ileti);
        }
        if (m->tur == T_DAR && !m->eski_sess) {
            n_dar++;
            bool coalesced = ko_sonuc_say(id, IDAW_ACK_ACCEPTED, IDAW_REASON_COALESCED) > 0u;
            (void)snprintf(ileti, sizeof ileti,
                           "tohum %u: daraltıcı %u çekirdeğe ulaşmadı ve birleştirilmedi (A4-K46)",
                           (unsigned)tohum, (unsigned)id);
            TEST_ASSERT_TRUE_MESSAGE(k->girdi >= 1u || coalesced, ileti);
        }
        if (m->tur == T_GENIS && m->yasak && !m->eski_sess &&
            ko_sonuc_say(id, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED) > 0u) {
            n_atil++;
            n_yin += m->yinelendi;
            (void)snprintf(ileti, sizeof ileti,
                           "tohum %u: atılan %u: NARROWED sayısı 1 + yineleme sayısı olmalı (A1-K63)",
                           (unsigned)tohum, (unsigned)id);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u + m->yinelendi,
                                             ko_sonuc_say(id, IDAW_ACK_REJECTED, IDAW_REASON_NARROWED),
                                             ileti);
        }
        if (m->tur != T_DAR && !m->yasak && !m->eski_sess && m->sess == sess) {
            bool dusen = ko_sonuc_say(id, IDAW_ACK_REJECTED, IDAW_REASON_LINK_DROPPED) > 0u;
            (void)snprintf(ileti, sizeof ileti,
                           "tohum %u: son oturumun komutu %u çağrıya girmedi ve düşmedi (A4-K105)",
                           (unsigned)tohum, (unsigned)id);
            TEST_ASSERT_TRUE_MESSAGE(k->girdi == 1u || dusen, ileti);
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, ko_bozuk_cerceve(), "kartın çözülemeyen çerçevesi");
    ko_bilgi("rol %d tohum %u: %u kimlik, %u daraltıcı, %u atılan genişletici, %u yineleme, "
             "%u oturum değişimi, %u çağrı",
             (int)rol, (unsigned)tohum, (unsigned)(sonraki_id - ILK_KIMLIK), (unsigned)n_dar,
             (unsigned)n_atil, (unsigned)n_yin, (unsigned)n_eski, (unsigned)ko_cagri_boy());
}

static void test_kart(void)
{
    for (uint32_t t = 1u; t <= 6u; ++t) {
        kos(IDA_LINK_UST_ROL_KART, t);
    }
}

static void test_sil(void)
{
    for (uint32_t t = 1u; t <= 6u; ++t) {
        kos(IDA_LINK_UST_ROL_SIL, t);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kart);
    RUN_TEST(test_sil);
    return UNITY_END();
}
