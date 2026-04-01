# RK3566-Android-Face-Access-System
本项目是基于 Rockchip RK3566 平台开发的一套工业级智能门禁系统。系统运行在 Android 环境下，深度融合了底层 Linux 驱动开发 与 上层 AI 算法集成。实现了从摄像头原始数据采集（MIPI-CSI）、人脸识别算法处理（ArcFace SDK）、NFC 刷卡鉴权（SPI 驱动）到闸机电机控制（PWM 驱动）的全链路闭环功能。
项目描述：设计开发基于RK3566平台的Android 人脸识别门禁系统，集成虹软算法，NFC 刷卡、闸机控制。
工作内容：
1. 基于 Android 平台集成虹软 ArcFace SDK 实现人脸检测与识别功能
2. 基于 Linux SPI 子系统编写 RC522 NFC 模块的字符设备驱动，实现寄存器读写、复位控制、设备树匹配和字符设备注册，完成 NFC 刷卡身份验证
3. 基于 Linux PWM 子系统编写舵机驱动，通过 proc 文件系统接口实现用户空间对闸机开关的控制
4. 移植 MIPI-CSI 摄像头驱动，完成人脸图像采集与识别流程的对接
5. 修改 Android SELinux 安全策略，保障 OTA 远程升级过程中的权限合规性
