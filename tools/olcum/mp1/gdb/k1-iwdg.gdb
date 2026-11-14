# W0-MP1a K1: kesme noktası, yığın izi ve IWDG dondurma denemesi (arm-none-eabi-gdb -batch -x).
# DBGMCU_APB1FZR1 = 0xE0044008, DBG_IWDG_STOP = bit 12 (CMSIS stm32u5a9xx.h).
set pagination off
set confirm off
set print pretty off
target remote localhost:61234

echo \n=== 1) kesme noktası ve yığın izi\n
break mp1_isaret
continue
bt
printf "mp1_sayac=%u dbg_apb1fzr1=0x%08x\n", mp1_sayac, *(unsigned int *)0xE0044008
set $s1 = mp1_sayac

echo \n=== 2) IWDG açık, dondurma biti açık: işlemci 10 s durdurulmuş bekliyor\n
shell date +%T
shell sleep 10
shell date +%T
continue
bt
printf "devam sonrası mp1_sayac=%u (beklenen %u: sıfırlama yok)\n", mp1_sayac, $s1 + 1
printf "IWDG_SONUC %s\n", (mp1_sayac == $s1 + 1) ? "SIFIRLAMA_YOK" : "SIFIRLAMA_VAR"

echo \n=== 3) karşı deneme: dondurma biti temizlenir, işlemci 10 s durdurulur (sıfırlama beklenir)\n
set *(unsigned int *)0xE0044008 = *(unsigned int *)0xE0044008 & ~0x1000
printf "dbg_apb1fzr1=0x%08x\n", *(unsigned int *)0xE0044008
continue
printf "durdu: mp1_sayac=%u\n", mp1_sayac
set $s2 = mp1_sayac
delete
shell date +%T
shell sleep 10
shell date +%T
detach
echo \n=== bitti (karşı denemenin sonucu seri kayıtta: yeni B satırı ve iwdgrst=1)\n
