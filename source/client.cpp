#include "client_main.hpp"

#include<iostream>
#include<thread>
#include<stop_token>
#include<mutex>
#include<functional>
#include<chrono>

#include<SFML/Window.hpp>
#include<SFML/Graphics.hpp>
#include<SFML/Network.hpp>

#include "tprint.hpp"
#include "pkt_headers.hpp"
#include "object.hpp"

using namespace std::literals::string_literals;

static Table table;

void GraphicsThread(std::stop_token stoken, sf::RenderWindow& target, std::mutex& target_m) {
    {//Set Thread Name
        std::lock_guard thread_msg_name_map_lock(thread_msg_name_map_m);
        thread_msg_name_map.insert_or_assign(std::this_thread::get_id(), "Graphics");
    }
    target.setActive(true);
    while(!stoken.stop_requested()){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard target_lock(target_m);
        target.clear({100,50,100});
        {
            std::lock_guard obj_lock(table.objects_m);
            for(auto iter = table.objects.begin(); iter != table.objects.end(); iter++){
                iter->second->Draw(target);
            }
        }
        target.display();
    }
    TPrintln("Stopping...", -1);
}

void ProcessPacket(sf::Packet& pkt, sf::TcpSocket& server, std::mutex& server_m){
    PktHeaderUnderlying header;
    if(pkt >> header) {
        
        TPrintln("Received Packet With Header "s + std::to_string(header) + " From Server", pkt_msg_priority);
        uint64_t index;
        sf::Vector2f position;
        std::string asset_name;
        switch(header){
            case PktHeader::SetObj:
                if(pkt >> index >> position.x >> position.y){
                    if(table.objects.contains(index)){
                        TPrintln("Setting Object");
                        table.SetObj(index, position);
                    } else {
                        TPrintln("Creating Object");
                        table.c_CreateObj(index, position);
                    }
                } else {
                    TPrintln("Received Erroneous SetObj Packet From The Server!", 0, true);
                }
                break;
            case PktHeader::DeleteObj:
                if(pkt >> index){
                    table.DeleteObj(index);
                } else {
                    TPrintln("Received Erroneous DeleteObj Packet From The Server!", 0, true);
                }
                break;
            case PktHeader::AssetObj:
                if(pkt >> index >> asset_name) {
                    if(table.objects.contains(index)){
                        table.objects.at(index)->SetAsset(asset_name);
                    } else {
                        TPrintln("Requesting Server Lock", mut_msg_priority);
                        std::lock_guard server_lock(server_m);
                        TPrintln("Received Server Lock", mut_msg_priority);
                        sf::Packet pkt;
                        pkt << PktHeader::QueryObj << index;
                        server.send(pkt);
                        TPrintln("Released Server Lock", mut_msg_priority);
                    }
                } else {
                    TPrintln("Received Erroneous AssetObj Packet From The Server!", 0, true);
                }
                break;
            case PktHeader::QueryObj:
                if(pkt >> index){
                    table.DeleteObj(index);
                    TPrintln("Deleted Obj Which Server Had To Query!", 0, true);
                } else {
                    TPrintln("Received Erroneous QueryObj Packet From The Server!", 0, true);
                }
                break;
            case PktHeader::SetUser:
                TPrintln("Received SetUser Packet");
                break;
            case PktHeader::SetUserColor:
                TPrintln("Received SetUserColor Packet");
                break;
            default:
                TPrintln("Received Unknown Packet Header From The Server!", 0, true);
        }
        if(!pkt.endOfPacket()){
            ProcessPacket(pkt, server, server_m);
        }
    } else {
        TPrintln("Received Erroneous Packet Header From The Server!", 0, true);
    }
}

void NetworkThread(std::stop_token stoken, sf::TcpSocket& server, std::mutex& server_m) {
    {//Set Thread Name
        std::lock_guard thread_msg_name_map_lock(thread_msg_name_map_m);
        thread_msg_name_map.insert_or_assign(std::this_thread::get_id(), "Client");
    }

    sf::SocketSelector server_selector;

    {
    std::lock_guard server_lock(server_m);
    server_selector.add(server);
    
    sf::Packet query;
    query << PktHeader::QueryObj << (uint64_t)0;
    server.send(query);
    }

    while(!stoken.stop_requested()){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(server_selector.wait(sf::milliseconds(10)) && !stoken.stop_requested()){
        TPrintln("Asking For Server Lock", mut_msg_priority-1);
        std::lock_guard server_lock(server_m);
        TPrintln("Received Server Lock", mut_msg_priority-1);
        TPrintln("Asking For Objects Lock", mut_msg_priority-1);
        std::lock_guard objects_lock(table.objects_m);
        TPrintln("Received Objects Lock", mut_msg_priority-1);
        if(server_selector.isReady(server)){
            
            sf::Packet pkt;
            if(server.receive(pkt) == sf::Socket::Disconnected){
                TPrintln("Server Disconnected!", 0, true);
                break;
            }
            ProcessPacket(pkt, server, server_m);

        } else {
            TPrintln("Error Awaiting Message!", 0, true);
        }
        }
        TPrintln("Released Objects Lock", mut_msg_priority-1);
        TPrintln("Released Server Lock", mut_msg_priority-1);
    }
    TPrintln("Stopping...", -1);
}

