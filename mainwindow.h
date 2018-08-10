#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <natpunch.h>

namespace Ui {
class MainWindow;
}

void displayData(void *data, void *args);

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    friend void displayData(void *data, void *args);
protected:
    void mousePressEvent(QMouseEvent* event);

    void on_Data_recv();

signals:
    void mousePressed();
    void signal_dataRecv();

private slots:
    void onMousePressed();

    void on_startConnection_clicked();

    void on_clear_clicked();

    void on_send_clicked();


    void on_SignaldataRecv();

    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;
    Natpunch *natpunch;
    std::string buf;//buffer for storing data
    bool hexFlag = false;
    bool waitingForRemoteClientInfo = false;
};

#endif // MAINWINDOW_H
