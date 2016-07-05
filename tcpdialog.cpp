#include "tcpdialog.h"
#include "ui_tcpdialog.h"
#include <QDialog>
#include <QPushButton>
#include <QDir>
#include <QFileDialog>
#include <QLineEdit>
#include <QDebug>
#include <QTcpSocket>
#include <QHostAddress>
#include <QMessageBox>
#include <QDataStream>
#include <QtSerialPort/QSerialPort>
#include <QSerialPortInfo>
#include <QTabWidget>
#include <QRadioButton>
#include <QThread>
#include <QProcess>
#include <QCoreApplication>
#include <QStandardPaths>

TcpDialog::TcpDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi(this);
    process=new QProcess(this);
    serial= new QSerialPort(this);
    sendFlag=0;
    times=0;
    buttonInit();
    progressBar->hide();

    connect(browsepushButton,SIGNAL(clicked(bool)),this,SLOT(browseFile()));
    connect(browseBLpushButton,SIGNAL(clicked(bool)),this,SLOT(browseBLFile()));
    connect(connectpushButton,SIGNAL(clicked(bool)),this,SLOT(connectToServer()));
    connect(unconnectpushButton,SIGNAL(clicked(bool)),this,SLOT(closeConnection()));
    connect(eraseBLpushButton,SIGNAL(clicked(bool)),this,SLOT(bl_eraseChip()));
    connect(uploadBLpushButton,SIGNAL(clicked(bool)),this,SLOT(bl_uploadFile()));    
    connect(uploadpushButton,SIGNAL(clicked(bool)),this,SLOT(wifi_openFile()));
    connect(usart_uploadpushButton,SIGNAL(clicked(bool)),this,SLOT(usart_openFile()));    
    connect(clearpushButton,SIGNAL(clicked(bool)),this,SLOT(clearReceiveBuf()));
    connect(erasepushButton,SIGNAL(clicked(bool)),this,SLOT(sendEraseData()));
    connect(openradioButton,SIGNAL(toggled(bool)),this,SLOT(toggleSerialPort()));
    connect(refreshpushButton,SIGNAL(clicked(bool)),this,SLOT(refreshSerialPorts()));
    connect(helppushButton,SIGNAL(clicked(bool)),this,SLOT(showHelp()));
    connect(uploadusbpushButton,SIGNAL(clicked(bool)),this,SLOT(uploadusbFile()));

    connect(&tcpSocket,SIGNAL(connected()),this,SLOT(connectedToServer()));
    connect(&tcpSocket,SIGNAL(readyRead()),this,SLOT(readFromServer()));
    connect(&tcpSocket,SIGNAL(disconnected()),this,SLOT(disconnectedToServer()));
    connect(&tcpSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(connectionError()));

    connect(serial,SIGNAL(readyRead()),this,SLOT(readSerialData()));

    connect(process,SIGNAL(readyReadStandardOutput()),this,SLOT(showData()));
    connect(process,SIGNAL(started()),this,SLOT(processStarted()));
    connect(process,SIGNAL(error(QProcess::ProcessError)),this,SLOT(processError()));
    connect(process,SIGNAL(finished(int,QProcess::ExitStatus)),this,SLOT(killProcess()));
    connect(process,SIGNAL(readyReadStandardError()),this,SLOT(processError()));

    connect(this,SIGNAL(haveOpenedFile()),this,SLOT(readFile()));
    connect(this,SIGNAL(sendAckToServer()),this,SLOT(returnAck()));
    connect(this,SIGNAL(gotSerialData()),this,SLOT(uploadFile()));
    connect(this,SIGNAL(usart_fileOpened()),this,SLOT(uploadFile()));
    connect(this,SIGNAL(wifi_fileOpened()),this,SLOT(clientReady()));

    fillPortsParameters();
    fillPortsNames();
}

TcpDialog::~TcpDialog()
{

}