int client_main(std::stop_token stoken, int argc, const char** argv, bool local_host) {
    {//Set Thread Name
        std::lock_guard thread_msg_name_map_lock(thread_msg_name_map_m);
        thread_msg_name_map.insert_or_assign(std::this_thread::get_id(), "Connector");
    }
    sf::IpAddress server_ip;
    uint16_t server_port;
    
    if (!local_host){
        std::cout << "Server Ip: ";
        std::cin >> server_ip;
        std::cout << "Server Port: ";
        std::cin >> server_port;
    } else {
        server_ip = sf::IpAddress::getLocalAddress();
        server_port = 7777;
    } 

    std::mutex server_m;
    sf::TcpSocket server;

    {
    std::lock_guard server_lock(server_m);

    sf::Socket::Status status = server.connect(server_ip, server_port, sf::seconds(5.0f));
    if(status != sf::Socket::Done){
        TPrintln(std::string("Could Not Connect To Server ")+server_ip.toString()+std::string(" On Port ")+std::to_string(server_port), 0, true);
        std::cin.get();
        return -1;
    } else {
        TPrintln("Connected!", -1);
    }
    
    }

    {//Reset Thread Name
        std::lock_guard thread_msg_name_map_lock(thread_msg_name_map_m);
        thread_msg_name_map.insert_or_assign(std::this_thread::get_id(), "Window");
    }

    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), std::string("TableSpace - ")+server_ip.toString());
    std::mutex window_m;
    window.setActive(false);

    std::jthread graphics_thread(GraphicsThread, std::ref(window), std::ref(window_m));
    std::jthread network_thread(NetworkThread, std::ref(server), std::ref(server_m));

    sf::Event e;
    while(!stoken.stop_requested() && window.isOpen()){
        window.pollEvent(e);
        switch(e.type){
            case sf::Event::Closed:
                window.close();
                break;
            case sf::Event::KeyPressed:
                if(e.key.code == sf::Keyboard::T){
                    TPrintln("Asking For Server Lock", mut_msg_priority);
                    std::lock_guard server_lock(server_m);
                    TPrintln("Received Server Lock", mut_msg_priority);
                    sf::Packet pkt;
                    sf::Vector2f pos;
                    {
                        TPrintln("Asking For Window Lock", mut_msg_priority);
                        std::lock_guard window_lock(window_m);
                        TPrintln("Received Window Lock", mut_msg_priority);
                        pos = window.mapPixelToCoords(sf::Mouse::getPosition(window), window.getView());
                        TPrintln("Pos: "s + std::to_string(pos.x) + " "s + std::to_string(pos.y));
                        TPrintln("Released Window Lock", mut_msg_priority);
                    }
                    pkt << PktHeader::SetObj << (uint64_t)0 << pos.x << pos.y;
                    server.send(pkt);
                } else if(e.key.code == sf::Keyboard::O){
                    TPrintln("Asking For Objects Lock", mut_msg_priority);
                    std::lock_guard objects_lock(table.objects_m);
                    TPrintln("Received Objects Lock", mut_msg_priority);
                    TPrintln("Table Objects:", cmd_msg_priority);
                    for(auto iter = table.objects.begin(); iter != table.objects.end(); iter++){
                        TPrintln("\t"s + iter->second->ToString(), cmd_msg_priority);
                    }
                    TPrintln("Released Objects Lock", mut_msg_priority);
                } else if(e.key.code == sf::Keyboard::R){
                    TPrintln("Asking For Server Lock", mut_msg_priority);
                    std::lock_guard server_lock(server_m);
                    TPrintln("Received Server Lock", mut_msg_priority);
                    sf::Packet query;
                    query << PktHeader::QueryObj << (uint64_t)0;
                    server.send(query);
                    TPrintln("Released Server Lock", mut_msg_priority);
                }
                TPrintln("Released Server Lock", mut_msg_priority);
                break;
        }
    }
    
    TPrintln("Stopping...", -1);
    
    return 0;
}