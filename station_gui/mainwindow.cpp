#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "json.hpp"
using json = nlohmann::json;

#define TRAIN_STOP_STATUS_WILL_STOP "Bedarfshalt wurde angefordert."
#define TRAIN_STOP_STATUS_WILL_NOT_STOP "Bedarfshalt wurde abgelehnt."
#define STYLESHEET_LABEL_TRAIN_WILL_STOP "QLabel { color: #aacea1; text-decoration: bold}"
#define STYLESHEET_LABEL_TRAIN_WILL_NOT_STOP "QLabel { color: #f36b2c; text-decoration: bold}"
#define PORTNR 10436

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    socket_fd(0)
{
    ui->setupUi(this);

    this->setWindowIcon(QIcon(":images/ne5.png"));
    struct sockaddr_in qemuAddress;
    qemuAddress.sin_family = AF_INET;
    qemuAddress.sin_port = htons(PORTNR);
    
    if(inet_pton(AF_INET, "127.0.0.1", &qemuAddress.sin_addr) < 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    
    this->socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (this->socket_fd == -1){
        perror("Create an ip socket");
        exit(EXIT_FAILURE);
    }

    int err = ::connect(this->socket_fd, (struct sockaddr *)&qemuAddress, sizeof(qemuAddress));
    if (err != 0){
        perror("Connect to ip socket");
        exit(EXIT_FAILURE);
    }

    notifier = new QSocketNotifier(this->socket_fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &MainWindow::readMessage);

    ui->cancelButton->setEnabled(false);

    QLabel label_raise_arm;
    QMovie *movie = new QMovie(":images/raise_arm.png");
    ui->label_raise_arm->setMovie(movie);
    ui->label_raise_arm->movie()->start();
    ui->label_raise_arm->hide();
}

MainWindow::~MainWindow()
{
    delete ui;
    ::close(socket_fd);
}

void MainWindow::readMessage(){
    char buf[1024];
    read(this->socket_fd, buf, sizeof(buf));

    std::string string_message = buf;
    json json_message = json::parse(string_message);

    if(json_message.contains("train_stop_status")) {
        if(json_message["train_stop_status"] == false){
            ui->label_train_stop_status->setStyleSheet(STYLESHEET_LABEL_TRAIN_WILL_NOT_STOP);
            ui->label_train_stop_status->setText(TRAIN_STOP_STATUS_WILL_NOT_STOP);
            ui->label_raise_arm->hide();
        }
    }
    if(json_message.contains("flag_stop_requested")) {
        if(json_message["flag_stop_requested"] == false){ // reset everything
            reset();
        }
    }
}
void MainWindow::reset(){
    ui->stopButton->setEnabled(true);
    ui->cancelButton->setEnabled(false);
    ui->label_train_stop_status->setText("");
    ui->label_raise_arm->hide();
}

void MainWindow::on_stopButton_clicked()
{
    // set flag_stop_requested to true

    ui->stopButton->setEnabled(false);
    ui->cancelButton->setEnabled(true);
    ui->label_train_stop_status->setStyleSheet(STYLESHEET_LABEL_TRAIN_WILL_STOP);
    ui->label_train_stop_status->setText(TRAIN_STOP_STATUS_WILL_STOP);


    json message_json = {
        {"flag_stop_requested", true}
    };

    std::string message_string = message_json.dump();

    size_t message_size = message_string.length();
    char sendbuf[message_size];

    strcpy(sendbuf, message_string.c_str());

    write(this->socket_fd, sendbuf, sizeof(sendbuf));

    ui->label_raise_arm->show();
}

void MainWindow::on_cancelButton_clicked()
{
    json message_json = {
        {"flag_stop_requested", false}
    };

    std::string message_string = message_json.dump();

    size_t message_size = message_string.length();
    char sendbuf[message_size];

    strcpy(sendbuf, message_string.c_str());

    write(this->socket_fd, sendbuf, sizeof(sendbuf));

    ui->stopButton->setEnabled(true);
    ui->cancelButton->setEnabled(false);
    ui->label_train_stop_status->setText("");
    ui->label_raise_arm->hide();
}
