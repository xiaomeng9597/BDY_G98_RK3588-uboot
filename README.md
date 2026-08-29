# BDY_G98_RK3588-uboot

This repository contains the uboot source code for BGY G98/Z98 RK3588, adapted from [rockchip-linux/u-boot](https://github.com/rockchip-linux/u-boot.git).

For documentation and usage guides, please visit: [yifengyou/BDY_G98_RK3588](https://github.com/yifengyou/BDY_G98_RK3588)

本仓库为 彼度云 G98/Z98 RK3588 内核源代码，基于 [rockchip-linux/u-boot](https://github.com/rockchip-linux/u-boot.git) 进行适配与修改。

文档及使用指南请参见：[yifengyou/BDY_G98_RK3588](https://github.com/yifengyou/BDY_G98_RK3588)


## 更新日志


- 支持 SATA/SCSI 存储设备的识别与系统引导。
- 支持双 M.2 NVMe 插槽的识别与系统引导。
- 支持存储后端动态切换（NVMe / SATA / SPI Flash / eMMC）。
- 两个 USB 3.0 端口工作正常，支持外设连接与启动介质识别。
- 实现 Distro Boot 规范，自动扫描 `extlinux.conf` 与 `boot.scr` 配置。
- 集成常用调试命令：`rbrom`、`sf`、`scsi`、`nvme`、`mmc` 等，便于底层开发与排错。
- 双存储介质适配：针对 SPI Nor Flash 与 eMMC 在硬件层面的物理引脚冲突问题，分别提供两套独立的配置方案，构建系统自动输出对应的两版固件镜像，用户可根据实际硬件选型按需选择使用。
- 2.5G 网口（RTL8125）：暂不支持，无法使用网络功能。
- 千兆网口（YT9215）：驱动未适配，网络功能不可用
