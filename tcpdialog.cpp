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

   // connect(usart_startpushButton,SIGNAL(clicked(bool)),this,SLOT(startApplication()));

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

void TcpDialog::buttonInit()
{
    browsepushButton->setEnabled(false);
    connectpushButton->setEnabled(true);
    unconnectpushButton->setEnabled(false);
    erasepushButton->setEnabled(false);
    uploadpushButton->setEnabled(false);
    sendpushButton->setEnabled(false);
    usart_startpushButton->setEnabled(false);
    usart_uploadpushButton->setEnabled(false);
}



void TcpDialog::browseFile()
{

    QString initialName = lineEdit->text();                                                                   //default get the path from the lineEdit component
    if(initialName.isEmpty())                                                                                          //if the default path is empty ,I set the home path as the default
            initialName = QDir::homePath();
    fileName =QFileDialog::getOpenFileName(this,tr("choose file"),initialName);  //at the path:initialName ,user choose which file to upload
    fileName =QDir::toNativeSeparators(fileName);                                                   //localize the separators based on the system
    lineEdit->setText(fileName);                                                                                     //input the path of the file into the lineEdit
    plainTextEdit->insertPlainText(fileName);                                                               //input the same message into the plainTextEdit
    uploadpushButton->setEnabled(true);
    usart_uploadpushButton->setEnabled(true);
}

void TcpDialog::wifi_openFile()
{
    if(!fileName.isEmpty())
    {
         file.setFileName(fileName);
        if(!file.open(QIODevice::ReadOnly))
        {
            qDebug()<<"can't open the file!"<<endl;
        }
        else
        {
             //TO DO
            //Add the Function  :
             plainTextEdit->insertPlainText(tr("\n this File is opened , Please go on  \n"));
             progressBar->setMaximum(file.size()>>10);
             emit wifi_fileOpened();
            //TO DO
        }
    }
}

void TcpDialog::usart_openFile()
{
    if(!fileName.isEmpty())
    {
         file.setFileName(fileName);
        if(!file.open(QIODevice::ReadOnly))
        {
            qDebug()<<"can't open the file!"<<endl;
        }
        else
        {
             //TO DO
             plainTextEdit->insertPlainText(tr("\n this File is opened , Please go on  \n"));
             progressBar->setMaximum(file.size()>>10);
             emit usart_fileOpened();
            //TO DO
        }
    }

}


//read the file and load the array
void TcpDialog::readFile()
{
    qint64 length=0;
    char binArray[255]={0};
    binArray[0]=0x01;
    binArray[1]=252;
    length=file.read(&(binArray[2]),252);    //length must be the multiple of 4 ,
    binArray[1]=length;
    if((length<252)||(file.atEnd()))       
    {
        binArray[0]=0x02;
        sendFlag=1;
        file.close();
        //file.seek(0);
    }
    tcpSocket.write(binArray,(length+2));   
}

void TcpDialog::connectToServer()
{
    if((!iplineEdit->text().isEmpty())&&(!portlineEdit->text().isEmpty()))
    {
        QString ipAdress = iplineEdit->text();
         quint64 port = portlineEdit->text().toInt();
        tcpSocket.connectToHost(ipAdress,port);
        connectpushButton->setEnabled(false);
        unconnectpushButton->setEnabled(true);
        //erasepushButton->setEnabled(true);
        plainTextEdit->insertPlainText(tr("connecting ......\n"));
        progressBar->setValue(0);
        progressBar->show();
    }
    else
    {
        QMessageBox::warning(this,tr("warning"),tr("Please input the Server IP Adress and Port..."));
    }
}

void TcpDialog::connectedToServer()
{
    QString ipAdress = tcpSocket.peerAddress().toString();
    quint64 port = tcpSocket.peerPort();
    erasepushButton->setEnabled(true);
    browsepushButton->setEnabled(true);
    plainTextEdit->insertPlainText(tr("connected to Host \nIP: %1 ,Port:  %2 \n").arg(ipAdress).arg(port));
    progressBar->hide();
}

void TcpDialog::disconnectedToServer()
{
    tcpSocket.close();
    plainTextEdit->insertPlainText(tr("Disconnected from host......\n"));
    connectpushButton->setEnabled(true);
    unconnectpushButton->setEnabled(false);
}

void TcpDialog::closeConnection()
{
    tcpSocket.flush();
    file.close();
    connectpushButton->setEnabled(true);
    unconnectpushButton->setEnabled(false);
    browsepushButton->setEnabled(false);
    erasepushButton->setEnabled(false);
    //uploadpushButton->setEnabled(false);
    sendpushButton->setEnabled(false);
    //lineEdit->clear();
    sendFlag=0;
    times=0;
    tcpSocket.close();
    progressBar->hide();
    plainTextEdit->insertPlainText(tr("close the current Tcp connection. \n"));
}

