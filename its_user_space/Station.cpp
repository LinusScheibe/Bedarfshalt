#include <iostream>
#include <linux/socket.h>
#include <linux/gn.h>
#include <sys/timerfd.h>
#include <linux/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include "Station.h"

uint64_t Station::getGnAddress(const char* ifname){
	uint16_t flags = 0;
	uint64_t myMac = 0;
	struct ifreq ir, ir_mac;

	size_t if_name_len=strlen(ifname);
	if (if_name_len<sizeof(ir_mac.ifr_name)) {
		memcpy(ir_mac.ifr_name,ifname,if_name_len);
		ir_mac.ifr_name[if_name_len]=0;
	} else {
		printf("interface name is too long");
	}

	if (ioctl(gn_fd, SIOCGIFHWADDR, &ir_mac)==-1) {
		perror("ioctl failed");
		exit(EXIT_FAILURE);
	}
	const unsigned char* mac=(unsigned char*)ir_mac.ifr_hwaddr.sa_data;
	printf("%02X:%02X:%02X:%02X:%02X:%02X\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

	flags |= (1 << 15);
	flags |= (5 << 10);
	flags |= 49;

	for (int i = 0; i < 6; i++) {
		myMac |= ((uint64_t)ir_mac.ifr_hwaddr.sa_data[i] & 0xFF) << ((5 - i) * 8);
	}

	my_addr = myMac | ((uint64_t)flags & 0xFFFF) << 48;

	return my_addr;

}

void Station::init_gn_socket() {
	const char* ifname = "enp0s3";
	int rc = 0;

	struct ifreq ir;

	gn_fd = socket(45, SOCK_DGRAM, GN_PROTO_BTP_A);
	std::cout << "GN_FD(Konst) : " << gn_fd << std::endl;
	if (gn_fd <= 0) {
		printf("socket() failed and returned errno %s \n",strerror(errno));
		printf("creating receiving socket failed");
		exit(EXIT_FAILURE);
	}

	uint64_t myAddr = getGnAddress(ifname);

	gnrcv.sgn_family = 45;
	gnrcv.sgn_addr = myAddr;
	gnrcv.sgn_port = 10537;

	strcpy((char *)&ir.ifr_ifrn.ifrn_name, ifname);

	memcpy(&ir.ifr_ifru.ifru_addr, &gnrcv, sizeof(gnrcv));
	rc = ioctl(gn_fd, SIOCSIFADDR, (void *) &ir);

	rc = bind(gn_fd, (const struct sockaddr*)&gnrcv, sizeof(gnrcv));
	if (rc != 0) {
		printf("binding address to receiving socket failed");
		exit(EXIT_FAILURE);
	}
}

void Station::init_ui_socket() {
	struct sockaddr_in address;
	un_fd = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(10435);
	if (bind(un_fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(un_fd, 1) < 0)
	{
		perror("listen failed");
		exit(EXIT_FAILURE);
	}
	int addrlen = sizeof(address);
	std::cout << "Waiting for ui socket connection..." << std::endl;
	if ((ui_fd = accept(un_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
	{
		perror("accept failed");
		exit(EXIT_FAILURE);
	}
}

void Station::init_poll_fd_array() {

	//gn_timer_fd
	int gn_timer_fd =  timerfd_create(CLOCK_REALTIME, 0);

	if (gn_timer_fd == -1) {
		std::cout << "gn_timer_fd fehlgeschlagen (timerfd_create)" << std::endl;
		exit(EXIT_FAILURE);
	}

	struct timespec interval;
	struct timespec value;
	struct itimerspec timerspec;

	interval.tv_sec = 1;
	interval.tv_nsec = 0;

	value.tv_sec = 1;
	value.tv_nsec = 0;

	timerspec.it_interval = interval;
	timerspec.it_value = value;

	if (timerfd_settime(gn_timer_fd, 0, &timerspec, NULL) == -1) {
		std::cout << "set timer failed (timerfd_settime) with errno " << strerror(errno)  << std::endl;
		close(gn_timer_fd);
		exit(EXIT_FAILURE);
	}

	// timeout_timer_fd
	int timeout_timer_fd = timerfd_create(CLOCK_REALTIME, 0);

	if (timeout_timer_fd == -1) {
		std::cout << "timeout_timer_fd fehlgeschlagen (timerfd_create)" << std::endl;
		exit(EXIT_FAILURE);
	}

	interval.tv_sec = 10;
	interval.tv_nsec = 0;

	value.tv_sec = 1;
	value.tv_nsec = 0;

	timerspec.it_interval = interval;
	timerspec.it_value = value;

	if (timerfd_settime(timeout_timer_fd, 0, &timerspec, NULL) == -1) {
		std::cout << "set timeout_timer failed (timerfd_settime) with errno " << strerror(errno) << std::endl;
		close(timeout_timer_fd);
		exit(EXIT_FAILURE);
	}

	//fdarray
	short events = 0;

	events |= POLLIN;

	fdarray[0].fd = gn_fd;
	fdarray[0].events = events;
	fdarray[1].fd = ui_fd;
	fdarray[1].events = events;
	fdarray[2].fd = gn_timer_fd;
	fdarray[2].events = events;
	fdarray[3].fd = timeout_timer_fd;
	fdarray[3].events = events;
}

Station::Station(std::string name) : my_name(name)

{
	init_gn_socket();
	init_ui_socket();
	init_poll_fd_array();
}

void Station::sendToTrain(){

	json message_json = {
			{"lat", 0},
			{"lon", 0},
			{"gn_addr", my_addr},
			{"station_name", my_name},
			{"flag_stop_requested", flag_stop_requested}
	};

	std::string message_string = message_json.dump();
	size_t message_size = message_string.length();
	char sndbuf[message_size];
	strcpy(sndbuf, message_string.c_str());
	std::cout << "Sendbuff: " << sndbuf << std::endl;

	int rc = 0;

	struct gn_scope scope = {0};
	scope.scope_type = GN_SCOPE_TOPOLOGICAL;
	scope.topo_hops = 1;

	setsockopt(gn_fd, 284, GN_SCOPE, &scope, sizeof(scope));

	gndest.sgn_family = 45;
	gndest.sgn_addr = GNADDR_BROADCAST;
	gndest.sgn_port = 10537;
	rc = sendto(gn_fd, (void *)sndbuf, sizeof(sndbuf), MSG_DONTWAIT,
				(const struct sockaddr*) &gndest, sizeof(struct sockaddr_gn));
	if (rc < 0) {
		std::cout << "Sending answer failed and returned errno " << strerror(errno) << std::endl;
	}
	std::cout << "message sent" << std::endl;
}

void Station::checkForTimeout() {
	if(current_train_addr == 0) // if not connected to a train
		return;
	std::cout << "Check for timeout" << std::endl;
	struct timeval current_time;
	gettimeofday(&current_time, NULL);
	if(current_time.tv_sec - timestamp_last_message.tv_sec >= 10) {
		std::cout << "timed out" << std::endl;
		reset();
	}
}

void Station::reset(){
	flag_stop_requested = false;
	train_stop_status = false;
	last_train_addr = current_train_addr;
	current_train_addr = 0;
	json json_message_ui = {
			{"flag_stop_requested", false}
	};
	sendToUi(json_message_ui);
}

void Station::sendToUi(json message_json) {
	std::string message_string = message_json.dump();
	size_t message_size = message_string.length();
	char sndbuf[message_size+1];

	strcpy(sndbuf, message_string.c_str());
	send(ui_fd, sndbuf, sizeof(sndbuf), 0);
}

void Station::processGNMessage(char message[]) {
	std::cout << "Message received: " << message << std::endl;
	std::string string_message(message);

	json json_message = json::parse(string_message);
	json json_message_ui;

	if(json_message["gn_addr"] == last_train_addr){ // if message originates from previously connected train, discard
		return;
	} else {
		current_train_addr = json_message["gn_addr"];
	}

	gettimeofday(&timestamp_last_message, NULL);
	std::cout << "Timestamp_last_message seconds is: " << timestamp_last_message.tv_sec << std::endl;

	bool data_changed = false;
	if(json_message["train_stop_status"] == false){
		if(train_stop_status) { // this is new to me
			data_changed = true;
			json_message_ui["train_stop_status"] = false;
		}
	}

	if(data_changed)
		sendToUi(json_message_ui);

}

void Station::processUIMessage(char message[]) {
	std::string string_message(message);
	json json_message = json::parse(string_message);
	if (json_message["flag_stop_requested"] == true) { // stop button was pressed
		flag_stop_requested = true;
		train_stop_status = true;
	} else {  // cancel button was pressed
		flag_stop_requested = false;
		train_stop_status = false;
	}
}

int main(int argc, char** argv){
	Station s("Astadt");
	char readbuf[4096];
	while (1) {
		if(poll(s.fdarray, 4, 5000) > 0){
			for (int i = 0; i < 4; i++) {
				if(s.fdarray[i].revents & POLLIN){
					if(read(s.fdarray[i].fd, &readbuf, sizeof(readbuf)) >= 0) {
						switch(i){
							case 0: // gn_fd
								s.processGNMessage(readbuf);
								break;
							case 1: // ui_fd
								s.processUIMessage(readbuf);
								break;
							case 2:  // gn_timer_fd
								s.sendToTrain();
								break;
							case 3:  // timeout_timer_fd
								s.checkForTimeout();
								break;
							default: continue;

						}
					}
					memset(readbuf, 0, sizeof(readbuf));
				}
			}
		}
	}
	return 0;
}