//该成员函数是整个软件使用方法的说明
void TcpDialog::showHelp()
{
    QString wifiStr="##WiFi 模式##\n "
                    "1、填写TCP服务器的IP地址以及对应端口号（默认已设置好），然后连接。\n "
                    "2、点击Browse选择上载的文件，点击Upload就可以开始烧写。\n"
                    "3、连接上后也可以直接选择擦除芯片，点击Erase Chip。\n\n";
    QString usartStr="##串口模式##\n"
                     "1、分别选择串口的配置参数，再打开串口\n"
                     "2、如果串口号列表中没有对应的串口，点击Refresh Serial Port更新串口号\n"
                     "3、点击Browse选择上载的文件，点击Upload就可以开始烧写。\n\n";
    QString usbStr="##USB 模式##\n"
                   "1、脚本文件夹scripts必须与本程序放置与同一目录下。\n"
                   "2、需要烧写的固件文件必须放置于用户主文件夹目录下。\n"
                   "3、固件名字不可更改，必须为nuttx-px4fmu-v2-default.px4。\n"
                   "4、脚本文件夹名称以及脚本文件名称均不允许修改。\n"
                   "5、直接点击upload就可以开始自动擦除，下载，校验，重启。\n\n";
    QString jlinkStr="##J-Link 模式##\n "
                     "1、选择上载的文件，填写烧写的起始地址。\n"
                     "2、点击Upload即可开始烧写。\n\n";
    plainTextEdit->insertPlainText(wifiStr);
    plainTextEdit->insertPlainText(usartStr);
    plainTextEdit->insertPlainText(usbStr);
    plainTextEdit->insertPlainText(jlinkStr);
}

///////////////////////////////////////////////////////////////////////
//                      该部分为WiFi方式下的部分函数                    //
//////////////////////////////////////////////////////////////////////

//该成员函数初始化一些按钮，按照一定逻辑，使能或失能按钮
void TcpDialog::buttonInit()
{
	browsepushButton->setEnabled(false);           //文件搜索按钮失能
	connectpushButton->setEnabled(true);           //WiFi方式下的connect按钮使能
	unconnectpushButton->setEnabled(false);        //WiFi方式下的unconnect按钮失能
	erasepushButton->setEnabled(false);            //WiFi方式下的erase按钮失能
	uploadpushButton->setEnabled(false);           //WiFi方式下的upload按钮失能
	sendpushButton->setEnabled(false);             //WiFi方式下的Send按钮失能，这个按钮没有使用
	usart_startpushButton->setEnabled(false);      //USART方式下的Start Application按钮失能
	usart_uploadpushButton->setEnabled(false);     //USART方式下upload按钮失能
    uploadBLpushButton->setEnabled(false);         //J-Link
}
//该成员函数是选择上载的文件
void TcpDialog::browseFile()
{
	QString initialName = lineEdit->text();        //从lineEdit部件中得到默认的文件路径
	if(initialName.isEmpty())                      //如果lineEdit部件为空，就设置home路径为默认文件路径
		initialName = QDir::homePath();
	fileName =QFileDialog::getOpenFileName(this,tr("choose file"),initialName);	//在默认路径下，开启一个文件对话框，选择上载的文件
	fileName =QDir::toNativeSeparators(fileName);  //根据不同系统，本地化路径的分隔符
	lineEdit->setText(fileName);                   //将选择的文件路径填充至lineEdit以及plainTextEdit
	plainTextEdit->insertPlainText(fileName);
	uploadpushButton->setEnabled(true);            //使能WiFi方式中的upload按键
	usart_uploadpushButton->setEnabled(true);      //使能USART方式中的upload按键
}

//该成员函数是在WiFi方式下打开之前选择的上载文件
void TcpDialog::wifi_openFile()
{
	if(!fileName.isEmpty()) {                     //如果文件名非空，设置文件名
		file.setFileName(fileName);
		if(!file.open(QIODevice::ReadOnly)) {     //以只读方式打开文件
			qDebug()<<"can't open the file!"<<endl;
		} else {
			plainTextEdit->insertPlainText(tr("\n this File is opened , Please go on  \n"));
			progressBar->setMaximum(file.size()>>10);   //设置进度条的最大值为文件大小
			emit wifi_fileOpened();              //发送文件打开的信号，该信号与槽函数 clientReady（）连接。
		}
	}
}

