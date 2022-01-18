#include<thread>

#include "client_main.hpp"
#include "server_main.hpp"

int main(int argc, const char** argv){
    constexpr bool host = 
    #if defined(NO_HOST)
        false
    #else
        true
    #endif
    ;
    constexpr bool play = 
    #if defined(NO_CLIENT)
        false
    #else
        true
    #endif
    ;

    std::stop_token s;

    if constexpr (host && play){
        std::jthread server_thread(server_main, argc, argv);
        return client_main(s, argc, argv, true);
    }
    else if constexpr (play) {
        return client_main(s, argc, argv);
    }
    else {
        return server_main(s, argc, argv);
    }
}
