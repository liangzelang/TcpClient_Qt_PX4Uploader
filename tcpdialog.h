#ifndef TCPDIALOG_H
#define TCPDIALOG_H
#include "ui_tcpdialog.h"
#include <QDialog>
#include <QTcpSocket>
#include <QFile>
#include <QtSerialPort/QSerialPort>


class TcpDialog : public QDialog ,public Ui::TcpDialog
{
    Q_OBJECT

public:
    explicit TcpDialog(QWidget *parent = 0);
    ~TcpDialog();
     QString fileName;
     quint8 sendFlag;
     quint64 times;
     quint64 progressNumber=0;
     void fillPortsParameters();
     void fillPortsNames();
     void startSerialPort();
     void closeSerialPort();
    void enterBootloader();
    void eraseChip();
    void multiProgData();
   // void uploadFile();

     struct Settings {
         QString name;
         qint32 baudRate;
         QString stringBaudRate;
         QSerialPort::DataBits dataBits;
         QString stringDataBits;
         QSerialPort::Parity parity;
         QString stringParity;
         QSerialPort::StopBits stopBits;
         QString stringStopBits;
         QSerialPort::FlowControl flowControl;
         QString stringFlowControl;
     };

     Settings settings() const;


signals:
    void haveOpenedFile();
    void sendAckToServer();
    void gotSerialData();

private slots:
    void browseFile();
    void readFile();
    void connectToServer();
    void closeConnection();
    void clearReceiveBuf();
    void connectedToServer();
    void disconnectedToServer();
    void connectionError();
    void sendMessageToServer();
    void readFromServer();
    void returnAck();
    void clientReady();
    void sendEraseData();
    void refreshSerialPorts();
    void toggleSerialPort();
    void startApplication();
    void uploadFile();
    void readSerialData();

private:
    QTcpSocket tcpSocket ;
    QFile file;
    QSerialPort *serial;
};

#endif // TCPDIALOG_H