//该成员函数是在USART方式下打开之前选择的上载文件
void TcpDialog::usart_openFile()
{
	if(!fileName.isEmpty()) {                   //该函数实现与wifi_openFile()基本相同，只是最后发射的信号不同
		 file.setFileName(fileName);
		if(!file.open(QIODevice::ReadOnly)) {
			qDebug()<<"can't open the file!"<<endl;
		} else {
			plainTextEdit->insertPlainText(tr("\n this File is opened , Please go on  \n"));
			progressBar->setMaximum(file.size()>>10);
			emit usart_fileOpened();           //发射文件打开的信号，该信号与槽函数 uploadFile()连接
		}
	}
}

//该成员函数是读取文件，并填充数据帧
//数据帧格式： 0x01+<length>+<Data>  (非结束帧)    0x02+<length>+<Data> (结束帧)
void TcpDialog::readFile()
{
	qint64 length=0;
	char binArray[255]={0};
	binArray[0]=0x01;
	binArray[1]=252;
	length=file.read(&(binArray[2]),252);     //每次欲读取252个字节，得到实际读取长度
	binArray[1]=length;
	if((length<252)||(file.atEnd())) {        //如果长度小于252，或者到了文件末尾
		binArray[0]=0x02;                     //重写数据帧头
		sendFlag=1;                           //置sendFlag为1，使下次数据请求无效
		file.close();                         //关闭文件
	}
	tcpSocket.write(binArray,(length+2));     //发送填充好的数据帧
}

//该成员函数是根据填写的IP和端口号连接TCP服务器
void TcpDialog::connectToServer()
{
	if((!iplineEdit->text().isEmpty())&&(!portlineEdit->text().isEmpty())) {    //如果iplineEdit和portlineEdit非空
		QString ipAdress = iplineEdit->text();         //得到ip地址
		quint64 port = portlineEdit->text().toInt();   //得到端口号
		tcpSocket.connectToHost(ipAdress,port);        //尝试连接到对应的IP和端口
		connectpushButton->setEnabled(false);          //失能connect按钮，失能unconnect按钮
		unconnectpushButton->setEnabled(true);
		plainTextEdit->insertPlainText(tr("connecting ......\n"));
		progressBar->setValue(0);                      //是进度条动态显示
		progressBar->show();
	} else {                                           //如果ip和port有一个为空，则弹出提示
		QMessageBox::warning(this,tr("warning"),tr("Please input the Server IP Adress and Port..."));
	}
}

//该成员函数是已经连上TCP服务器对应信号的槽函数
void TcpDialog::connectedToServer()
{
	QString ipAdress = tcpSocket.peerAddress().toString();    //将服务器的IP和端口号显示出来
	quint64 port = tcpSocket.peerPort();
	erasepushButton->setEnabled(true);        //使能erase按钮
	browsepushButton->setEnabled(true);       //使能browse按钮
	plainTextEdit->insertPlainText(tr("connected to Host \nIP: %1 ,Port:  %2 \n").arg(ipAdress).arg(port));
	progressBar->hide();
}

//该成员函数是断开TCP连接信号（tcpsocket类自带信号）对应的槽函数
void TcpDialog::disconnectedToServer()
{
	tcpSocket.close();                        //关闭连接
	plainTextEdit->insertPlainText(tr("Disconnected from host......\n"));
	connectpushButton->setEnabled(true);      //使能connect按钮
	unconnectpushButton->setEnabled(false);   //失能unconnect按钮
}

//该成员函数是关闭连接
void TcpDialog::closeConnection()
{
	tcpSocket.flush();                        //清除网络缓存
	file.close();                             //关闭文件
	connectpushButton->setEnabled(true);      //使能、失能一部分按钮
	unconnectpushButton->setEnabled(false);
	browsepushButton->setEnabled(false);
	erasepushButton->setEnabled(false);
	sendpushButton->setEnabled(false);
	sendFlag=0;                              //清理一些变量
	times=0;
	tcpSocket.close();                       //关闭TCP连接
	progressBar->hide();
	plainTextEdit->insertPlainText(tr("close the current Tcp connection. \n"));
}

//该成员函数是清理plainTextEdit
void TcpDialog::clearReceiveBuf()
{
	plainTextEdit->clear();
}

//该成员函数是Tcpsocket自带信号error对应的槽函数
void TcpDialog::connectionError()
{
	plainTextEdit->insertPlainText(tcpSocket.errorString());   //打印出错误信息
	plainTextEdit->appendPlainText(tr("\n"));
	closeConnection();
}

