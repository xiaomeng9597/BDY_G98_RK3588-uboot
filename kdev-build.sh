#!/bin/bash
#

set -ex

rm -f arch/arm/dts/rk3588-evb.dtb
./make.sh rk3588

grep -i pcie .config
grep -i spi .config
grep -i nvme .config
grep -i CONFIG_PHY_ROCKCHIP_SNPS_PCIE3  .config
grep -Ei "SCSI|SATA" .config
grep -i RTL8169 .config
grep -i CONFIG_CMD_IMI .config
grep -i CONFIG_CMD_SETEXPR .config
grep -i CONFIG_EMBED_KERNEL_DTB .config

dtc -I dtb -O dts arch/arm/dts/rk3588-evb.dtb -o g98-uboot.dts
fdtdump u-boot.dtb > u-boot.dts
ls -alh g98-uboot.dts u-boot.dts

./make.sh loader
ls -alh rk3588_spl*.bin
echo "All ok! All done!"
