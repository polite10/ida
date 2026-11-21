/*
 * stm32u5a9_yer_tutucu.ld — W2 yer tutucu bağlayıcı betiği (STM32U5A9NJ, Cortex-M33).
 *
 * Yalnız kartsız bağlama, harita ve boyut raporu içindir; kartta çalıştırılmaz. W3 kart
 * yazılımı işi ST'nin betiğini bir kez kopyalayıp firmware/bsp/ altında sahiplenir
 * (09 §2.6) ve bu dosyanın yerini alır.
 *
 * Bellek: FLASH 4 MB (0x08000000); RAM olarak yalnız SRAM1 (768 KB, 0x20000000) kullanılır
 * (öteki SRAM bölgeleri W3'te). Yığın RAM'in sonundadır; yığın tavanı W3'te konur.
 */

ENTRY(Reset_Handler)
EXTERN(ida_vektor_tablosu)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 4096K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 768K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_ida_yigin_payi = 0x2000;

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
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

  /* Yığın payı: RAM'e sığmazsa bağlama hata verir. */
  ._yigin_payi (NOLOAD) :
  {
    . = ALIGN(8);
    . = . + _ida_yigin_payi;
    . = ALIGN(8);
  } > RAM

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