//该成员函数测试使用
void TcpDialog::sendMessageToServer()
{
	QByteArray bin("test");
	tcpSocket.write(bin);
}

/*
 * 读取来自TCP server的数据
 * 根据最新的通信协议：
TCP Server(ESP8266)与TCP客户端（Qt端）通信信号定义(C代表TCP客户端，S代表TCP服务器)
	C-->S
		模式（流程）选择信号（C-->S）： 0x01+0x20（upload模式）   0x02+0x20(erase模式)
		回复信号（C-->S）:            0x12+0x10 (OK)
		数据发送信号(C-->S):          0x01+<length>+<Data>(非结束帧)  0x02+<length>+<Data>(结束帧)
	S-->C
		数据请求信号（S-->C）:          0x31+0x01
		擦除确认信号（S-->C）:          0x31+0x02
		提示信号（S-->C）:             0x31+0x03
		结束信号（S-->C）:             0x31+0x04
*/
void TcpDialog::readFromServer()
{
	if(tcpSocket.bytesAvailable()==1) return;
	QByteArray str =  tcpSocket.read(2);
	char *echo =str.data();
	//QByteArray str =  tcpSocket.read(2);
	//above is OK ,however it can't work at some situation
/*
	char echo[2];
	if(tcpSocket.bytesAvailable()==1) {
		return;
	} else if(tcpSocket.bytesAvailable()>=2) {
		QByteArray str=tcpSocket.read(1);
		char *temp1=str.data();
		if((temp1[0]&0xf0)==0) {
			QByteArray str1=tcpSocket.read(1);
			char *temp2=str1.data();
			echo[0]=temp1[0];
			echo[1]=temp2[0];
		} else {
		echo[0]=0x01;
		echo[1]=temp1[0];
		//return;
		}
	}
*/
	progressBar->show();
	if((echo[0]==0x31)&&(echo[1]==0x01)) {                        //数据请求：0x31+0x01 (可自定义)
		if(sendFlag==0) {
			for(quint8 i=0;i<5;i++) {                             //重复发送五次，每次2+252字节
				readFile();                                       //读取文件，填充发送数组，发送
				if(sendFlag==1) break;                            //读取到文件末尾，跳出循环，不再发送
			}
			times++;
			if(times%100==0) {                                    //5*252=1260=1.2kb   故大约120KB显示一次
				plainTextEdit->insertPlainText(QString("send :   %1  KB \n").arg(times*5*252.0/1024.0));
			}
			progressBar->setValue(times*5*252>>10);               //使进度条同步
		} else if(sendFlag==1) {                                  //如果已经读取到文件末尾并发送，那么再接收到数据请求就忽略
			// return;
		}
	} else if((echo[0]==0x31)&&(echo[1]==0x02)) {                 // 芯片擦除请求: 0x31+0x02(可自定义)
		plainTextEdit->insertPlainText("got the 0x31 and 0x02 \n");
		plainTextEdit->insertPlainText("return the Ack  \n");
		int choice=QMessageBox::information(this,"Confirm","Ensure to erase the chip?",QMessageBox::Yes,QMessageBox::No);
		if(choice==QMessageBox::Yes) {
			plainTextEdit->insertPlainText("you push the yes \n");
			emit sendAckToServer();                              //发射回复信号，该信号连接至槽函数returnAck
		} else if(choice==QMessageBox::No) {
			plainTextEdit->insertPlainText("you push the no\n");
			emit sendAckToServer();                              //暂时：无论你选yes或no都按找正常流程走。待逻辑定好再修改此处
		}
		//此处添加可添加一个确认对话框
	} else if((echo[0]==0x31)&&(echo[1]==0x03)) {                //ESP已经做好接受bin文件数据的准备:0x31+0x03(可自定义)
		plainTextEdit->insertPlainText("got the 0x31 and 0x03 \n");
		plainTextEdit->insertPlainText("return the Ack  \n");
		emit sendAckToServer();
	} else if((echo[0]==0x31)&&(echo[1]==0x04)) {                //结束命令：0x31+0x04(可自定义)
		returnAck();                                             //直接调用槽函数，返回回复信号 0x12+0x10
		plainTextEdit->insertPlainText("got the 0x31 and 0x04 \n");
		plainTextEdit->insertPlainText("Everything is OK \n");
		closeConnection();                                       //整个过程结束，需要关闭及清理相应东西
		progressBar->hide();
	} else {                                                     //如果是异常数据，显示出来
		plainTextEdit->insertPlainText(QByteArray(echo).toHex());
		plainTextEdit->appendPlainText(tr("\n"));
	}
}

