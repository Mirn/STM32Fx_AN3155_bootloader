#include <stdbool.h>
#include <stdint.h>
#include <string.h>

//AN3155: USART protocol used in the STM32™ bootloader
//http://www.st.com/st-web-ui/static/active/cn/resource/technical/document/application_note/CD00264342.pdf

#define LENGTH(array_ptr) (sizeof(array_ptr) / sizeof(array_ptr[0]))

#ifdef STM32F10X_LD_VL
#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#endif

#ifdef STM32F4XX
#include "stm32f4xx.h"
#include "stm32f4xx_flash.h"

#define FLASH_GetReadOutProtectionStatus() FLASH_OB_GetRDP()
#endif

#include "usart_mini.h"
#include "bootloader.h"

#include "rnd_gen.h"

#define BOOT_ACK      0x79
#define BOOT_ERROR    0x1F
#define BOOT_ACTIVATE 0x7F

#define BOOT_GET_COMMANDS      0x00
#define BOOT_VERISON_PROTECT   0x01
#define BOOT_GET_ID            0x02
#define BOOT_READ_MEMORY       0x11
#define BOOT_GO                0x21
#define BOOT_WRITE_MEMORY      0x31
#define BOOT_ERASE             0x43
#define BOOT_ERASE_EXT         0x44
#define BOOT_WRITE_PROTECT     0x63
#define BOOT_WRITE_UNPROTECT   0x73
#define BOOT_READOUT_PROTECT   0x82
#define BOOT_READOUT_UNPROTECT 0x92

#define BOOT_VERSION    0x22
#define BOOT_CMD_COUNT  0x0B
#define BOOT_OPTIONS_A  0x00
#define BOOT_OPTIONS_B  0x00

#define BOOT_PID_Low_density               0x412
#define BOOT_PID_Medium_density            0x410
#define BOOT_PID_High_density              0x414
#define BOOT_PID_Connectivity_line         0x418
#define BOOT_PID_Medium_density_value_line 0x420
#define BOOT_PID_High_density_value_line   0x428
#define BOOT_PID_XL_density                0x430
#define BOOT_PID_Medium_density_ultralow_power_line 0x416
#define BOOT_PID_STM32F2xx_devices         0x411
#define BOOT_PID_STM32F4xx_devices		   0x413

#ifdef STM32F10X_LD_VL
#define BOOT_PID                           BOOT_PID_Medium_density_value_line
#endif

#ifdef STM32F4XX
#define BOOT_PID                           BOOT_PID_STM32F4xx_devices
#endif

static bool cmd_get_commands();
static bool cmd_version_protect();
static bool cmd_get_id();
static bool cmd_mem_read();
static bool cmd_go();
static bool cmd_erase();
static bool cmd_mem_write();

typedef bool (*tCMD_func)();
const tCMD_func commands[] = {
		[BOOT_GET_COMMANDS]      = cmd_get_commands,
		[BOOT_VERISON_PROTECT]   = cmd_version_protect,
		[BOOT_GET_ID]            = cmd_get_id,
		[BOOT_READ_MEMORY]       = cmd_mem_read,
		[BOOT_GO]				 = cmd_go,
		[BOOT_ERASE]             = cmd_erase,
		[BOOT_WRITE_MEMORY]      = cmd_mem_write,
};

uint8_t bootloader_block[256];
uint32_t bootloader_old_addr = 0;
uint8_t *bootloader_main_end = 0;

bool bootloader_init(bool check_protect)
{
	if (check_protect && (FLASH_GetReadOutProtectionStatus() == RESET))
		while (true)
			send_str("NEED_PROTECT\r");

	bootloader_main_end = (void*)(FLASH_BASE + FLASH_SIZE*1024 - 1);
	while (((uint32_t)bootloader_main_end) > BOOTLOADER_TO)
	{
		if (*bootloader_main_end != 0xFF) break;
		bootloader_main_end--;
	}

	//printf("bootloader_main_end = 0x%x\r", bootloader_main_end);
	return (((uint32_t)bootloader_main_end) > BOOTLOADER_TO);
}

bool bootloader_activate(uint32_t timeout)
{
    while(timeout--)
    {
    	if (!recive_ready()) continue;

    	bool activated = (recive() == BOOT_ACTIVATE);
		send(activated ? BOOT_ACK : BOOT_ERROR);
		if (activated) return true;
    }
    return false;
}

