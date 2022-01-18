#include "tprint.hpp"

std::unordered_map<std::thread::id, std::string> thread_msg_name_map;
std::mutex thread_msg_name_map_m;
int minimum_msg_priority = -2;
int mut_msg_priority = -3;
int pkt_msg_priority = -2;
int cmd_msg_priority = 256;
void TPrintln(std::string msg, int priority, bool is_error, std::ostream& out, std::ostream& err) {
    static std::mutex tprint_mut;
    std::lock_guard lock(tprint_mut);
    if(is_error || priority >= minimum_msg_priority){
        std::thread::id id = std::this_thread::get_id();
        std::string thread_name = thread_msg_name_map.contains(id) ? "["+thread_msg_name_map.at(id)+"] " : "[Unknown Thread] ";
        if(!is_error){
            err << thread_name << msg << std::endl;
        } else {
            out << thread_name << msg << std::endl;
        }
    }
}
