/* Sözleşme başlıkları denetimi: A8 algilama_kaydi.h tek başına, ISO C17. */
#include "ida/algilama_kaydi.h"

size_t ida_denetim_c17_algilama(void);
size_t ida_denetim_c17_algilama(void)
{
    return sizeof(ida_det_record_t) + sizeof(ida_det_batch_t);
}
