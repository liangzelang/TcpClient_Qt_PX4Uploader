#ifndef TCPDIALOG_H
#define TCPDIALOG_H
#include "ui_tcpdialog.h"
#include <QDialog>
#include <QTcpSocket>
//#include <QFile>
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
signals:
    void haveOpenedFile(QFile &file);

private slots:
    void browseFile();
    void readFile(QFile &file);
    void connectToServer();
    void closeConnection();
    void clearReceiveBuf();
    void connectedToServer();
    void disconnectedToServer();
    void connectionError();
    void sendMessageToServer();
    void readFromServer();

private:
    //Ui::TcpDialog *ui;
    QTcpSocket tcpSocket ;
    //QFile file;
};

#endif // TCPDIALOG_H