//该成员函数是发送回复信号：0x12+0x10
void TcpDialog::returnAck()
{
	char ackData[2]={0x12,0x10};
	tcpSocket.write(ackData,sizeof(ackData));
}

//该成员函数与已打开文件信号对应，发送执行upload流程命令
void TcpDialog::clientReady()
{
	char readySignal[2]={0x01,0x20};                       //如果接收的数据为0x01+0x20，执行upload流程
	tcpSocket.write(readySignal,sizeof(readySignal));
}

//该成员函数对应erase按钮，发送执行erase命令
void TcpDialog::sendEraseData()
{
	//TO DO
	//there should be sending the customed erase signal
	//and  you should match the signal in the firmware of ESP8266
	char eraseSignal[2]={0x02,0x20};                      //如果接收的数据为0x02+0x20，执行erase流程
	tcpSocket.write(eraseSignal,sizeof(eraseSignal));
	//TO DO
}


/************************************************/
/*                     该部分为USART方式下的部分函数                  */
/************************************************/

//该成员函数是填充串口的一些参数框
void TcpDialog::fillPortsParameters()
{
	baudRatecomboBox->addItem(QStringLiteral("9600"), QSerialPort::Baud9600);
	baudRatecomboBox->addItem(QStringLiteral("57600"),QSerialPort::Baud57600);
	baudRatecomboBox->addItem(QStringLiteral("115200"),QSerialPort::Baud115200);
	baudRatecomboBox->addItem(tr("Custom"));                                     //波特率：当前9600

	stopBitscomboBox->addItem(QStringLiteral("1"),QSerialPort::OneStop);         //停止位：当前1位停止位
	stopBitscomboBox->addItem(QStringLiteral("2"),QSerialPort::TwoStop);

	dataBitscomboBox->addItem(QStringLiteral("7"),QSerialPort::Data7);           //数据位
	dataBitscomboBox->addItem(QStringLiteral("8"),QSerialPort::Data8);
	dataBitscomboBox->setCurrentIndex(1);                                        //设置当前显示index1 的数，即8位数据位

	paritycomboBox->addItem(QStringLiteral("Even"),QSerialPort::EvenParity);     //校验位：当前无校验位
	paritycomboBox->addItem(QStringLiteral("Odd"),QSerialPort::OddParity);
	paritycomboBox->addItem(QStringLiteral("None"),QSerialPort::NoParity);
	paritycomboBox->setCurrentIndex(2);
}

//该成员函数是填充串口端口号
void TcpDialog::fillPortsNames()
{
	foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {    //搜索当前可获得的串口，并将串口信息赋予QSerialPortInfo对象
		QStringList list;
		list << info.portName();                               //将串口名赋予list
		serialPortcomboBox->addItem(list.first(),list);        //把list添加至串口端口选择框
	}
	serialPortcomboBox->addItem(tr("Custom"));                 //增加自定义选项
}

//该成员函数是更新串口端口
void TcpDialog::refreshSerialPorts()
{
	serialPortcomboBox->clear();                              //把原有的串口信息清理，重新搜索并填充至选择框中
	foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
		QStringList list;
		list << info.portName()  ;
		serialPortcomboBox->addItem(list.first(),list);
	}
	serialPortcomboBox->addItem(tr("Custom"));
}

