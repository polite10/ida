/*
 * stm32u5a9_ida.ld — kart yazılımının bağlayıcı betiği (STM32U5A9NJ, Cortex-M33; W3-kart dilim 1).
 *
 * Bellek (01 §2.9, §2.10; DS13543):
 *   FLASH  Bank 1 + Bank 2, son dört 8 KB sayfa hariç: kod, vektör tablosu, sabitler.
 *   NVM    Bank 2'nin son dört sayfası (0x083F8000): açılış sayacı ve parametre blokları
 *          (dilim 2); bu betik oraya hiçbir şey koymaz.
 *   RAM    SRAM1 768 KB (0x20000000): .data, .bss (çekirdek örneği, halkalar, acil yığın).
 *   SRAM2  64 KB (0x200C0000): .noinit (açılış/hata kaydı; sıfırlamada silinmez, CRC'li)
 *          ve ana yığın 16 KB (SRAM2'nin sonunda; tabanı MSPLIM).
 * SRAM3/SRAM5 bu dilimde kullanılmaz. Yığın (heap) bölgesi yoktur.
 */

ENTRY(Reset_Handler)
EXTERN(ida_vektor_tablosu)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 4064K
  NVM   (r)   : ORIGIN = 0x083F8000, LENGTH = 32K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 768K
  SRAM2 (rw)  : ORIGIN = 0x200C0000, LENGTH = 64K
}

_ida_yigin_bayt = 16K;
_estack = ORIGIN(SRAM2) + LENGTH(SRAM2);
_ida_yigin_tabani = _estack - _ida_yigin_bayt;

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(1024);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } > FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > FLASH
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /* Açılış ve hata kaydı: başlatılmaz, sıfırlanmaz (SRAM2'nin başı; adres sabit). */
  .noinit (NOLOAD) :
  {
    . = ALIGN(8);
    _ida_noinit_bas = .;
    KEEP(*(.noinit))
    KEEP(*(.noinit*))
    . = ALIGN(8);
    _ida_noinit_son = .;
  } > SRAM2

  /* Ana yığın: SRAM2'nin son 16 KB'ı. */
  ._ana_yigin _ida_yigin_tabani (NOLOAD) :
  {
    . = . + _ida_yigin_bayt;
  } > SRAM2

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

ASSERT(_ida_noinit_son <= _ida_yigin_tabani, "noinit bölümü ana yığına taşıyor")
ASSERT(_etext <= ORIGIN(NVM), "kod kalıcı bellek sayfalarına taşıyor")
