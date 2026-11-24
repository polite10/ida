/* ecc.c — flash ECC olaylarının işlenmesi (ecc.h). */
#include "ecc.h"

#include <string.h>

void ida_ecc_kur(ida_ecc_t *e)
{
    memset(e, 0, sizeof *e);
}

/* Hatalı adres kalıcı bölgede mi; öyleyse bölge içi quad-word dizini. */
static bool nvm_qw(uint32_t eccr, uint32_t *qw)
{
    if ((eccr & IDA_ECC_SYSF) != 0u || (eccr & IDA_ECC_BK) == 0u) {
        return false;
    }
    const uint32_t a = eccr & IDA_ECC_ADDR_MASK;
    if (a < IDA_ECC_NVM_OFS || a - IDA_ECC_NVM_OFS >= IDA_ECC_NVM_BAYT) {
        return false;
    }
    *qw = (a - IDA_ECC_NVM_OFS) / IDA_ECC_QW;
    return true;
}

uint32_t ida_ecc_isle(ida_ecc_t *e, uint32_t eccr, bool *ecc_cift, bool *nvm)
{
    uint32_t temizle = IDA_ECC_ECCIE;
    bool cift = false;
    bool bizim = false;
    uint32_t qw = 0u;
    const bool nvm_adres = nvm_qw(eccr, &qw);
    if ((eccr & IDA_ECC_ECCC) != 0u) {
        if (nvm_adres) {
            e->eccc_nvm++;
        } else {
            e->eccc_diger++;
        }
        e->son_eccc = eccr;
        temizle |= IDA_ECC_ECCC;
    }
    if ((eccr & IDA_ECC_ECCD) != 0u) {
        cift = true;
        e->son_eccd = eccr;
        if (nvm_adres && qw < IDA_ECC_QW_N) {
            bizim = true;
            e->eccd_nvm++;
            e->bozuk[qw / 8u] = (uint8_t)(e->bozuk[qw / 8u] | (1u << (qw % 8u)));
        } else {
            e->eccd_diger++;
        }
        temizle |= IDA_ECC_ECCD;
    }
    if (ecc_cift != NULL) {
        *ecc_cift = cift;
    }
    if (nvm != NULL) {
        *nvm = bizim;
    }
    return temizle;
}

bool ida_ecc_bozuk(const ida_ecc_t *e, uint32_t ofs, uint32_t n)
{
    if (n == 0u || ofs >= IDA_ECC_NVM_BAYT || n > IDA_ECC_NVM_BAYT - ofs) {
        return true;
    }
    const uint32_t ilk = ofs / IDA_ECC_QW;
    const uint32_t son = (ofs + n - 1u) / IDA_ECC_QW;
    for (uint32_t q = ilk; q <= son && q < IDA_ECC_QW_N; q++) {
        if ((e->bozuk[q / 8u] & (1u << (q % 8u))) != 0u) {
            return true;
        }
    }
    return false;
}

void ida_ecc_sayfa_sil(ida_ecc_t *e, uint32_t sayfa)
{
    const uint32_t qw_sayfa = IDA_ECC_SAYFA / IDA_ECC_QW;
    if (sayfa >= IDA_ECC_NVM_BAYT / IDA_ECC_SAYFA) {
        return;
    }
    const uint32_t bas = sayfa * qw_sayfa / 8u;
    memset(&e->bozuk[bas], 0, qw_sayfa / 8u);
}
