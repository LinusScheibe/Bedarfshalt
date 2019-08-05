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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_declineButton_clicked();

    void readMessage();
    void reset();

private:
    Ui::MainWindow *ui;
    QSocketNotifier *notifier;
    int socket_fd;

};

#endif // MAINWINDOW_H
