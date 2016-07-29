# 使用说明文档

## bootloader

	1　git clone git@192.168.1.201:firmware-tools/bootloader.git
	2　导入Eclipse,添加Make Target :px4fmuv2_bl ,点击px4fmuv2_bl编译，得到bootloader:px4fmuv2_bl.bin文件
	３　使用J-Link烧写程序，起始地址0x8000000

## mavesp8266

### １　安装plantformio

	sudo pip install -U pip setuptools
	sudo pip install -U platformio


### ２　编译

	git clone git@192.168.1.201:firmware-tools/mavesp8266.git
	cd mavesp8266
	git submodule init
	git submodule update
	platformio run

### 常用命令

	pio run -t clean  //clean 工程
	pio run -e esp01 -t upload  //编译esp01模块的程序，并下载

#### 注意：下载时候可能出现烧录不成功的情况，请重新插拔，clean,upload.

## uploader-wifi

### 使用方法

	1 git clone git@192.168.1.201:firmware-tools/uploader-wifi.git
	2 导入Qt Creator,在project->build dirctory修改编译路径，通常只要修改username就行
	3 点击编译，即可生成可执行的客户端。这个就在第２步的路径中。
	４在客户端中根据实际选择更新固件方式

### 常用方式固件更新方法

####  WiFi 方式
	1 在电脑端连接ESP8266的WiFi,名称wifibl,密码uav12345
	2 在Qt客户端点击connect即可，选择更新的固件(bin文件)，点击upload即可
	3 下载过程中，在擦除芯片前会弹出确认对话框，点击Yes即可
	4 自动下载完成，如不需再进行操作，点击unconnect断开连接，飞控、ESP8266即可正常运行。

#### USART 方式
	1　点击连接串口，选择更新的固件(bin文件)，点击upload即可自动下载
##### 注意：串口使用的飞控串口３，波特率115200

#### USB 方式
	1 将固件(px4文件)放在/home/<username>目录下，点击upload即可

#### SD卡方式
	1 该方式不依赖与Qt客户端，直接将需要更新的固件(bin文件)复制到SD中，并重命名为fw.bin.
	2 将ＳＤ卡插入飞控板，上电即可自动更新
