# BDY_G98_RK3588 uboot源码仓

本项目是 **BDY_G98 (RK3588)** 开发板的 U-Boot 引导加载程序适配仓库，基于 Rockchip Linux 官方 U-Boot 进行定制开发。

## 📦 源码基线
- **上游来源**: [rockchip-linux/u-boot](https://github.com/rockchip-linux/u-boot.git)
- **分支**: `next-dev`

## ✨ 适配进展与特性
针对 BDY_G98 硬件平台，已完成以下外设及启动功能的适配：

| 模块 | 状态 | 说明 |
| :--- | :--- | :--- |
| **SATA** | ✅ 已适配 | 可正常识别 SATA 存储设备 |
| **NVMe** | ✅ 已适配 | 双 NVMe M.2 插槽均可识别并启动 |
| **USB 3.0** | ✅ 已适配 | 两个 USB 3.0 端口均工作正常 |
| **启动配置** | ✅ 增强 | 支持自动扫描 `boot.scr` 和 `extlinux.conf` |

## 🚀 默认启动顺序
U-Boot 将按以下优先级自动探测启动介质：
```text
USB -> NVMe -> SATA
```

## 🐛 问题反馈
如在适配或使用过程中发现缺陷，请通过 [Issues](../../issues) 提交详细的问题描述与日志，感谢您的贡献！