//该成员函数是提取各参数选择框中的参数，并启动串口
void TcpDialog::startSerialPort()
{
	Settings p ;                                     //初始化一个结构体p
	p.name = serialPortcomboBox->currentText();      //将参数选择框中的参数赋予结构体P
	p.baudRate = static_cast<QSerialPort::BaudRate>(
		 baudRatecomboBox->itemData( baudRatecomboBox->currentIndex()).toInt());
	p.stringBaudRate = QString::number(p.baudRate);
	p.dataBits =static_cast<QSerialPort::DataBits>(
		dataBitscomboBox->itemData(dataBitscomboBox->currentIndex()).toInt());
	p.stringDataBits =  dataBitscomboBox->currentText();
	p.stopBits = static_cast<QSerialPort::StopBits>(
		stopBitscomboBox->itemData(stopBitscomboBox->currentIndex()).toInt());
	p.stringStopBits = stopBitscomboBox->currentText();
	p.parity = static_cast<QSerialPort::Parity>(
		paritycomboBox->itemData(paritycomboBox->currentIndex()).toInt());
	p.stringParity = paritycomboBox->currentText();

	serial->setPortName(p.name);                     //通过结构体P给串口设置参数
	serial->setBaudRate(p.baudRate);
	serial->setDataBits(p.dataBits);
	serial->setParity(p.parity);
	serial->setStopBits(p.stopBits);
	serial->setFlowControl(QSerialPort::NoFlowControl);
	if (serial->open(QIODevice::ReadWrite)) {        //开启串口，如果打开成功，将串口参数：波特率，数据位，校验位，停止位打印出来
		 plainTextEdit->insertPlainText(tr("Connected to %1 : %2, %3, %4, %5")
						  .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
						  .arg(p.stringParity).arg(p.stringStopBits));
	} else {                                        //如果打开串口失败，则打印出错误信息
		QMessageBox::critical(this, tr("Error"), serial->errorString());
	}
	browsepushButton->setEnabled(true);
	connectpushButton->setEnabled(false);
}

//该成员函数是关闭串口
void TcpDialog::closeSerialPort()
{
	if(serial->isOpen()) {                      //如果串口是打开的，则关闭串口
		serial->close();
	}
	file.close();                               //关闭文件
	progressNumber=0;                           //此变量控制文件上载进行到哪里，在uploadFile函数中使用
	plainTextEdit->insertPlainText(tr("SerialPort is closed. \n"));
	connectpushButton->setEnabled(true);
	usart_startpushButton->setEnabled(false);
	browsepushButton->setEnabled(false);
}

//该成员函数是控制串口开关的按钮响应函数
void TcpDialog::toggleSerialPort()
{
	if(openradioButton->isChecked()) {         //如果openradioButton按下，则开启串口，相反关闭串口
		startSerialPort();
	} else {
		closeSerialPort();
	}
}

//该成员函数是发送Reboot指令：0x30+0x20
void TcpDialog::startApplication()
{
	char rebootData[2]={0x30,0x20};
	serial->write(rebootData,sizeof(rebootData));
}

//该成员函数是USART方式下上载固件
// 遵守Bootloader程序中定义的协议
/* *
 *  跳转指令：		{0xfe,0x21,0x72,0xff,0x00,0x4c,0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0x00,0x01,0x00,0x00,0x48,0xf0};
							{0xfe,0x21,0x45,0xff,0x00,0x4c,0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0x00,0x00,0x00,0x00,0xd7,0xac};
 *  同步信号：                      0x21+0x20
 *  擦除信号：                      0x23+0x20
 *  烧写信号：                      0x27+<length>+<Data>+0x20
 *  重启信号：                      0x30+0x20
 *  回复信号：                      0x12+0x10(OK)  0x12+0x13(invalid)
 * */
