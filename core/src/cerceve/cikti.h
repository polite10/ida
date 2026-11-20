/*
 * cikti.h — ida_out_t'nin yazımı (W2; A4-K27, K28, K53–K58, K64, K87–K91, K121, K126; 4.14).
 *
 * Çağrı çerçevesi `out`'u (b2)'den sonra bir kez sıfırlar (A4-K27, K121); sonuçları komut
 * dağıtıcısı, det_tx ve bg'yi sonar modülü (yalnız step OK) yazar; geri kalan her alan
 * ida_cik_yaz'da, dönüş koduna göre 4.14'teki satırla yazılır.
 */
#ifndef IDA_CORE_CIKTI_H
#define IDA_CORE_CIKTI_H

#include "cerceve/cerceve.h"
#include "ida/port.h"

/* NOT_INIT (A4-K126): örneğin belleği okunmaz; sabit "kurulmamış" içerik. */
void ida_cik_kurulmamis(const ida_in_t *in, uint8_t src, ida_out_t *out);
/* Bütün dönüş kodları (NOT_INIT hariç): sürekli komut, olaylar, nav, status, health, fc,
   günlük. cg->rc yazılmış olmalıdır. */
void ida_cik_yaz(ida_core_t *c, const ida_cagri_t *cg, const ida_in_t *in, ida_out_t *out);

#endif /* IDA_CORE_CIKTI_H */
