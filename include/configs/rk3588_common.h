/* SPDX-License-Identifier:     GPL-2.0+ */
/*
 * (C) Copyright 2021 Rockchip Electronics Co., Ltd
 *
 */

#ifndef __CONFIG_RK3588_COMMON_H
#define __CONFIG_RK3588_COMMON_H

#include "rockchip-common.h"

#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SPL_TEXT_BASE		0x00000000
#ifdef CONFIG_SPL_SKIP_RELOCATE
#define CONFIG_SPL_MAX_SIZE		0x00040000
#else
#define CONFIG_SPL_MAX_SIZE		0x00080000
#endif
#define CONFIG_SPL_BSS_START_ADDR	0x03fe0000
#define CONFIG_SPL_BSS_MAX_SIZE		0x00010000
#define CONFIG_SPL_STACK		0x03fe0000
#ifdef CONFIG_SPL_LOAD_FIT_ADDRESS
#undef CONFIG_SPL_LOAD_FIT_ADDRESS
#endif
#define CONFIG_SPL_LOAD_FIT_ADDRESS	0x10000000

#define CONFIG_SYS_MALLOC_LEN		(32 << 20)
#define CONFIG_SYS_CBSIZE		1024
#define CONFIG_SKIP_LOWLEVEL_INIT

#ifdef CONFIG_SUPPORT_USBPLUG
#define CONFIG_SYS_TEXT_BASE		0x00000000
#else
#define CONFIG_SYS_TEXT_BASE		0x00200000
#endif

#define CONFIG_SYS_INIT_SP_ADDR		0x00600000
#define CONFIG_SYS_LOAD_ADDR		0x00600800
#define CONFIG_SYS_BOOTM_LEN		(64 << 20)	/* 64M */
#define COUNTER_FREQUENCY		24000000

#define GICD_BASE			0xfe600000
#define GICR_BASE			0xfe680000
#define GICC_BASE			0xfe600000

/* secure otp */
#define OTP_UBOOT_ROLLBACK_OFFSET	0x150
#define OTP_UBOOT_ROLLBACK_WORDS	2	/* 64 bits, 2 words */
#define OTP_ALL_ONES_NUM_BITS		32
#define OTP_SECURE_BOOT_ENABLE_ADDR	0x20
#define OTP_SECURE_BOOT_ENABLE_SIZE	1
#define OTP_RSA4096_ENABLE_ADDR		0x21
#define OTP_RSA4096_ENABLE_SIZE		1
#define OTP_RSA_HASH_ADDR		0x9c0
#define OTP_RSA_HASH_SIZE		32

/* MMC/SD IP block */
#define CONFIG_BOUNCE_BUFFER

#define CONFIG_SYS_SDRAM_BASE		0
#define SDRAM_MAX_SIZE			0xf0000000
#define CONFIG_SYS_NONCACHED_MEMORY	(1 << 20)	/* 1 MiB */

#ifndef CONFIG_SPL_BUILD
/* usb mass storage */
#define CONFIG_USB_FUNCTION_MASS_STORAGE
#define CONFIG_ROCKUSB_G_DNL_PID	0x350b
#define ROCKUSB_FSG_BUFLEN		0x400000

/* Nand */
#define CONFIG_SYS_NAND_BASE		0
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_NAND_PAGE_SIZE	2048
#define CONFIG_SYS_NAND_PAGE_COUNT	64
#define CONFIG_SYS_NAND_SIZE		(256 * 1024 * 1024)

/*
 * decompressed kernel:  4M ~ 84M
 *	Why not start from 2M ? if kernel < 5.10 in Android image,
 *	the image header will use the 0x180000~0x200000, which is
 *	overlap with share memory region 0x100000~0x200000.
 *
 * compressed kernel:   84M ~ 130M
 */
