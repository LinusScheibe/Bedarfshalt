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

#define CONNECTED "Verbunden mit: "
#define NOT_CONNECTED "Nicht verbunden."
#define FLAG_STOP_REQUESTED "Haltewunsch wurde empfangen."
#define FLAG_STOP_NOT_REQUESTED "Kein Haltewunsch empfangen."
#define FLAG_STOP_DECLINED "Haltewunsch wurde abgelehnt."
#define STYLESHEET_FLAG_STOP_REQUESTED "QLabel { color: red; text-decoration: underline}"
#define STYLESHEET_FLAG_STOP_NOT_REQUESTED "QLabel { color: black; text-decoration: none}"
#define PORTNR 10536
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    socket_fd(0)
{
    ui->setupUi(this);
    this->setWindowIcon(QIcon(":images/train.png"));
    ui->declineButton->setEnabled(false);

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

    QLabel label_ne5;
    QMovie *movie = new QMovie(":images/Ne5blink_small.gif");

    ui->label_ne5->setMovie(movie);
    ui->label_ne5->hide();
}

MainWindow::~MainWindow()
{
    delete ui;
    ::close(socket_fd);
}

void MainWindow::readMessage(){
    char buf[4096];
    read(this->socket_fd, buf, sizeof(buf));
    std::cout << "Received: " << buf << std::endl << std::endl;
    std::string string_message(buf);
    json json_message = json::parse(string_message);

    if(json_message.contains("station_name")){ // Connection to station established
        ui->label_connected_to->setText(CONNECTED);
        std::string station_name = json_message["station_name"];
        QString qstation_name(station_name.c_str());
        ui->value_station_name->setText(qstation_name);
    }

    if(json_message.contains("flag_stop_requested")){
        if(json_message["flag_stop_requested"] == true){
            ui->value_flag_stop->setText(FLAG_STOP_REQUESTED);
            ui->value_flag_stop->setStyleSheet(STYLESHEET_FLAG_STOP_REQUESTED);
            ui->declineButton->setEnabled(true);
            ui->label_ne5->show();
            ui->label_ne5->movie()->start();
            ui->label_ne5->movie()->stop();
            ui->label_ne5->movie()->jumpToFrame(1);
        } else {
            ui->value_flag_stop->setText(FLAG_STOP_NOT_REQUESTED);
            ui->value_flag_stop->setStyleSheet(STYLESHEET_FLAG_STOP_NOT_REQUESTED);
            ui->declineButton->setEnabled(false);
            ui->label_ne5->movie()->stop();
            ui->label_ne5->hide();
        }
    }

    if(json_message.contains("train_stop_status")){
        if(json_message["train_stop_status"] == false){ // reset everything
            reset();
        }
    }
}

void MainWindow::reset(){


    ui->value_flag_stop->setText(FLAG_STOP_NOT_REQUESTED);
    ui->value_station_name->setText("");
    ui->label_connected_to->setText(NOT_CONNECTED);
    ui->value_flag_stop->setStyleSheet(STYLESHEET_FLAG_STOP_NOT_REQUESTED);
    ui->label_ne5->movie()->stop();
    ui->label_ne5->hide();
}

void MainWindow::on_declineButton_clicked()
{
    json message_json = {
        {"train_stop_status", false}
    };

    std::string message_string = message_json.dump();

    size_t message_size = message_string.length();
    char sendbuf[message_size];

    strcpy(sendbuf, message_string.c_str());

    write(this->socket_fd, sendbuf, sizeof(sendbuf));

    ui->value_flag_stop->setText(FLAG_STOP_DECLINED);
    ui->declineButton->setEnabled(false);
    ui->label_ne5->movie()->stop();
    ui->label_ne5->hide();
}
