#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDataStream>
#include <QMovie>
#include <QtNetwork/QLocalSocket>
#include <QSocketNotifier>
#include <QString>


namespace Ui {
class MainWindow;
}

class QLocalSocket;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_stopButton_clicked();
    void on_cancelButton_clicked();
    void readMessage();
    void reset();

private:
    Ui::MainWindow *ui;
    QLocalSocket *socket;
    QSocketNotifier *notifier;
    QDataStream in;
    int socket_fd;
};

#endif // MAINWINDOW_H