void bootloader_commands()
{
	uint32_t timeout = 1000000;

	while (timeout--)
	{
		if (!recive_ready()) continue;

		uint8_t num = recive();
		uint8_t xor = num;

		bool normal = true;
		normal = normal && wait(&xor);
		normal = normal && ((num ^ xor) == 0xFF);
		normal = normal && (commands[num] != NULL);
		normal = normal && (num < LENGTH(commands));

		send(normal ? BOOT_ACK : BOOT_ERROR);
		if (!normal) continue;

		if ((*commands[num])())
			timeout = 1000000;
		else
			send(BOOT_ERROR);
	};

	send(BOOT_ERROR); //send_str("\rBOOT: commands timeout\r");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static bool cmd_get_commands()
{
	static const uint8_t commands_list[] = {
			BOOT_CMD_COUNT,
			BOOT_VERSION,
			BOOT_GET_COMMANDS,
			BOOT_VERISON_PROTECT,
			BOOT_GET_ID,
			BOOT_READ_MEMORY,
			BOOT_GO,
			BOOT_WRITE_MEMORY,
			BOOT_ERASE,
			BOOT_WRITE_PROTECT,
			BOOT_WRITE_UNPROTECT,
			BOOT_READOUT_PROTECT,
			BOOT_READOUT_UNPROTECT,
			BOOT_ACK,
	};
	send_block(commands_list);
	return true;
}

static bool cmd_version_protect()
{
	static const uint8_t version_info[] = {
			BOOT_VERSION,
			BOOT_OPTIONS_A,
			BOOT_OPTIONS_B,
			BOOT_ACK,
	};
	send_block(version_info);
	return true;
}

static bool cmd_get_id()
{
	static const uint8_t id_info[] = {
			0x01,
			(BOOT_PID >> 8) & 0xFF,
			(BOOT_PID >> 0) & 0xFF,
			BOOT_ACK,
	};
	send_block(id_info);
	return true;
}

static bool check_addr(uint32_t addr)
{
	if ((addr >= BOOTLOADER_FROM) && (addr < BOOTLOADER_TO)) return false;

	if ((addr >> 16) == (((uint32_t)(&FLASH_SIZE)) >> 16)) return true;
	if ((addr >> 24) == (FLASH_BASE >> 24)) return true;

	return false;
}

static bool cmd_get_addr(uint32_t *addr)
{
	uint8_t xor = 0;
	uint8_t val = 0;
	uint32_t result = 0;

	if (!wait(&val)) return false; xor ^= val; result = (result << 8) | val;
	if (!wait(&val)) return false; xor ^= val; result = (result << 8) | val;
	if (!wait(&val)) return false; xor ^= val; result = (result << 8) | val;
	if (!wait(&val)) return false; xor ^= val; result = (result << 8) | val;
	if (!wait(&val)) return false; xor ^= val;

	if (xor != 0) return false;

	if ((result >> 24) == (FLASH_BASE >> 24))
		result += BOOTLOADER_SIZE;

	if (!check_addr(result + 0x00)) return false;
	if (!check_addr(result + 0xFF)) return false;

	*addr = result;
	send(BOOT_ACK);
	return true;
}

static bool check_lineral(uint32_t addr, uint32_t count)
{
	if ((addr >> 24) != (FLASH_BASE >> 24)) return true;

	bool start = (addr == BOOTLOADER_TO);
	if (start)
		rnd_init();

	bool result = start || (addr == bootloader_old_addr);
	bootloader_old_addr = addr + count;
	return result;
}

static bool cmd_get_count(uint16_t *count)
{
	*count = 0;
	uint8_t buf[2] = {0, 0};

	if (!wait(buf + 0)) return false;
	if (!wait(buf + 1)) return false;
	if ((buf[0] ^ buf[1]) != 0xFF) return false;

	*count = ((uint16_t)buf[0]) + 1;
	return true;
}

static bool cmd_mem_read()
{
	uint32_t addr = 0;
	uint16_t count = 0;
	uint16_t pos = 0;

	if (!cmd_get_addr(&addr)) return false;
	if (!cmd_get_count(&count)) return false;
	if (!check_lineral(addr, count)) return false;

	send(BOOT_ACK);

	for (pos = 0; pos < count; pos++)
	{
		bootloader_block[pos] = ((uint8_t*)addr)[pos];

		if ((addr + pos) == ((uint32_t)(&FLASH_SIZE)) + 0) bootloader_block[pos] = FLASH_SIZE_CORRECT_L;
		if ((addr + pos) == ((uint32_t)(&FLASH_SIZE)) + 1) bootloader_block[pos] = FLASH_SIZE_CORRECT_H;
	}

	if ((addr >> 24) == (FLASH_BASE >> 24))
		for (pos = 0; pos < count; pos++)
		{
			if ((addr + pos) > (uint32_t)bootloader_main_end) break;
			bootloader_block[pos] ^= rnd_calc();
		}

	__send_block(bootloader_block, count);
	return true;
}

static bool cmd_mem_write()
{
	uint32_t addr = 0;
	uint16_t count = 0;
	uint8_t  val = 0;

	if (!cmd_get_addr(&addr)) return false;

	if (!wait(&val)) return false;
	count = ((uint16_t)val) + 1;
	if (count == 0) return false;
	if ((count % 4) != 0) return false;
	if (!check_lineral(addr, count)) return false;

	uint8_t   xor = val;
	uint8_t  *byte_data  = bootloader_block;
	uint32_t *word_data  = (uint32_t*)byte_data;
	uint8_t   word_count = count / 4;

	while (count)
	{
		if (!wait(byte_data)) return false;
		xor ^= *byte_data;
		byte_data++;
		count--;
	}

	if (!wait(&val)) return false;
	if (xor != val) return false;

	byte_data = bootloader_block;
	count = word_count * 4;
	while (count > 0)
	{
		*byte_data ^= rnd_calc();
		byte_data++;
		count--;
	}

	FLASH_Status status = FLASH_BUSY;
	FLASH_Unlock();

#ifdef STM32F10X_LD_VL
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR | FLASH_FLAG_BSY);
#endif
#ifdef STM32F4XX
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
#endif

	while (word_count--)
	{
		status = FLASH_ProgramWord(addr, *word_data);
		if (status != FLASH_COMPLETE) break;

		addr += 4;
		word_data++;
	}
	FLASH_Lock();

	if (addr > ((uint32_t)bootloader_main_end))
		bootloader_main_end = (void*)(addr);

	send((status == FLASH_COMPLETE) ? BOOT_ACK : BOOT_ERROR);
	return true;
}

