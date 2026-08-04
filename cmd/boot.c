/*
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * Misc boot support
 */
#include <common.h>
#include <command.h>
#include <net.h>
#include <asm/io.h>
#include <asm/arch/boot_mode.h>

#ifdef CONFIG_CMD_GO

/* Allow ports to override the default behavior */
__attribute__((weak))
unsigned long do_go_exec(ulong (*entry)(int, char * const []), int argc,
				 char * const argv[])
{
#ifdef CONFIG_CPU_V7
	ulong addr = (ulong)entry | 1;
	entry = (void *)addr;
#endif
	return entry (argc, argv);
}

static int do_go(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong	addr, rc;
	int     rcode = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], NULL, 16);

	printf ("## Starting application at 0x%08lX ...\n", addr);

	/*
	 * pass address parameter as argv[0] (aka command name),
	 * and all remaining args
	 */
	rc = do_go_exec ((void *)addr, argc - 1, argv + 1);
	if (rc != 0) rcode = 1;

	printf ("## Application terminated, rc = 0x%lX\n", rc);
	return rcode;
}

/* -------------------------------------------------------------------- */

U_BOOT_CMD(
	go, CONFIG_SYS_MAXARGS, 1,	do_go,
	"start application at address 'addr'",
	"addr [arg ...]\n    - start application at address 'addr'\n"
	"      passing 'arg' as arguments"
);
#endif

static int do_reboot_brom(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	u32 reg_val;
	// 1. 读取并打印写入前的寄存器值
	reg_val = readl(CONFIG_ROCKCHIP_BOOT_MODE_REG);
	printf("Boot Mode Reg BEFORE write: 0x%08x to Reg 0x%08x\n", reg_val, CONFIG_ROCKCHIP_BOOT_MODE_REG);

	writel(0xFFFF0000, 0xFD58A004);
	// 2. 写入进入 Maskrom 的标志位
	writel(BOOT_BROM_DOWNLOAD, CONFIG_ROCKCHIP_BOOT_MODE_REG);

	// 3. 增加微小的延时，确保写操作在复位前被硬件完全接收
	udelay(1000);

	// 4. 读取并打印写入后的寄存器值，验证是否写入成功
	reg_val = readl(CONFIG_ROCKCHIP_BOOT_MODE_REG);
	printf("Boot Mode Reg AFTER write:  0x%08x\n", reg_val);
	printf("Expected value:             0x%08x\n", BOOT_BROM_DOWNLOAD);

	if (reg_val == BOOT_BROM_DOWNLOAD) {
		printf("Write SUCCESS! Rebooting to Maskrom...\n");
	} else {
		printf("Write FAILED! Value mismatch.\n");
	}

	// 5. 触发系统复位
	do_reset(NULL, 0, 0, NULL);

	return 0;
}

U_BOOT_CMD_ALWAYS(
	rbrom, 1, 0,	do_reboot_brom,
	"Perform RESET of the CPU",
	""
);

U_BOOT_CMD(
	reset, 2, 0,    do_reset,
	"Perform RESET of the CPU",
	""
);

U_BOOT_CMD(
        reboot, 2, 0,    do_reset,
        "Perform RESET of the CPU, alias of 'reset'",
        ""
);

#ifdef CONFIG_CMD_POWEROFF
U_BOOT_CMD(
	poweroff, 1, 0,	do_poweroff,
	"Perform POWEROFF of the device",
	""
);
#endif
