/*
 * (C) Copyright 2019 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <console.h>

__weak void do_board_download(void)
{
}

static int do_download(cmd_tbl_t *cmdtp, int flag,
		       int argc, char * const argv[])
{
	disable_ctrlc(1);

	/* Allow board specific download, maybe noreturn */
	do_board_download();

#ifdef CONFIG_CMD_USB
	printf("Download: Scanning USB...\n");
	run_command("usb start", 0);
#endif

#ifdef CONFIG_CMD_MMC
	printf("Download: Scanning EMMC...\n");
	run_command("mmc rescan", 0);
	run_command("mmc info", 0);
#endif

#ifdef CONFIG_CMD_PCI
	printf("Download: Scanning NVMe...\n");
	run_command("pci enum", 0);
#endif
#ifdef CONFIG_CMD_NVME
	run_command("nvme scan", 0);
#endif

#ifdef CONFIG_CMD_SCSI
	printf("Download: Scanning SCSI...\n");
	run_command("scsi scan", 0);
#endif

	/* Generic download */
#ifdef CONFIG_CMD_ROCKUSB
	run_command("rockusb 0 $devtype $devnum", 0);
#endif
	printf("Enter rockusb failed, fallback to bootrom...\n");
	flushc();
	run_command("rbrom", 0);

	return 0;
}

U_BOOT_CMD_ALWAYS(
	download, 1, 1, do_download,
	"enter rockusb/bootrom download mode", ""
);