static bool cmd_erase()
{
#ifdef STM32F10X_LD_VL
	uint16_t count = 0;
	if (!cmd_get_count(&count)) return false;
	if (count != 0x100) return false;

	uint32_t total_size = FLASH_SIZE * 1024;
	uint32_t addr = FLASH_BASE + BOOTLOADER_SIZE;

	FLASH_Status status = FLASH_BUSY;
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	while (total_size > 0)
	{
		status = FLASH_ErasePage(addr);
		if (status != FLASH_COMPLETE) break;
		addr += FLASH_PAGE_SIZE;
		total_size -= FLASH_PAGE_SIZE;
	}
	FLASH_Lock();
#endif

#ifdef STM32F4XX
	uint8_t a = 0;
	uint8_t b = 0;
	uint8_t c = 0;

	if (!wait(&a)) return false;
	if (!wait(&b)) return false;
	if (!wait(&c)) return false;
	if ((a != 0xFF) || (b != 0xFF) || (c != 0)) return false;

	uint32_t sectors[] = {
			FLASH_Sector_1,
			FLASH_Sector_2,
			FLASH_Sector_3,
			FLASH_Sector_4,
			FLASH_Sector_5,
			FLASH_Sector_6,
			FLASH_Sector_7,
			FLASH_Sector_8,
			FLASH_Sector_9,
			FLASH_Sector_10,
			FLASH_Sector_11,
	};

	if ((FLASH_SIZE != 1024) && (FLASH_SIZE != 512)) return false;

	FLASH_Status status = FLASH_BUSY;
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

	uint32_t pos = 0;
	for (pos = 0; pos < LENGTH(sectors); pos++)
	{
		status = FLASH_EraseSector(sectors[pos], VoltageRange_3);
		if (status != FLASH_COMPLETE) break;
		if ((FLASH_SIZE == 512) && (sectors[pos] == FLASH_Sector_8)) break;
	}
	FLASH_Lock();
#endif

	if (status == FLASH_COMPLETE)
	{
		bootloader_main_end = (void*)BOOTLOADER_TO;
		send(BOOT_ACK);
	}
	else
		send(BOOT_ERROR);
	return true;
}

bool bootloader_go(uint32_t addr)
{
#ifdef STM32F4XX
	if (addr != BOOTLOADER_TO) return false;
	addr = FLASH_BASE + 0x10000;
#endif

	uint32_t *boot_from = (uint32_t*)addr;
	if ((boot_from[0] >> 24) != (SRAM_BASE >> 24)) return false;
	if ((boot_from[1] >> 24) != (FLASH_BASE >> 24)) return false;
	send(BOOT_ACK);

	__set_MSP(boot_from[0]);
	(*(void (*)())(boot_from[1]))();
	return true;
}

static bool cmd_go()
{
	uint32_t addr = 0;
	if (!cmd_get_addr(&addr)) return false;

	return bootloader_go(addr);
}