#define ENV_MEM_LAYOUT_SETTINGS \
	"scriptaddr=0x00500000\0" \
	"pxefile_addr_r=0x00600000\0" \
	"fdt_addr_r=0x08300000\0" \
	"kernel_addr_r=0x00400000\0" \
	"kernel_addr_c=0x05480000\0" \
	"ramdisk_addr_r=0x0a200000\0" \
	"bootcmd_nvme=echo NVMe boot: pci enum; pci enum; echo NVMe boot: scan; nvme scan; echo NVMe boot: info; nvme info; echo NVMe boot: part 0; nvme part 0; echo NVMe boot: part 1; nvme part 1; setenv devnum 0; if nvme dev 0; then run nvme_boot; fi; setenv devnum 1; if nvme dev 1; then run nvme_boot; fi; echo NVMe boot: no boot script found in NVME; \0" \
	"nvme_boot=setenv devtype nvme; for distro_bootpart in 1 2 3 4; do for prefix in / /boot/; do for script in boot.scr.uimg boot.scr; do echo Trying nvme ${devnum}:${distro_bootpart} ${prefix}${script}; if test -e nvme ${devnum}:${distro_bootpart} ${prefix}${script}; then echo Found Armbian script ${prefix}${script} on nvme ${devnum}:${distro_bootpart}; load nvme ${devnum}:${distro_bootpart} ${scriptaddr} ${prefix}${script}; source ${scriptaddr}; echo SCRIPT FAILED: continuing...; fi; done; done; done \0" \
        "bootcmd_usb=echo USB boot: start; usb start; echo USB boot: info; usb info; setenv devnum 0; if usb dev 0; then run usb_boot; fi; setenv devnum 1; if usb dev 1; then run usb_boot; fi; setenv devnum 2; if usb dev 2; then run usb_boot; fi; setenv devnum 3; if usb dev 3; then run usb_boot; fi; echo USB boot: no boot script found in USB; \0" \
        "usb_boot=setenv devtype usb; for distro_bootpart in 1 2 3 4; do for prefix in / /boot/; do for script in boot.scr.uimg boot.scr; do echo Trying usb ${devnum}:${distro_bootpart} ${prefix}${script}; if test -e usb ${devnum}:${distro_bootpart} ${prefix}${script}; then echo Found Armbian script ${prefix}${script} on usb ${devnum}:${distro_bootpart}; load usb ${devnum}:${distro_bootpart} ${scriptaddr} ${prefix}${script}; source ${scriptaddr}; echo SCRIPT FAILED: continuing...; fi; done; done; done \0" \
        "bootcmd_sata=echo SATA boot: init; sata init; echo SATA boot: info; sata info; setenv devnum 0; if sata dev 0; then run sata_boot; fi; setenv devnum 1; if sata dev 1; then run sata_boot; fi; echo SATA boot: no boot script found in SATA; \0" \
        "sata_boot=setenv devtype sata; for distro_bootpart in 1 2 3 4; do for prefix in / /boot/; do for script in boot.scr.uimg boot.scr; do echo Trying sata ${devnum}:${distro_bootpart} ${prefix}${script}; if test -e sata ${devnum}:${distro_bootpart} ${prefix}${script}; then echo Found Armbian script ${prefix}${script} on sata ${devnum}:${distro_bootpart}; load sata ${devnum}:${distro_bootpart} ${scriptaddr} ${prefix}${script}; source ${scriptaddr}; echo SCRIPT FAILED: continuing...; fi; done; done; done \0"


#include <config_distro_bootcmd.h>

#define CONFIG_EXTRA_ENV_SETTINGS \
	BOOTENV_SHARED_MTD	\
	ENV_MEM_LAYOUT_SETTINGS \
	"partitions=" PARTS_RKIMG \
	ROCKCHIP_DEVICE_SETTINGS \
	RKIMG_DET_BOOTDEV \
	BOOTENV
#endif

/* rockchip ohci host driver */
#define CONFIG_USB_OHCI_NEW
#define CONFIG_SYS_USB_OHCI_MAX_ROOT_PORTS	1

#define CONFIG_PREBOOT
#define CONFIG_LIB_HW_RAND

#endif
