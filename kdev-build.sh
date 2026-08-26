#!/bin/bash

set -ex

# clean
rm -rf output
mkdir -p output

# only spi
rm -f uboot.img arch/arm/dts/rk3588-evb.dtb
cp -a only-spi/rk3588-evb.dts arch/arm/dts/rk3588-evb.dts
cp -a only-spi/rk3588_defconfig configs/rk3588_defconfig
cp -a only-spi/spl.c common/spl/spl.c
./make.sh rk3588

dtc -I dtb -O dts arch/arm/dts/rk3588-evb.dtb -o only-spi/g98-uboot.dts
fdtdump u-boot.dtb >only-spi/u-boot.dts
ls -alh only-spi/g98-uboot.dts only-spi/u-boot.dts

./make.sh loader
ls -alh rk3588_spl*.bin
mv rk3588_spl_loader_v1.21.114.bin output/rk3588_spl_loader_v1.21.114_only-spi.bin

dd if=uboot.img of=output/uboot-g98_only-spi.img bs=2M count=1
dumpimage -l output/uboot-g98_only-spi.img
ls -alh output/uboot-g98_only-spi.img

cp -a output/rk3588_spl_loader_v1.21.114_only-spi.bin RKDevTool_Release_v3.37/rk3588_spl_loader_v1.21.114.bin
cp -a output/uboot-g98_only-spi.img RKDevTool_Release_v3.37/uboot.img
ls -alh RKDevTool_Release_v3.37
zip -r output/BYD_G98_UBOOT_ONLY-SPI.zip RKDevTool_Release_v3.37

# only emmc
rm -f uboot.img arch/arm/dts/rk3588-evb.dtb
cp -a only-emmc/rk3588-evb.dts arch/arm/dts/rk3588-evb.dts
cp -a only-emmc/rk3588_defconfig configs/rk3588_defconfig
cp -a only-emmc/spl.c common/spl/spl.c
./make.sh rk3588

dtc -I dtb -O dts arch/arm/dts/rk3588-evb.dtb -o only-emmc/g98-uboot.dts
fdtdump u-boot.dtb >only-emmc/u-boot.dts
ls -alh only-emmc/g98-uboot.dts only-emmc/u-boot.dts

./make.sh loader
ls -alh rk3588_spl*.bin
mv rk3588_spl_loader_v1.21.114.bin output/rk3588_spl_loader_v1.21.114_only-emmc.bin

dd if=uboot.img of=output/uboot-g98_only-emmc.img bs=2M count=1
dumpimage -l output/uboot-g98_only-emmc.img
ls -alh output/uboot-g98_only-emmc.img

cp -a output/rk3588_spl_loader_v1.21.114_only-emmc.bin RKDevTool_Release_v3.37/rk3588_spl_loader_v1.21.114.bin
cp -a output/uboot-g98_only-emmc.img RKDevTool_Release_v3.37/uboot.img
ls -alh RKDevTool_Release_v3.37
zip -r output/BYD_G98_UBOOT_ONLY-EMMC.zip RKDevTool_Release_v3.37

# show output
ls -alh output/*
sha256sum output/*
echo "All ok! All done!"
