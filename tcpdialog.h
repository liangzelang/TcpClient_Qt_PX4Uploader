#ifndef TCPDIALOG_H
#define TCPDIALOG_H
#include "ui_tcpdialog.h"
#include <QDialog>
#include <QTcpSocket>
#include <QFile>
#include <QtSerialPort/QSerialPort>
#include <QProcess>

class TcpDialog : public QDialog ,public Ui::TcpDialog
{
    Q_OBJECT

public:
    explicit TcpDialog(QWidget *parent = 0);
    ~TcpDialog();
    //QProcess *process;
    QString fileName;
    quint8 sendFlag;
    quint64 times;
    quint64 progressNumber=0;
    quint8 unconnect_flag;
    void fillPortsParameters();
    void fillPortsNames();
    void startSerialPort();
    void closeSerialPort();
    void enterBootloader();
    void eraseChip();
    void multiProgData();
    void buttonInit();
    void startApplication();
    void cleanVariables();
    void loadData(char* binArray, qint64 *length);
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
    void sendCancelAckToServer();
    void gotSerialData();
    void wifi_fileOpened();
    void usart_fileOpened();
    void sendUnconnectAckToServer();
    void remainReadyRead();

private slots:
    void browseFile();
    void browseBLFile();
    void readFile();
    void connectToServer();
    void closeConnection();
    void onUnconnectionClicked();
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
    void bl_eraseChip();
    void bl_uploadFile();
    void uploadFile();
    void readSerialData();
    void wifi_openFile();
    void usart_openFile();
    void showData();
    void processStarted();
    void processError();
    void killProcess();
    void showHelp();
    void uploadusbFile();
    void returnCancelAck();
    void returnUnconnectAck();
    void setScrollPosition();
protected:
    void closeEvent(QCloseEvent *);
private:
    QTcpSocket tcpSocket ;
    QFile file;
    QSerialPort *serial;
    QProcess *process;
};

#endif // TCPDIALOG_H
