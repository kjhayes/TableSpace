#ifndef TPRINT_HPP
#define TPRINT_HPP

#include<unordered_map>
#include<iostream>
#include<thread>
#include<mutex>
#include<string>

//oh no globals!!!... but... im lazy... and this was kinda quick...
extern std::unordered_map<std::thread::id, std::string> thread_msg_name_map;
extern std::mutex thread_msg_name_map_m;
extern int minimum_msg_priority;
extern int mut_msg_priority;
extern int pkt_msg_priority;
extern int cmd_msg_priority;
void TPrintln(std::string msg, int priority = 0, bool is_error = false, std::ostream& out = std::cout, std::ostream& err = std::cerr);

#endif