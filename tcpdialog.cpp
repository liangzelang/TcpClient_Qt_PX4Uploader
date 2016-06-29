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

TcpDialog::TcpDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi(this);
    serial= new QSerialPort(this);
    sendFlag=0;
    times=0;
    buttonInit();
    progressBar->hide();    
    iplineEdit->setText(tr("192.168.4.1"));
    portlineEdit->setText("8086");
    connect(browsepushButton,SIGNAL(clicked(bool)),this,SLOT(browseFile()));
    connect(connectpushButton,SIGNAL(clicked(bool)),this,SLOT(connectToServer()));
    connect(unconnectpushButton,SIGNAL(clicked(bool)),this,SLOT(closeConnection()));
    connect(this,SIGNAL(wifi_fileOpened()),this,SLOT(clientReady()));
    connect(uploadpushButton,SIGNAL(clicked(bool)),this,SLOT(wifi_openFile()));
    connect(usart_uploadpushButton,SIGNAL(clicked(bool)),this,SLOT(usart_openFile()));
    connect(this,SIGNAL(usart_fileOpened()),this,SLOT(uploadFile()));
    connect(clearpushButton,SIGNAL(clicked(bool)),this,SLOT(clearReceiveBuf()));
    connect(erasepushButton,SIGNAL(clicked(bool)),this,SLOT(sendEraseData()));
    connect(openradioButton,SIGNAL(toggled(bool)),this,SLOT(toggleSerialPort()));
    connect(refreshpushButton,SIGNAL(clicked(bool)),this,SLOT(refreshSerialPorts()));
    connect(&tcpSocket,SIGNAL(connected()),this,SLOT(connectedToServer()));
    connect(&tcpSocket,SIGNAL(readyRead()),this,SLOT(readFromServer()));
    connect(&tcpSocket,SIGNAL(disconnected()),this,SLOT(disconnectedToServer()));
    connect(&tcpSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(connectionError()));
    connect(serial,SIGNAL(readyRead()),this,SLOT(readSerialData()));
    connect(this,SIGNAL(haveOpenedFile()),this,SLOT(readFile()));
    connect(this,SIGNAL(sendAckToServer()),this,SLOT(returnAck()));
    connect(this,SIGNAL(gotSerialData()),this,SLOT(uploadFile()));
    fillPortsParameters();
    fillPortsNames();
}

TcpDialog::~TcpDialog()
{

}

///////////////////////////////////////////////////////////////////////
//                      该部分为WiFi方式下的部分函数                          //
//////////////////////////////////////////////////////////////////////

//该成员函数初始化一些按钮，按照一定逻辑，使能或失能按钮
void TcpDialog::buttonInit()
{
	browsepushButton->setEnabled(false);              //文件搜索按钮失能
	connectpushButton->setEnabled(true);              //WiFi方式下的connect按钮使能
	unconnectpushButton->setEnabled(false);        //WiFi方式下的unconnect按钮失能
	erasepushButton->setEnabled(false);                //WiFi方式下的erase按钮失能
	uploadpushButton->setEnabled(false);              //WiFi方式下的upload按钮失能
	sendpushButton->setEnabled(false);                 //WiFi方式下的Send按钮失能，这个按钮没有使用
	usart_startpushButton->setEnabled(false);       //USART方式下的Start Application按钮失能
	usart_uploadpushButton->setEnabled(false);   //USART方式下upload按钮失能
}
//该成员函数是选择上载的文件
void TcpDialog::browseFile()
{
	QString initialName = lineEdit->text();						//从lineEdit部件中得到默认的文件路径
	if(initialName.isEmpty())											//如果lineEdit部件为空，就设置home路径为默认文件路径
		initialName = QDir::homePath();
	fileName =QFileDialog::getOpenFileName(this,tr("choose file"),initialName);	//在默认路径下，开启一个文件对话框，选择上载的文件
	fileName =QDir::toNativeSeparators(fileName);		//根据不同系统，本地化路径的分隔符
	lineEdit->setText(fileName);										//将选择的文件路径填充至lineEdit以及plainTextEdit
	plainTextEdit->insertPlainText(fileName);
	uploadpushButton->setEnabled(true);					//使能WiFi方式中的upload按键
	usart_uploadpushButton->setEnabled(true);			//使能USART方式中的upload按键
}

