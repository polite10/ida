/*
 * gunluk.c — günlük tamponu (W2; A4-K90, tablo 3.15).
 */
#include "telemetri/telemetri.h"

void ida_gun_kur(ida_gunluk_t *g)
{
    g->n = 0u;
    g->dusen = 0u;
}

uint8_t ida_gun_onem(const ida_log_t *k)
{
    switch (k->code) {
    case IDA_LOG_RESET_INFO:
    case IDA_LOG_ARM_CHANGE:
    case IDA_LOG_SESSION:
    case IDA_LOG_SAFETY_INPUT:
        return (uint8_t)IDA_SEV_NOTICE;
    case IDA_LOG_PARAM_LOAD:
        return (k->arg0 == (int32_t)IDA_RC_PARAM_DEFAULTS) ? (uint8_t)IDA_SEV_WARNING
                                                             : (uint8_t)IDA_SEV_INFO;
    case IDA_LOG_PARAM_COMMIT:
        return (k->arg0 == 0) ? (uint8_t)IDA_SEV_WARNING : (uint8_t)IDA_SEV_INFO;
    case IDA_LOG_PARAM_SAVE:
        return (k->arg1 == (int32_t)IDA_PSTORE_FAIL) ? (uint8_t)IDA_SEV_WARNING
                                                      : (uint8_t)IDA_SEV_INFO;
    case IDA_LOG_MODE_CHANGE:
        return (uint8_t)IDA_SEV_INFO;
    case IDA_LOG_FAILSAFE_CHANGE:
        return (k->arg0 >= (int32_t)IDA_FS_SURFACE) ? (uint8_t)IDA_SEV_CRITICAL
                                                     : (uint8_t)IDA_SEV_WARNING;
    case IDA_LOG_IMU_GAP:
    case IDA_LOG_SAMPLE_CONFLICT:
        return (uint8_t)IDA_SEV_WARNING;
    case IDA_LOG_IMU_LAG:
    case IDA_LOG_DET_STREAM:
        return (k->arg0 == 1) ? (uint8_t)IDA_SEV_WARNING : (uint8_t)IDA_SEV_INFO;
    case IDA_LOG_IMU_STALE:
        return (k->arg0 == 1) ? (uint8_t)IDA_SEV_CRITICAL : (uint8_t)IDA_SEV_INFO;
    case IDA_LOG_WALL_BACKSTEP:
    case IDA_LOG_BG_SLICE_MISMATCH:
    case IDA_LOG_BG_BAD_INPUT:
    case IDA_LOG_EVENT_UNKNOWN:
        return (uint8_t)IDA_SEV_ERROR;
    case IDA_LOG_EVENT_RESULT:
        return ((k->arg1 == (int32_t)IDA_EVR_APPLIED) || (k->arg1 == (int32_t)IDA_EVR_DUPLICATE))
                   ? (uint8_t)IDA_SEV_INFO
                   : (uint8_t)IDA_SEV_WARNING;
    case IDA_LOG_FC_STATE:
        return (k->arg0 == (int32_t)IDA_FCS_FAULT_LATCHED) ? (uint8_t)IDA_SEV_ERROR
                                                            : (uint8_t)IDA_SEV_INFO;
    case IDA_LOG_FC_INHIBIT:
        return (uint8_t)IDA_SEV_DEBUG;
    case IDA_LOG_NUMERIC_FAULT:
    case IDA_LOG_CORE_INTEGRITY:
        return (uint8_t)IDA_SEV_CRITICAL;
    default:
        /* 3.15 dışında kod üretilmez; buraya düşen bir geliştirme hatasıdır. */
        return (uint8_t)IDA_SEV_ERROR;
    }
}

void ida_gun_ekle(ida_gunluk_t *g, const ida_log_t *k)
{
    if (g->n >= IDA_PORT_MAX_LOGS) {
        g->dusen++;
        return;
    }
    ida_log_t *h = &g->kayit[g->n];
    h->t_w = k->t_w;
    h->code = k->code;
    h->severity = ida_gun_onem(k);
    h->arg0 = k->arg0;
    h->arg1 = k->arg1;
    g->n++;
}

void ida_gun_bosalt(ida_gunluk_t *g, ida_out_t *out, ida_core_counters_t *cnt)
{
    for (uint32_t i = 0u; i < (uint32_t)g->n; i++) {
        out->logs[i] = g->kayit[i];
    }
    out->n_logs = g->n;
    cnt->log_dropped += g->dusen;
    g->n = 0u;
    g->dusen = 0u;
}
