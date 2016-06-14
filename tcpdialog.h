#ifndef TCPDIALOG_H
#define TCPDIALOG_H
#include "ui_tcpdialog.h"
#include <QDialog>
#include <QTcpSocket>
#include <QFile>
/*
namespace Ui {
class TcpDialog;
}*/

class TcpDialog : public QDialog ,public Ui::TcpDialog
{
    Q_OBJECT

public:
    explicit TcpDialog(QWidget *parent = 0);
    ~TcpDialog();
     QString fileName;
     void clientReady();
     quint8 sendFlag;
     quint64 times;
signals:
    void haveOpenedFile();
    void sendAckToServer();

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

private:
    //Ui::TcpDialog *ui;
    QTcpSocket tcpSocket ;
    QFile file;
};

#endif // TCPDIALOG_H