void TcpDialog::clearReceiveBuf()
{
    plainTextEdit->clear();
}

void TcpDialog::connectionError()
{
    plainTextEdit->insertPlainText(tcpSocket.errorString());
    plainTextEdit->appendPlainText(tr("\n"));
    closeConnection();
}

void TcpDialog::sendMessageToServer()
{
    QByteArray bin("test");
    tcpSocket.write(bin);
}


/*
        //  got the Ack from Server
        //according to the procotol which LZL set before ,
        //the first send(0x01+0x20)  is    tell the server ,   client is ready
        //the firsr receive(0x01+0x20) is ask the client whether to erase the rom of Vehicle
               //and the client should return the Ack signals(0x12+0x10)
        //the second receive(0x01+0x20) is tell the client that the Vehicle is ready
               //and the client should return the Ack signals(0x12+0x10)

      loop:
        //the third receive (0x01+0x21)  is ask the client for bin data
                     //and  the client should return the Ack signals(0x01+length+data)
      end:       //especially  , if it is the last transmisson ,the Ack signals(0x20+length+data)

      //the forth(virtual)  reveive (0x02+0x20)  confirm everything is ok
                //and the client should return the Ack signals(0x12+0x10)
*/
void TcpDialog::readFromServer()
{
    if(tcpSocket.bytesAvailable()==1) return;
    QByteArray str =  tcpSocket.read(2);
    char *echo =str.data();
    //QByteArray str =  tcpSocket.read(2);

    //above is OK ,however it can't work at some situation
   /* char echo[2];
    if(tcpSocket.bytesAvailable()==1)
    {
        return;
    }
    else if(tcpSocket.bytesAvailable()>=2)
    {
        QByteArray str=tcpSocket.read(1);
       char *temp1=str.data();
       if((temp1[0]&0xf0)==0)
       {
           QByteArray str1=tcpSocket.read(1);
           char *temp2=str1.data();
           echo[0]=temp1[0];
           echo[1]=temp2[0];
       }
       else
       {
           echo[0]=0x01;
           echo[1]=temp1[0];
           //return;
       }
    }*/
    progressBar->show();
    if((echo[0]==0x01)&&(echo[1]==0x20))
    {
        //TO DO
        //   add signals of return the Ack
        plainTextEdit->insertPlainText("got the 0x01 and 0x20 \n");
        plainTextEdit->insertPlainText("return the Ack  \n");
        emit sendAckToServer();
        //progressBar->setValue(100);
        //TO DO
    }
    else if((echo[0]==0x02)&&(echo[1]==0x20))
    {
        //TO DO
        //add signals of end the task
        //emit sendAckToServer();
        returnAck();
        plainTextEdit->insertPlainText("got the 0x02 and 0x20 \n");
        plainTextEdit->insertPlainText("Everything is OK \n");
        closeConnection();     //in fact , finish all the work.
        progressBar->hide();
        //TO DO
    }
    else if((echo[0]==0x01)&&(echo[1]==0x21))
    {
        //TO DO
        //plainTextEdit->insertPlainText("got the 0x01 and 0x21 \n");
        if(sendFlag==0)
        {
             for(quint8 i=0;i<5;i++)
            {
                readFile();
             }
            times++;
            if(times%100==0)
            {
                plainTextEdit->insertPlainText(QString("send :   %1  KB \n").arg(times*5*252.0/1024.0));
            }
            progressBar->setValue(times*5*252>>10);
        }
        else if(sendFlag==1)
        {
           // return;
        }
    }
    else
    {
        plainTextEdit->insertPlainText(QByteArray(echo).toHex());
        //plainTextEdit->insertPlainText(str.toHex());
        plainTextEdit->appendPlainText(tr("\n"));
    }
}

void TcpDialog::returnAck()
{
    char ackData[2]={0x12,0x10};
    tcpSocket.write(ackData,sizeof(ackData));

}

void TcpDialog::clientReady()
{
    char readySignal[2]={0x01,0x20};
    tcpSocket.write(readySignal,sizeof(readySignal));    
}

void TcpDialog::sendEraseData()
{
    //TO DO
    //there should be sending the customed erase signal
    //and  you should match the signal in the firmware of ESP8266
    char eraseSignal[2]={0x03,0x20};
    tcpSocket.write(eraseSignal,sizeof(eraseSignal));
    //TO DO
}