//该成员函数与自定义信号gotSerialData连接，故每次收到正确回复信号，就调用该函数进入更新的相应阶段
void TcpDialog::uploadFile()
{
	switch (progressNumber) {            //判断progressNumber，进入相应的阶段
	case 0:                              //progressNumber：0   发送同步信号，请求同步
		enterBootloader();               //此处需要为使从firmware跳转至bootloader，先发送两条跳转指令
		usart_uploadpushButton->setEnabled(false);
		plainTextEdit->insertPlainText("begin to upload ... ... \n");
		progressNumber++;                //progressNumber++，以使进入下一阶段
		break;
	case 1:                              //progressNumber：1   发送擦除信号
		eraseChip();
		plainTextEdit->insertPlainText("Erasing chip ... ... \n");
		progressNumber++;                //progressNumber++，以使进入下一阶段
		break;
	case 2:                              //progressNumber：2    循环发送bin文件数据
		if(sendFlag==0) {                //如果发送标志位为0 ，则读取文件，选择发送
			multiProgData();             //读取文件，并直接发送
			times++;
			if((times%5)==0) {           //每1.2KB左右更新一下进度条
				progressBar->setValue(times*252>>10);
			}
		}
	   break;
	case 3:                              //progressNumber：3   发送Reboot信号
		startApplication();              //发送重启信号：0x30+0x20
		plainTextEdit->insertPlainText(tr("start Application ... ... \n"));
		progressNumber++;                //progressNumber++，以使进入下一阶段
		break;
	default:                             //更新完成，重置progressNumber，及其他相关变量
		progressNumber=0;
		sendFlag=0;
		usart_uploadpushButton->setEnabled(true);
		progressBar->hide();
		break;
	}
}

//该成员函数是使飞控跳转至Bootloader并取得同步
void TcpDialog::enterBootloader()
{
	char Reboot_ID1[41]={0xfe,0x21,0x72,0xff,0x00,0x4c,0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0x00,0x01,0x00,0x00,0x48,0xf0};
	char Reboot_ID0[41]={0xfe,0x21,0x45,0xff,0x00,0x4c,0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0x00,0x00,0x00,0x00,0xd7,0xac};
	char insyncData[2]={0x21,0x20};
	 serial->write(insyncData,sizeof(insyncData));         //提前发送一次同步信号，为避免一些怪异的问题

	serial->write(Reboot_ID0,sizeof(Reboot_ID0));
	serial->write(Reboot_ID1,sizeof(Reboot_ID1));
	QThread::msleep(1500);                                 //等待1.5秒主要是飞控从Firmware跳转至bootloader需要一定时间
	serial->readAll();                                     //待到飞控跳转至bootloader，清空串口接收缓存
	serial->write(insyncData,sizeof(insyncData));          //发送同步信号

	progressBar->setMaximum(file.size()>>10);
	progressBar->setValue(0);
	progressBar->show();
}

//该成员函数是发送擦除信号：0x23+0x20
void TcpDialog::eraseChip()
{
	char eraseData[2]={0x23,0x20};
	serial->write(eraseData,sizeof(eraseData));
}

//该成员函数是读取文件，按照格式发送bin文件数据：0x27+<length>+<Data>+0x20
void TcpDialog::multiProgData()
{
	qint64 length=0;
	char binArray[255]={0};
	binArray[0]=0x27;
	binArray[1]=252;
	length=file.read(&(binArray[2]),252);            //欲读取252个字节，得到实际读取字节length
	binArray[1]=length;                              //重写数组中的长度字节
	binArray[length+2]=0x20;                         //结束符
	if((length<252)||(file.atEnd())) {               //如果是length小于252或者文件结尾了
		sendFlag=1;                                  //置发送标志位为1，不再发送数据
		file.close();                                //关闭文件
		progressBar->hide();
		plainTextEdit->insertPlainText(tr("finish to MultiProgram ... ... \n"));
		progressNumber++;                            //文件最后一帧数据读取完成，故progressNumber++,进入下一阶段
	}
	serial->write(binArray,length+3);
}

//该成员函数是串口接收函数
void TcpDialog::readSerialData()
{
	QByteArray serialReceiveData = serial->read(2);     //读取两个字节
	char* serialBuf = serialReceiveData.data();
	if((serialBuf[0]==0x12)&&(serialBuf[1]==0x10)) {    //判断回复信号
		emit gotSerialData();                           //发射gotSerialData信号，该信号与槽函数uploadFile连接
	} else {
		plainTextEdit->insertPlainText(serialReceiveData.toHex());
	}
	}

