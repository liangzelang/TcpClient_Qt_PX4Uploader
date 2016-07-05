#!/usr/bin/expect
set fileName [lindex $argv 0]
set fileAddr [lindex $argv 1]
spawn JLinkExe
expect "J-Link>" { 
	send "connect\r"
}

expect "Device>" {
	send "STM32F427VI\r"
}

expect "TIF>" {
	send "S\r"
}

expect "Speed>" {
	send "4000\r"
}

expect "J-Link>" {
	#send_user "11111\r"
	#send "loadbin /home/liangzelang/firmware_nuttx_USART3_921600.bin 0x8004000\r"
	send "loadbin $fileName $fileAddr\r"
	
}
interact
