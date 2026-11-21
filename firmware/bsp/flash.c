/*
 * flash.c — kalıcı bellek bölgesinin flash sürücüsü (01 §2.9; W3-kart dilim 2).
 *
 * Yazmaç düzeyinde, bloklamayan: her işlem (sayfa silme, 16 B quad-word yazımı) başlatılır ve
 * bitişi ana döngüde durum() ile yoklanır. Bölge Bank 2'nin son dört 8 KB sayfasıdır
 * (0x083F8000, Bank 2 sayfa 252–255); kod Bank 1'dedir, işlem sürerken işlemci Bank 1'den
 * okumayı sürdürür (okurken yazma; DS13543). İşlem sürerken bölge okunmaz (app/kalici.c
 * yalnız işlem bitince okur). Her işlem bitince ICACHE geçersiz kılınır (önbellekte eski içerik
 * kalmasın). İşlem süreleri DWT ile ölçülür (ölçüm bloğu).
 *
 * ECC (bsp/ecc.h; sol bulgu 2 ve 6): açılışta bekleyen ECCC/ECCD kaydedilip temizlenir ve
 * düzeltme kesmesi (ECCIE, FLASH_IRQn) açılır; her tek hata hemen kaydedilip temizlenir. Kalıcı
 * bölgedeki çift hata NMI'de yutulur: hatalı quad-word işaretlenir (okuyucu kaydı reddeder,
 * ida_flash_ecc_bozuk), ICACHE geçersiz kılınır (hatalı satır önbellekte kalmasın). Başka
 * yerdeki çift hata ölümcüldür.
 */
#include "flash.h"

#include "bsp.h"
#include "ecc.h"
#include "stm32u5xx_hal.h"

_Static_assert(IDA_ECC_ECCD == FLASH_ECCR_ECCD, "ECCD");
_Static_assert(IDA_ECC_ECCC == FLASH_ECCR_ECCC, "ECCC");
_Static_assert(IDA_ECC_ECCIE == FLASH_ECCR_ECCIE, "ECCIE");
_Static_assert(IDA_ECC_SYSF == FLASH_ECCR_SYSF_ECC, "SYSF_ECC");
_Static_assert(IDA_ECC_BK == FLASH_ECCR_BK_ECC, "BK_ECC");
_Static_assert(IDA_ECC_ADDR_MASK == FLASH_ECCR_ADDR_ECC_Msk, "ADDR_ECC");
_Static_assert(IDA_ECC_NVM_OFS == 0x083F8000u - 0x08200000u, "kalıcı bölge Bank 2 ofseti");

#define NVM_TABAN 0x083F8000u
#define NVM_ILK_SAYFA 252u /* Bank 2 içindeki sayfa numarası */
#define HATA_BAYRAK                                                                               \
    (FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR | FLASH_NSSR_PGAERR |               \
     FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR)

static uint8_t islem;          /* 0 yok, 1 silme, 2 yazma */
static uint32_t islem_sayfa;   /* silinen sayfa (0..3) */
static ida_ecc_t ecc;
static uint32_t islem_bas;     /* DWT */
static int son_sonuc;
ida_flash_olcum_t ida_flash_olcum;

const uint8_t *ida_flash_taban(void)
{
    return (const uint8_t *)NVM_TABAN;
}

static void kilit_ac(void)
{
    if ((FLASH->NSCR & FLASH_NSCR_LOCK) != 0u) {
        FLASH->NSKEYR = 0x45670123u;
        FLASH->NSKEYR = 0xCDEF89ABu;
    }
}

static int hazir(void)
{
    if ((FLASH->NSSR & (FLASH_NSSR_BSY | FLASH_NSSR_WDW)) != 0u || islem != 0u) {
        return 0;
    }
    FLASH->NSSR = HATA_BAYRAK | FLASH_NSSR_EOP; /* eski bayraklar */
    return 1;
}

int ida_flash_sil(uint32_t sayfa)
{
    if (sayfa >= 4u || !hazir()) {
        return -1;
    }
    kilit_ac();
    const uint32_t pnb = NVM_ILK_SAYFA + sayfa;
    FLASH->NSCR = (FLASH->NSCR & ~(FLASH_NSCR_PNB | FLASH_NSCR_PER | FLASH_NSCR_PG)) |
                  FLASH_NSCR_BKER | (pnb << FLASH_NSCR_PNB_Pos) | FLASH_NSCR_PER;
    islem = 1u;
    islem_sayfa = sayfa;
    islem_bas = ida_bsp_cevrim();
    FLASH->NSCR |= FLASH_NSCR_STRT;
    return 0;
}

