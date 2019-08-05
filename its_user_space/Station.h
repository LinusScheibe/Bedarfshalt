#pragma once

#include "json.hpp"
using json = nlohmann::json;

class Station {
private:
	int gn_fd = 0, un_fd = 0, ui_fd = 0;
	uint64_t my_addr = 0, current_train_addr = 0, last_train_addr = 0;
	std::string my_name;
	struct timeval timestamp_last_message;
	bool flag_stop_requested = false;
	bool train_stop_status = false;
	struct sockaddr_gn gnrcv = {0}, gndest = {0};

	uint64_t getGnAddress(const char* ifname);
	void init_gn_socket();
	void init_ui_socket();
	void init_poll_fd_array();
	void reset();
	void sendToUi(json message_json);

public:
	struct pollfd fdarray[4];

	Station(std::string name);
	void processGNMessage(char message[4096]);
	void processUIMessage(char message[4096]);
	void sendToTrain();
	void checkForTimeout();
};
