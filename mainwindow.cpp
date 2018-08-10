#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <string>
#include <QMessageBox>
using namespace std;
using namespace std::placeholders;

void displayData(void *data, void *args)
{
    MainWindow* mainwindow = (MainWindow*) args;

    mainwindow->buf.clear();
    mainwindow->buf = *(string *)data;
    mainwindow->on_Data_recv();//send signal
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //ui->textEdit_2->installEventFilter(this);
    natpunch = new Natpunch;
    connect(this,SIGNAL(signal_dataRecv()),this,SLOT(on_SignaldataRecv()));
    //connect(this,SIGNAL(mousePressed()),this, SLOT(onMousePressed()));
}

MainWindow::~MainWindow()
{
    disconnect(this,SIGNAL(signal_dataRecv()),this,SLOT(on_SignaldataRecv()));
    //disconnect(this,SIGNAL(mousePressed()),this,SLOT(onMousePressed()));
    delete natpunch;
    delete ui;
}

void MainWindow::on_Data_recv()
{
    emit signal_dataRecv();
}


void MainWindow::onMousePressed()
{
    std::cout<<" Mainwindow::onMousePressed";
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);

    emit mousePressed();
}

void MainWindow::on_startConnection_clicked()
{
    if(!(natpunch->ifRemoteClientInfoGet() && natpunch->ifSelfInfoGet()))
    {
        std::cerr<<"on_startConnection return"<<std::endl;
        return;
    }
    natpunch->closeConnection();
    string STUNServer_domain = ui->stun_domainEdit->text().toStdString();
    int STUNServer_port = ui->stun_portEdit->text().toInt();
    string signalServerIp = ui->signal_ipEdit->text().toStdString();
    int signalServerPort = ui->signal_portEdit->text().toInt();
    natpunch->setSTUNServerDomain(STUNServer_domain);
    natpunch->setSTUNServerPort(STUNServer_port);
    natpunch->setHeartServerDomain(signalServerIp);
    natpunch->setHeartServerPort(signalServerPort);
    /*ui->textEdit->setText(STUNServer_domain.c_str());
    ui->textEdit->append(QString::number(STUNServer_port));
    ui->textEdit->append(signalServerIp.c_str());
    ui->textEdit->append(QString::number(signalServerPort));*/
    int ret = 0;
    natpunch->initialWork();
    ret = natpunch->clientConnect();
    while(!natpunch->ifSelfInfoGet())
    {
        QCoreApplication::processEvents();
    }
    if(natpunch->getIfTimeout() || !natpunch->getErrorMsg().empty())
    {
        QString errmsg;
        if(natpunch->getIfTimeout())
            errmsg = "Recv Data From StunServer Timeout";
        else
            errmsg = natpunch->getErrorMsg().c_str();
        QMessageBox::information(NULL, "information", errmsg,QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    ret = natpunch->sendMsgToSigalServer();
    if(ret < 0)
    {
        QMessageBox::information(NULL, "information", QString(natpunch->getErrorMsg().c_str()),QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    ret = natpunch->RecvMsgFromSigalServer();
    while(!natpunch->ifRemoteClientInfoGet())
    {
        QCoreApplication::processEvents();
    }
    if(natpunch->getIfTimeout() || !natpunch->getErrorMsg().empty())
    {
        QString errmsg;
        if(natpunch->getIfTimeout())
            errmsg = "Recv Data From SignalServer Timeout";
        else
            errmsg = natpunch->getErrorMsg().c_str();
        QMessageBox::information(NULL, "information", errmsg,QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    ret = natpunch->startNatPunch();
    if(ret < 0)
    {
        QMessageBox::information(NULL, "information", QString(natpunch->getErrorMsg().c_str()),QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    function<void (void *)> func = bind(displayData,_1,this);
    natpunch->setCallbackFunc(func);
    natpunch->startRecvData();
    ui->textEdit->clear();
    ui->textEdit_2->clear();
}

void MainWindow::on_SignaldataRecv()
{
    QString QRecvData = "remote: ";
    if(!hexFlag)
    {
        QRecvData += buf.c_str();
        ui->textEdit->append(QRecvData);
    }
    else
    {
        QString strDis;
        QString str2(buf.c_str());
        QString str3 = str2.toLatin1().toHex();//以十六进制显示
        str3 = str3.toUpper ();//转换为大写
        for(int i = 0;i<str3.length ();i+=2)//填加空格
        {
            QString st = str3.mid (i,2);
            strDis += st;
            strDis += " ";
        }
        QRecvData += strDis;
        ui->textEdit->append(QRecvData);
    }
}

void MainWindow::on_clear_clicked()
{
    ui->textEdit->clear();
}

void MainWindow::on_send_clicked()
{
    const string send_data = ui->textEdit_2->toPlainText().toStdString();
    natpunch->sendData(send_data);
    ui->textEdit_2->clear();
    QString QSendData = "me: ";
    QSendData += send_data.c_str();
    ui->textEdit->append(QSendData);
}

void MainWindow::on_pushButton_clicked()
{
    hexFlag = !hexFlag;
}
