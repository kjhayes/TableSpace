#ifndef CLIENT_MAIN_HPP
#define CLIENT_MAIN_HPP

#include<stop_token>

int client_main(std::stop_token stoken, int argc, const char** argv, bool local_host = false);

#endif