int ida_flash_yaz16(uint32_t ofs, const uint8_t q[16])
{
    if ((ofs & 15u) != 0u || ofs >= 4u * 8192u || !hazir()) {
        return -1;
    }
    kilit_ac();
    FLASH->NSCR = (FLASH->NSCR & ~(FLASH_NSCR_PER | FLASH_NSCR_BKER)) | FLASH_NSCR_PG;
    volatile uint32_t *hedef = (volatile uint32_t *)(NVM_TABAN + ofs);
    uint32_t w[4];
    for (uint32_t i = 0u; i < 4u; i++) {
        w[i] = (uint32_t)q[4u * i] | ((uint32_t)q[4u * i + 1u] << 8) |
               ((uint32_t)q[4u * i + 2u] << 16) | ((uint32_t)q[4u * i + 3u] << 24);
    }
    islem = 2u;
    islem_bas = ida_bsp_cevrim();
    /* Quad-word kesintisiz yazılır (HAL FLASH_Program_QuadWord ile aynı). */
    const uint32_t m = __get_PRIMASK();
    __disable_irq();
    for (uint32_t i = 0u; i < 4u; i++) {
        hedef[i] = w[i];
    }
    __set_PRIMASK(m);
    return 0;
}

int ida_flash_durum(void)
{
    if (islem == 0u) {
        return son_sonuc;
    }
    const uint32_t sr = FLASH->NSSR;
    if ((sr & (FLASH_NSSR_BSY | FLASH_NSSR_WDW)) != 0u) {
        return 1;
    }
    const uint32_t sure = ida_bsp_cevrim() - islem_bas;
    if (islem == 1u) {
        ida_ecc_sayfa_sil(&ecc, islem_sayfa); /* silinen sayfanın ECC işaretleri kalkar */
        ida_flash_olcum.silme_sayisi++;
        if (sure > ida_flash_olcum.silme_en_kotu_cevrim) {
            ida_flash_olcum.silme_en_kotu_cevrim = sure;
        }
    } else {
        ida_flash_olcum.yazma_sayisi++;
        if (sure > ida_flash_olcum.yazma_en_kotu_cevrim) {
            ida_flash_olcum.yazma_en_kotu_cevrim = sure;
        }
    }
    FLASH->NSCR &= ~(FLASH_NSCR_PER | FLASH_NSCR_PG | FLASH_NSCR_PNB | FLASH_NSCR_BKER);
    FLASH->NSCR |= FLASH_NSCR_LOCK;
    islem = 0u;
    if ((sr & HATA_BAYRAK) != 0u) {
        FLASH->NSSR = sr & HATA_BAYRAK;
        ida_flash_olcum.hata++;
        son_sonuc = -1;
    } else {
        FLASH->NSSR = FLASH_NSSR_EOP;
        son_sonuc = 0;
    }
    /* Önbellekte bölgenin eski içeriği kalmasın. */
    ICACHE->CR |= ICACHE_CR_CACHEINV;
    for (uint32_t i = 0u; i < 100000u && (ICACHE->SR & ICACHE_SR_BUSYF) != 0u; i++) {
    }
    return son_sonuc;
}

static void ecc_kaydet(void)
{
    ida_flash_olcum.ecc_nmi = ecc.eccd_nvm;
    ida_flash_olcum.eccc_nvm = ecc.eccc_nvm;
    ida_flash_olcum.eccc_diger = ecc.eccc_diger;
    ida_flash_olcum.eccd_diger = ecc.eccd_diger;
    ida_flash_olcum.ecc_son = ecc.son_eccd != 0u ? ecc.son_eccd : ecc.son_eccc;
}

void ida_flash_ecc_baslat(void)
{
    ida_ecc_kur(&ecc);
    const uint32_t e = FLASH->ECCR;
    if ((e & (FLASH_ECCR_ECCC | FLASH_ECCR_ECCD)) != 0u) {
        FLASH->ECCR = ida_ecc_isle(&ecc, e, NULL, NULL); /* açılıştan önce kalanlar */
    } else {
        FLASH->ECCR = FLASH_ECCR_ECCIE;
    }
    ecc_kaydet();
    ida_bsp_kesme_ac((int32_t)FLASH_IRQn, IDA_BSP_ONCELIK_FLASH);
}

/* Düzeltme kesmesi (ECCC): kaydedilir ve hemen temizlenir. */
void FLASH_IRQHandler(void);
void FLASH_IRQHandler(void)
{
    const uint32_t e = FLASH->ECCR;
    if ((e & (FLASH_ECCR_ECCC | FLASH_ECCR_ECCD)) != 0u) {
        FLASH->ECCR = ida_ecc_isle(&ecc, e, NULL, NULL);
        ecc_kaydet();
    }
}

bool ida_flash_ecc_nmi(void)
{
    const uint32_t e = FLASH->ECCR;
    if ((e & FLASH_ECCR_ECCD) == 0u) {
        return false;
    }
    bool cift = false;
    bool bizim = false;
    FLASH->ECCR = ida_ecc_isle(&ecc, e, &cift, &bizim);
    ecc_kaydet();
    if (bizim) {
        /* Hatalı satır önbellekten atılır (AN5342); bekleme sınırlı. */
        ICACHE->CR |= ICACHE_CR_CACHEINV;
        for (uint32_t i = 0u; i < 100000u && (ICACHE->SR & ICACHE_SR_BUSYF) != 0u; i++) {
        }
    }
    return bizim;
}

bool ida_flash_ecc_bozuk(uint32_t ofs, uint32_t n)
{
    return ida_ecc_bozuk(&ecc, ofs, n);
}
