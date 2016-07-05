by ZeLang Liang
# WiFi 模式
+ 填写TCP服务器的IP地址以及对应端口号（默认已设置好），然后连接。 
+ 点击Browse选择上载的文件，点击Upload就可以开始烧写。
+ 连接上后也可以直接选择擦除芯片，点击Erase Chip。
# USART 模式
+ 分别选择串口的配置参数，再打开串口。
+ 如果串口号列表中没有对应的串口，点击Refresh Serial Port更新串口号。
+ 点击Browse选择上载的文件，点击Upload就可以开始烧写。
# USB 模式
+ 脚本文件夹scripts必须与本程序的可执行文件放置与同一目录下。
+ 需要烧写的固件文件必须放置于用户主文件夹目录下。
+ 固件名字不可更改，必须为nuttx-px4fmu-v2-default.px4。
+ 脚本文件夹名称以及脚本文件名称均不允许修改。
+ 直接点击upload就可以开始自动擦除，下载，校验，重启。
# J-Link 模式
+ 选择上载的文件，填写烧写的起始地址。
+ 点击Upload即可开始烧写。

