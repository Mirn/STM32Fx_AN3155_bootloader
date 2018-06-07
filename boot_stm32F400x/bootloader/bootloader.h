#ifndef __BOOT_H__
#define __BOOT_H__

#ifdef STM32F10X_LD_VL
#define FLASH_PAGE_SIZE      1024
#define FLASH_SIZE          (*((uint16_t *)0x1FFFF7E0))

extern unsigned long        _filler_end;
#define BOOTLOADER_SIZE    (((uint32_t)(&_filler_end)) - FLASH_BASE)//0x4000
#define BOOTLOADER_FROM    (FLASH_BASE)
#define BOOTLOADER_TO      (FLASH_BASE + BOOTLOADER_SIZE)
#endif

#ifdef STM32F4XX
#define FLASH_PAGE_SIZE      1024
#define FLASH_SIZE          (*((uint16_t *)0x1FFF7A22))

#define BOOTLOADER_SIZE     0x8000
#define BOOTLOADER_FROM    (FLASH_BASE)
#define BOOTLOADER_TO      (FLASH_BASE + BOOTLOADER_SIZE)
#endif

#define FLASH_SIZE_CORRECT   ((FLASH_SIZE*1024 - BOOTLOADER_SIZE) / 1024)
#define FLASH_SIZE_CORRECT_L (FLASH_SIZE_CORRECT & 0xFF)
#define FLASH_SIZE_CORRECT_H (FLASH_SIZE_CORRECT >> 8)


bool bootloader_init(bool check_protect);
bool bootloader_activate();
void bootloader_commands();
bool bootloader_go(uint32_t addr);

#endif
