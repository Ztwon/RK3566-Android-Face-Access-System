# RK3566-Android-Face-Access-System
项目名称：基于RK3566 Android平台的人脸识别门禁系统<br>
项目描述：设计开发基于RK3566平台的Android 人脸识别门禁系统，集成虹软算法，NFC 刷卡、闸机控制。<br>
工作内容：
1. 基于 Android 平台集成虹软 ArcFace SDK 实现人脸检测与识别功能
2. 基于 Linux SPI 子系统编写 RC522 NFC 模块的字符设备驱动，实现寄存器读写、复位控制、设备树匹配和字符设备注册，完成 NFC 刷卡身份验证
3. 基于 Linux PWM 子系统编写舵机驱动，通过 proc 文件系统接口实现用户空间对闸机开关的控制
4. 移植 MIPI-CSI 摄像头驱动，完成人脸图像采集与识别流程的对接
5. 修改 Android SELinux 安全策略，保障 OTA 远程升级过程中的权限合规性