/*******************************************************/
/*                     J-Link Mode                 */
/*******************************************************/
//该成员函数是选择上载的文件，设计的为Bootloader文件
void TcpDialog::browseBLFile()
{
	QString bl_initialName = blBrowse_lineEdit->text();            //从lineEdit部件中得到默认的文件路径
	if(bl_initialName.isEmpty())                                   //如果lineEdit部件为空，就设置home路径为默认文件路径
		bl_initialName = QDir::homePath();
	QString bl_fileName =QFileDialog::getOpenFileName(this,tr("choose BL file"),bl_initialName);    //在默认路径下，开启一个文件对话框，选择上载的文件
	bl_fileName =QDir::toNativeSeparators(bl_fileName);            //根据不同系统，本地化路径的分隔符
	blBrowse_lineEdit->setText(bl_fileName);                       //将选择的文件路径填充至lineEdit以及plainTextEdit
	plainTextEdit->insertPlainText(bl_fileName);
	uploadBLpushButton->setEnabled(true);
}

//该成员函数擦除芯片，该功能没有实现，需要在expect脚本上实现。
//实现的原理就是传递一个过程参数，脚本根据这个过程参数，执行相应的代码，目前没有这个功能
void TcpDialog::bl_eraseChip()
{
	//TO DO
}
void TcpDialog::bl_uploadFile()
{
	QStringList strList;
	QString fileNameStr=blBrowse_lineEdit->text();          //从文本框中得到上载文件的路径，为expect脚本的第一个参数
	QString fileAddrStr=blAddr_lineEdit->text();            //从文本框中得到烧写的起始地址，为expect脚本的第二个参数
	strList<<fileNameStr<<fileAddrStr;                      //把两个参数合成一个字符串列表
	QString commandStr=QCoreApplication::applicationDirPath().append("/scripts/hello.sh");//根据本程序的位置找到要执行的脚本
	//plainTextEdit->insertPlainText(commandStr);           //测试用，通过此可以看到这个命令是什么
	process->start(commandStr,strList,QProcess::ReadWrite); //开启进程，运行脚本
}

/***********************************************/
/*                                   USB MODE                                         */
/***********************************************/

//该成员函数是USB模式下调用相应脚本的槽函数
//该命令的完整形式是　python XXX.py --port PORT XXX.px4
void TcpDialog::uploadusbFile()
{
	QString execDir=QCoreApplication::applicationDirPath().append("/scripts/px_uploader.py ");            //得到该程序的目录，由于脚本文件夹与该程序至于同一目录，所以得到脚本文件路径
	QString commandLine="python ";                                                                        //找到运行的程序，实际为usr/bin/python
	QString deviceStr="--port /dev/serial/by-id/usb-3D_Robotics*,/dev/serial/by-id/pci-3D_Robotics* ";   //指定端口
	QString filePath=QStandardPaths::writableLocation(QStandardPaths::HomeLocation).append("/nuttx-px4fmu-v2-default.px4");//得到用户主文件目录，由于待上载的文件置于主文件目录，故得到上载文件路径
	//plainTextEdit->insertPlainText(filePath);
	QString commandStr=commandLine+execDir+deviceStr+filePath;                                           //合成命令
	//plainTextEdit->insertPlainText(commandStr);
	process->start(commandStr,QProcess::ReadWrite);                                                      //开启新进程，启动脚本，开始上载程序
}

/**********************************************/
/*          Process 的相关处理函数              */
/*值得注意的由于USB和J-Link都是使用的同一个对象process,
 * 　　　　　　因此每次执行完之后，进程一定要清理干净*/
/*********************************************/
//该成员函数是一个杀死当前进程的槽函数，对应的信号是当前的进程finished
void TcpDialog::killProcess()
{
	process->kill();
	//process->close();
	plainTextEdit->insertPlainText("The process is killed.\n\n");
}

//该成员函数是把进程中的标准输出全部读取到本软件的显示框，该槽函数对应的信号是readyReadStandardOutput()
void TcpDialog::showData()
{
	QByteArray ba=process->readAllStandardOutput();    //读取所有标准输出，将标准输出显示在plainTextEdit控件上
	plainTextEdit->insertPlainText(ba);
	//plainTextEdit->insertPlainText("Standard Output: \n\n");
}

//该成员函数是提示进程开始（脚本开始运行）
void TcpDialog::processStarted()
{
	("Script starts. \n\n");
}

//该成员函数是输出错误信息，并杀死进程。但是该函数好像么有用到
void TcpDialog::processError()
{
	plainTextEdit->insertPlainText("Standard Error:\n\n");
	QByteArray ba= process->readAllStandardError();
	plainTextEdit->insertPlainText(ba);
	killProcess();
}