///////////////////////////////////////////////////////////////////////////////
void TcpDialog::fillPortsParameters()
{
    baudRatecomboBox->addItem(QStringLiteral("9600"), QSerialPort::Baud9600);
    baudRatecomboBox->addItem(QStringLiteral("57600"),QSerialPort::Baud57600);
    baudRatecomboBox->addItem(QStringLiteral("115200"),QSerialPort::Baud115200);
    baudRatecomboBox->addItem(tr("Custom"));

    stopBitscomboBox->addItem(QStringLiteral("1"),QSerialPort::OneStop);
    stopBitscomboBox->addItem(QStringLiteral("2"),QSerialPort::TwoStop);

    dataBitscomboBox->addItem(QStringLiteral("7"),QSerialPort::Data7);
    dataBitscomboBox->addItem(QStringLiteral("8"),QSerialPort::Data8);
    dataBitscomboBox->setCurrentIndex(1);

    paritycomboBox->addItem(QStringLiteral("Even"),QSerialPort::EvenParity);
    paritycomboBox->addItem(QStringLiteral("Odd"),QSerialPort::OddParity);
    paritycomboBox->addItem(QStringLiteral("None"),QSerialPort::NoParity);
    paritycomboBox->setCurrentIndex(2);
}

void TcpDialog::fillPortsNames()
{
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QStringList list;
        list << info.portName();
        serialPortcomboBox->addItem(list.first(),list);
    }
    serialPortcomboBox->addItem(tr("Custom"));
}

void TcpDialog::refreshSerialPorts()
{
    serialPortcomboBox->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QStringList list;
        list << info.portName()  ;
        serialPortcomboBox->addItem(list.first(),list);
    }
    serialPortcomboBox->addItem(tr("Custom"));
}

void TcpDialog::startSerialPort()
{
    Settings p ;
    p.name = serialPortcomboBox->currentText();
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
   // p.flowControl =

    serial->setPortName(p.name);
    serial->setBaudRate(p.baudRate);
    serial->setDataBits(p.dataBits);
    serial->setParity(p.parity);
    serial->setStopBits(p.stopBits);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    if (serial->open(QIODevice::ReadWrite))
    {
         plainTextEdit->insertPlainText(tr("Connected to %1 : %2, %3, %4, %5")
                          .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
                          .arg(p.stringParity).arg(p.stringStopBits));
    }
    else
    {
        QMessageBox::critical(this, tr("Error"), serial->errorString());
    }
    //okpushButton->setEnabled(true);
    browsepushButton->setEnabled(true);
   // usart_startpushButton->setEnabled(true);
   // usart_uploadpushButton->setEnabled(true);
    connectpushButton->setEnabled(false);
}

void TcpDialog::closeSerialPort()
{
    if(serial->isOpen())
    {
        serial->close();
    }
    file.close();
    progressNumber=0;
    plainTextEdit->insertPlainText(tr("SerialPort is closed. \n"));
    connectpushButton->setEnabled(true);
    usart_startpushButton->setEnabled(false);
   // usart_uploadpushButton->setEnabled(false);
    browsepushButton->setEnabled(false);
}


void TcpDialog::toggleSerialPort()
{
    if(openradioButton->isChecked())
    {
        startSerialPort();
    }
    else
    {
        closeSerialPort();
    }
}

void TcpDialog::startApplication()
{
    char rebootData[2]={0x30,0x20};
    serial->write(rebootData,sizeof(rebootData));
}

void TcpDialog::uploadFile()
{
    switch (progressNumber) {
    case 0:
        enterBootloader();
        usart_uploadpushButton->setEnabled(false);
        plainTextEdit->insertPlainText("begin to upload ... ... \n");
        progressNumber++;
        break;
    case 1:
        eraseChip();
        plainTextEdit->insertPlainText("Erasing chip ... ... \n");
        progressNumber++;
        break;
    case 2:

        if(sendFlag==0)
        {
            multiProgData();
            times++;
            if((times%5)==0)
            {
                progressBar->setValue(times*252>>10);
               // plainTextEdit->insertPlainText(QString("send :   %1  KB \n").arg(times*252.0/1024.0));
            }
        }
       break;
    case 3:
        startApplication();
        plainTextEdit->insertPlainText(tr("start Application ... ... \n"));
        progressNumber++;
        break;
    default:
        progressNumber=0;
        sendFlag=0;
        usart_uploadpushButton->setEnabled(true);
        progressBar->hide();
        //progressNumber is reset
        //progressBar is hideed
        // file is closed
        //sendFlag is reset
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
