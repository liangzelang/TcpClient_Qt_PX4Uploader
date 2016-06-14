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

TcpDialog::TcpDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi(this);
    sendFlag=0;
    times=0;
    connectpushButton->setEnabled(true);
    unconnectpushButton->setEnabled(false);
    progressBar->hide();
    iplineEdit->setPlaceholderText(tr("192.168.4.1"));
    portlineEdit->setPlaceholderText("8086");
    //progressBar->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Ignored);
    connect(browsepushButton,SIGNAL(clicked(bool)),this,SLOT(browseFile()));
    connect(this,SIGNAL(haveOpenedFile()),this,SLOT(readFile()));
    connect(connectpushButton,SIGNAL(clicked(bool)),this,SLOT(connectToServer()));
    connect(unconnectpushButton,SIGNAL(clicked(bool)),this,SLOT(closeConnection()));
    connect(clearpushButton,SIGNAL(clicked(bool)),this,SLOT(clearReceiveBuf()));
    connect(&tcpSocket,SIGNAL(connected()),this,SLOT(connectedToServer()));
    connect(&tcpSocket,SIGNAL(readyRead()),this,SLOT(readFromServer()));
    connect(&tcpSocket,SIGNAL(disconnected()),this,SLOT(disconnectedToServer()));
    connect(&tcpSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(connectionError()));
    connect(uploadpushButton,SIGNAL(clicked(bool)),this,SLOT(readFile()));
    connect(this,SIGNAL(sendAckToServer()),this,SLOT(returnAck()));

}

TcpDialog::~TcpDialog()
{
    //delete tcpSocket;
    //delete ui;
}

void TcpDialog::browseFile()
{

    QString initialName = lineEdit->text();     //default get the path from the lineEdit component
    if(initialName.isEmpty())                            //if the default path is empty ,I set the home path as the default
            initialName = QDir::homePath();
    fileName =QFileDialog::getOpenFileName(this,tr("choose file"),initialName);  //at the path:initialName ,user choose which file to upload
    fileName =QDir::toNativeSeparators(fileName);
    lineEdit->setText(fileName);
    plainTextEdit->insertPlainText(fileName);
    if(!fileName.isEmpty())
    {
         file.setFileName(fileName);

        if(!file.open(QIODevice::ReadOnly))
        {
            qDebug()<<"can't open the file!"<<endl;
        }

        else
        {
            plainTextEdit->insertPlainText(tr("\n this File is opened , send signal 0f ready to Server \n"));
            //TO DO
            //Add the Function  :
            clientReady();
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
   // binArray[2+length]=0x20;
    if((length<252)||(file.atEnd()))
        binArray[0]=0x02;
    tcpSocket.write(binArray,(length+2));
    //times++;
     if(file.atEnd())
     {
         plainTextEdit->insertPlainText(tr("the last length is %1 \n").arg(length));
         plainTextEdit->insertPlainText(QByteArray(binArray).toHex());
        file.close();
        sendFlag=1;
       // emit
     }
}

void TcpDialog::connectToServer()
{
    //QHostAddress  ipAdress = iplineEdit->text();
    if((!iplineEdit->text().isEmpty())&&(!portlineEdit->text().isEmpty()))
    {
        QString ipAdress = iplineEdit->text();
         quint64 port = portlineEdit->text().toInt();
         //quint64 port = 8086;
        tcpSocket.connectToHost(ipAdress,port);
        connectpushButton->setEnabled(false);
        unconnectpushButton->setEnabled(true);
        plainTextEdit->insertPlainText(tr("connecting ......\n"));
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
    tcpSocket.close();
    connectpushButton->setEnabled(true);
    unconnectpushButton->setEnabled(false);
    plainTextEdit->insertPlainText(tr("close the current Tcp connection. \n"));
    progressBar->hide();
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
    QByteArray bin("what is fuck");
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
    QByteArray str =  tcpSocket.readAll();
    char *echo=str.data();
    progressBar->show();
    if((echo[0]==0x01)&&(echo[1]==0x20))
    {
        //TO DO
        //   add signals of return the Ack
        plainTextEdit->insertPlainText("got the 0x01 and 0x20 \n");
        plainTextEdit->insertPlainText("return the Ack  \n");
        emit sendAckToServer();
        //TO DO
    }
    else if((echo[0]==0x02)&&(echo[1]==0x20))
    {
        //TO DO
        //   add signals of end the task
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
            readFile();
            times++;
            if(times%250==0)
            {
                plainTextEdit->insertPlainText(QString("send :   %1  KB \n").arg(times*252.0/1024.0));
            }
        }
        else if(sendFlag==1)
        {
           // return;
        }
    }
    else
    {
        plainTextEdit->insertPlainText(str.toHex());
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