//该成员函数是在WiFi方式下打开之前选择的上载文件
void TcpDialog::wifi_openFile()
{
	if(!fileName.isEmpty()) {                                            //如果文件名非空，设置文件名
		file.setFileName(fileName);
		if(!file.open(QIODevice::ReadOnly)) {                  //以只读方式打开文件
			qDebug()<<"can't open the file!"<<endl;
		} else {
			plainTextEdit->insertPlainText(tr("\n this File is opened , Please go on  \n"));
			progressBar->setMaximum(file.size()>>10);   //设置进度条的最大值为文件大小
			emit wifi_fileOpened();                                //发送文件打开的信号，该信号与槽函数 clientReady（）连接。
		}
	}
}

//该成员函数是在USART方式下打开之前选择的上载文件
void TcpDialog::usart_openFile()
{
	if(!fileName.isEmpty()) {                                         //该函数实现与wifi_openFile()基本相同，只是最后发射的信号不同
		 file.setFileName(fileName);
		if(!file.open(QIODevice::ReadOnly)) {
			qDebug()<<"can't open the file!"<<endl;
		} else {
			plainTextEdit->insertPlainText(tr("\n this File is opened , Please go on  \n"));
			progressBar->setMaximum(file.size()>>10);
			emit usart_fileOpened();                           //发射文件打开的信号，该信号与槽函数 uploadFile()连接
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
	length=file.read(&(binArray[2]),252);        //每次欲读取252个字节，得到实际读取长度
	binArray[1]=length;
	if((length<252)||(file.atEnd())) {                //如果长度小于252，或者到了文件末尾
		binArray[0]=0x02;                                   //重写数据帧头
		sendFlag=1;                                            //置sendFlag为1，使下次数据请求无效
		file.close();                                               //关闭文件
	}
	tcpSocket.write(binArray,(length+2));        //发送填充好的数据帧
}

//该成员函数是根据填写的IP和端口号连接TCP服务器
void TcpDialog::connectToServer()
{
	if((!iplineEdit->text().isEmpty())&&(!portlineEdit->text().isEmpty())) {    //如果iplineEdit和portlineEdit非空
		QString ipAdress = iplineEdit->text();                //得到ip地址
		 quint64 port = portlineEdit->text().toInt();        //得到端口号
		tcpSocket.connectToHost(ipAdress,port);          //尝试连接到对应的IP和端口
		connectpushButton->setEnabled(false);          //失能connect按钮，失能unconnect按钮
		unconnectpushButton->setEnabled(true);
		plainTextEdit->insertPlainText(tr("connecting ......\n"));
		progressBar->setValue(0);                                   //是进度条动态显示
		progressBar->show();
	} else {                                                                     //如果ip和port有一个为空，则弹出提示
		QMessageBox::warning(this,tr("warning"),tr("Please input the Server IP Adress and Port..."));
	}
}

//该成员函数是已经连上TCP服务器对应信号的槽函数
void TcpDialog::connectedToServer()
{
    QString ipAdress = tcpSocket.peerAddress().toString();    //将服务器的IP和端口号显示出来
    quint64 port = tcpSocket.peerPort();
    erasepushButton->setEnabled(true);                                //使能erase按钮
    browsepushButton->setEnabled(true);                             //使能browse按钮
    plainTextEdit->insertPlainText(tr("connected to Host \nIP: %1 ,Port:  %2 \n").arg(ipAdress).arg(port));
    progressBar->hide();
}

//该成员函数是断开TCP连接信号（tcpsocket类自带信号）对应的槽函数
void TcpDialog::disconnectedToServer()
{
    tcpSocket.close();                                                     //关闭连接
    plainTextEdit->insertPlainText(tr("Disconnected from host......\n"));
    connectpushButton->setEnabled(true);               //使能connect按钮
    unconnectpushButton->setEnabled(false);         //失能unconnect按钮
}

//该成员函数是关闭连接
void TcpDialog::closeConnection()
{
	tcpSocket.flush();                                               //清除网络缓存
	file.close();                                                         //关闭文件
	connectpushButton->setEnabled(true);         //使能、失能一部分按钮
	unconnectpushButton->setEnabled(false);
	browsepushButton->setEnabled(false);
	erasepushButton->setEnabled(false);
	sendpushButton->setEnabled(false);
	sendFlag=0;                                                       //清理一些变量
	times=0;
	tcpSocket.close();                                              //关闭TCP连接
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
		模式（流程）选择信号（C-->S）：  0x01+0x20（upload模式）   0x02+0x20(erase模式)
		回复信号（C-->S）:              0x12+0x10 (OK)
		数据发送信号(C-->S):            0x01+<length>+<Data>(非结束帧)  0x02+<length>+<Data>(结束帧)
	S-->C
		数据请求信号（S-->C）:          0x31+0x01
		擦除确认信号（S-->C）:          0x31+0x02
		提示信号（S-->C）:                 0x31+0x03
		结束信号（S-->C）:                 0x31+0x04
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
	if((echo[0]==0x31)&&(echo[1]==0x01)) {                            //数据请求：0x31+0x01 (可自定义)
		if(sendFlag==0) {
			for(quint8 i=0;i<5;i++) {                  //重复发送五次，每次2+252字节
				readFile();                                    //读取文件，填充发送数组，发送
				if(sendFlag==1) break;              //读取到文件末尾，跳出循环，不再发送
			}
			times++;
			if(times%100==0) {                         //5*252=1260=1.2kb   故大约120KB显示一次
				plainTextEdit->insertPlainText(QString("send :   %1  KB \n").arg(times*5*252.0/1024.0));
			}
			progressBar->setValue(times*5*252>>10);                //使进度条同步
		} else if(sendFlag==1) {                      //如果已经读取到文件末尾并发送，那么再接收到数据请求就忽略
			// return;
		}
	} else if((echo[0]==0x31)&&(echo[1]==0x02)) {                    // 芯片擦除请求: 0x31+0x02(可自定义)
		plainTextEdit->insertPlainText("got the 0x31 and 0x02 \n");
		plainTextEdit->insertPlainText("return the Ack  \n");
		emit sendAckToServer();                  //发射回复信号，该信号连接至槽函数returnAck
																		//此处添加可添加一个确认对话框
	} else if((echo[0]==0x31)&&(echo[1]==0x03)) {                   //ESP已经做好接受bin文件数据的准备:0x31+0x03(可自定义)
		plainTextEdit->insertPlainText("got the 0x31 and 0x03 \n");
		plainTextEdit->insertPlainText("return the Ack  \n");
		emit sendAckToServer();
	} else if((echo[0]==0x31)&&(echo[1]==0x04)) {                  //结束命令：0x31+0x04(可自定义)
		returnAck();                                           //直接调用槽函数，返回回复信号 0x12+0x10
		plainTextEdit->insertPlainText("got the 0x31 and 0x04 \n");
		plainTextEdit->insertPlainText("Everything is OK \n");
		closeConnection();                                //整个过程结束，需要关闭及清理相应东西
		progressBar->hide();
	} else {                                                      //如果是异常数据，显示出来
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
    char readySignal[2]={0x01,0x20};                         //如果接收的数据为0x01+0x20，执行upload流程
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
	baudRatecomboBox->addItem(tr("Custom"));                                                       //波特率：当前9600

	stopBitscomboBox->addItem(QStringLiteral("1"),QSerialPort::OneStop);            //停止位：当前1位停止位
	stopBitscomboBox->addItem(QStringLiteral("2"),QSerialPort::TwoStop);

	dataBitscomboBox->addItem(QStringLiteral("7"),QSerialPort::Data7);                 //数据位
	dataBitscomboBox->addItem(QStringLiteral("8"),QSerialPort::Data8);
	dataBitscomboBox->setCurrentIndex(1);                                                                //设置当前显示index1 的数，即8位数据位

	paritycomboBox->addItem(QStringLiteral("Even"),QSerialPort::EvenParity);       //校验位：当前无校验位
	paritycomboBox->addItem(QStringLiteral("Odd"),QSerialPort::OddParity);
	paritycomboBox->addItem(QStringLiteral("None"),QSerialPort::NoParity);
	paritycomboBox->setCurrentIndex(2);
}

//该成员函数是填充串口端口号
void TcpDialog::fillPortsNames()
{
	foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {       //搜索当前可获得的串口，并将串口信息赋予QSerialPortInfo对象
		QStringList list;
		list << info.portName();                                                                                     //将串口名赋予list
		serialPortcomboBox->addItem(list.first(),list);                                                 //把list添加至串口端口选择框
	}
	serialPortcomboBox->addItem(tr("Custom"));                                                      //增加自定义选项
}

//该成员函数是更新串口端口
void TcpDialog::refreshSerialPorts()
{
	serialPortcomboBox->clear();                    //把原有的串口信息清理，重新搜索并填充至选择框中
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
	Settings p ;                                                               //初始化一个结构体p
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

	serial->setPortName(p.name);                            //通过结构体P给串口设置参数
	serial->setBaudRate(p.baudRate);
	serial->setDataBits(p.dataBits);
	serial->setParity(p.parity);
	serial->setStopBits(p.stopBits);
	serial->setFlowControl(QSerialPort::NoFlowControl);
	if (serial->open(QIODevice::ReadWrite)) {      //开启串口，如果打开成功，将串口参数：波特率，数据位，校验位，停止位打印出来
		 plainTextEdit->insertPlainText(tr("Connected to %1 : %2, %3, %4, %5")
						  .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
						  .arg(p.stringParity).arg(p.stringStopBits));
	} else {                                                            //如果打开串口失败，则打印出错误信息
		QMessageBox::critical(this, tr("Error"), serial->errorString());
	}
	browsepushButton->setEnabled(true);
	connectpushButton->setEnabled(false);
}

//该成员函数是关闭串口
void TcpDialog::closeSerialPort()
{
	if(serial->isOpen()) {                       //如果串口是打开的，则关闭串口
		serial->close();
	}
	file.close();                                      //关闭文件
	progressNumber=0;                      //此变量控制文件上载进行到哪里，在uploadFile函数中使用
	plainTextEdit->insertPlainText(tr("SerialPort is closed. \n"));
	connectpushButton->setEnabled(true);
	usart_startpushButton->setEnabled(false);
	browsepushButton->setEnabled(false);
}

//该成员函数是控制串口开关的按钮响应函数
void TcpDialog::toggleSerialPort()
{
	if(openradioButton->isChecked()) {    //如果openradioButton按下，则开启串口，相反关闭串口
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
	switch (progressNumber) {              //判断progressNumber，进入相应的阶段
	case 0:                                                //progressNumber：0   发送同步信号，请求同步
		enterBootloader();                          //此处需要为使从firmware跳转至bootloader，先发送两条跳转指令
		usart_uploadpushButton->setEnabled(false);
		plainTextEdit->insertPlainText("begin to upload ... ... \n");
		progressNumber++;                       //progressNumber++，以使进入下一阶段
		break;
	case 1:                                               //progressNumber：1   发送擦除信号
		eraseChip();
		plainTextEdit->insertPlainText("Erasing chip ... ... \n");
		progressNumber++;
		break;
	case 2:                                              //progressNumber：2    循环发送bin文件数据
		if(sendFlag==0) {
			multiProgData();
			times++;
			if((times%5)==0) {
				progressBar->setValue(times*252>>10);
			}
		}
	   break;
	case 3:                                            //progressNumber：3   发送Reboot信号
		startApplication();
		plainTextEdit->insertPlainText(tr("start Application ... ... \n"));
		progressNumber++;
		break;
	default:                                       //更新完成，重置progressNumber，及其他相关变量
		progressNumber=0;
		sendFlag=0;
		usart_uploadpushButton->setEnabled(true);
		progressBar->hide();
		break;
	}
}

void TcpDialog::enterBootloader()
{
    char Reboot_ID1[41]={0xfe,0x21,0x72,0xff,0x00,0x4c,0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0x00,0x01,0x00,0x00,0x48,0xf0};
    char Reboot_ID0[41]={0xfe,0x21,0x45,0xff,0x00,0x4c,0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0x00,0x00,0x00,0x00,0xd7,0xac};
    char insyncData[2]={0x21,0x20};
     serial->write(insyncData,sizeof(insyncData));

    serial->write(Reboot_ID0,sizeof(Reboot_ID0));
    serial->write(Reboot_ID1,sizeof(Reboot_ID1));
    QThread::msleep(1500);
    serial->readAll();       //clear the receive buffer.
    serial->write(insyncData,sizeof(insyncData));

    progressBar->setMaximum(file.size()>>10);
    progressBar->setValue(0);
    progressBar->show();
}

void TcpDialog::eraseChip()
{
    char eraseData[2]={0x23,0x20};
    serial->write(eraseData,sizeof(eraseData));
}

void TcpDialog::multiProgData()
{

        qint64 length=0;
        char binArray[255]={0};
        binArray[0]=0x27;
        binArray[1]=252;
        length=file.read(&(binArray[2]),252);    //length must be the multiple of 4 ,
        binArray[1]=length;
        binArray[length+2]=0x20;
        if((length<252)||(file.atEnd()))
        {
            sendFlag=1;
            file.close();
            progressBar->hide();
            plainTextEdit->insertPlainText(tr("finish to MultiProgram ... ... \n"));
            progressNumber++;
        }
        serial->write(binArray,length+3);
}

void TcpDialog::readSerialData()
{
    QByteArray serialReceiveData = serial->read(2);
    char* serialBuf = serialReceiveData.data();
    //plainTextEdit->insertPlainText(QString(serialReceiveData));
    if((serialBuf[0]==0x12)&&(serialBuf[1]==0x10))
    {
       // plainTextEdit->insertPlainText("send data OK \n");
        emit gotSerialData();
    }
    else
    {
        plainTextEdit->insertPlainText(serialReceiveData.toHex());
    }
}
