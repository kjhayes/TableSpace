#include "server_main.hpp"

#include<list>
#include<thread>
#include<stop_token>
#include<mutex>
#include<functional>
#include<string>
#include<regex>

#include<SFML/System.hpp>
#include<SFML/Network.hpp>

#include "tprint.hpp"
#include "object.hpp"
#include "pkt_headers.hpp"

using namespace std::literals::string_literals;

struct ServerInfo {
    ServerInfo(sf::IpAddress&& server_ip, sf::IpAddress&& local_ip, const uint16_t& port) : server_ip(server_ip), local_ip(local_ip), port(port) {}

    sf::IpAddress server_ip;
    sf::IpAddress local_ip;
    uint16_t port;
};

struct Connection {
    sf::TcpSocket* socket;
    std::string name;
    Connection(sf::TcpSocket* socket) : socket(socket) {
        name = std::string("User_")+std::to_string(std::hash<sf::TcpSocket*>()(socket));
    }
    ~Connection(){
        if(socket){delete socket;}
    }

    std::string ToString() {
        return "["s + socket->getRemoteAddress().toString() + " "s + name + "]";
    }
};

struct ClientPool {
    std::recursive_mutex connections_m;
    std::list<Connection> connections;

    std::mutex selector_m;
    sf::SocketSelector selector;

    Connection* GetConnectionFromName(const std::string& name) {
        std::lock_guard connections_lock(connections_m);
        auto iter = std::find_if(connections.begin(), connections.end(), [&name](Connection& conn){return conn.name == name;});
        if(iter == connections.end()){return nullptr;}
        return &(*iter);
    }
};

static const ServerInfo server_info(sf::IpAddress::getPublicAddress(), sf::IpAddress::getLocalAddress(), 7777);
static ClientPool client_pool;
static Table table;

void ListenThread(std::stop_token stoken) {
    {//Set Thread Name
        std::lock_guard thread_msg_name_map_lock(thread_msg_name_map_m);
        thread_msg_name_map.insert_or_assign(std::this_thread::get_id(), std::string("Server Listener"));
    }

    TPrintln("Running...", -1);
    sf::TcpListener listener;
    listener.listen(server_info.port);
    sf::TcpSocket* socket = nullptr;
    bool trash_socket = true;
    while(!stoken.stop_requested()){
        if(trash_socket || socket == nullptr) { socket = new sf::TcpSocket(); trash_socket = false; }
        if(listener.accept(*socket) == sf::Socket::Done) {
            trash_socket = true;
            TPrintln("Accepted Connection To " + socket->getRemoteAddress().toString(), 0);
            {
                std::lock_guard connections_lock(client_pool.connections_m);
                socket = client_pool.connections.emplace_back(socket).socket;
            }
            std::lock_guard selector_lock(client_pool.selector_m);
            client_pool.selector.add(*socket);
        }
    }
    TPrintln("Stopping...", -1);
}

void SendAll(sf::Packet& pkt){
    TPrintln("Asking For Connections Lock", mut_msg_priority);
    std::lock_guard connections_lock(client_pool.connections_m);
    TPrintln("Received Connections Lock", mut_msg_priority);
    for(auto iter = client_pool.connections.begin(); iter != client_pool.connections.end(); iter++){
        TPrintln("Sending Packet To Client "s + iter->ToString(), pkt_msg_priority);
        iter->socket->send(pkt);
    }
    TPrintln("Released Connections Lock", mut_msg_priority);
}
void SendIf(sf::Packet& pkt, std::function<bool(Connection&)> pred){
    TPrintln("Asking For Connections Lock", mut_msg_priority);
    std::lock_guard connections_lock(client_pool.connections_m);
    TPrintln("Received Connections Lock", mut_msg_priority);
    for(auto iter = client_pool.connections.begin(); iter != client_pool.connections.end(); iter++){
        if(pred(*iter)){
            TPrintln("Sending Packet To Client "s + iter->ToString(), pkt_msg_priority);
            iter->socket->send(pkt);
        }
    }
    TPrintln("Released Connections Lock", mut_msg_priority);
}
void SendTo(sf::Packet& pkt, Connection& connection){
    TPrintln("Asking For Connections Lock", mut_msg_priority);
    std::lock_guard connections_lock(client_pool.connections_m);
    TPrintln("Received Connections Lock", mut_msg_priority);
    TPrintln("Sending Packet To Client "s + connection.ToString(), pkt_msg_priority);
    connection.socket->send(pkt);
    TPrintln("Released Connections Lock", mut_msg_priority);
}
void SendQuery(uint64_t index, Connection& connection){
    sf::Packet query;
    query << PktHeader::QueryObj << index;
    SendTo(query, connection);
}

void ProcessPacket(sf::Packet& pkt, Connection& from) {
    PktHeaderUnderlying header;
    if(pkt >> header){
        
        uint64_t index;
        sf::Vector2f position;
        std::string asset_name;
                
        switch(header){
            case PktHeader::SetObj:
                if(pkt >> index >> position.x >> position.y){
                    if(index != 0){
                        if(table.SetObj(index, position))
                        {
                            sf::Packet setPkt;
                            table.objects.at(index)->CreateSetPkt(setPkt);
                            SendAll(setPkt);
                        } else {
                            SendQuery(index, from);
                        }
                    } else {
                        auto obj = table.s_CreateObj(position);
                        sf::Packet setPkt;
                        obj->CreateSetPkt(setPkt);
                        obj->CreateAssetPkt(setPkt);
                        SendAll(setPkt);
                    }
                } else {
                    TPrintln("Recieved Erroneous SetObj Packet From "s + from.ToString(), 0, true);
                }
                break;
            case PktHeader::DeleteObj:
                if(pkt >> index) {
                    if(table.DeleteObj(index)){
                        sf::Packet p;
                        p << PktHeader::DeleteObj << index;
                        SendAll(p);
                    } else {
                        SendQuery(index, from);
                    }
                } else {
                    TPrintln("Recieved Erroneous DeleteObj Packet From "s + from.ToString(), 0, true);
                }
                break;
            case PktHeader::AssetObj:
                if(pkt >> index >> asset_name) {
                    if(table.objects.contains(index)){
                        table.objects.at(index)->SetAsset(asset_name, true);
                        table.objects.at(index)->CreateAssetPkt(pkt);
                        SendAll(pkt);
                    } else {
                        SendQuery(index, from);
                    }
                } else {
                    TPrintln("Received Erroneous AssetObj Packet From "s + from.ToString(), 0, true);
                }
                break;
            case PktHeader::QueryObj:
                if(pkt >> index){
                    if(index == 0){
                        for(auto iter = table.objects.begin(); iter != table.objects.end(); iter++){
                            sf::Packet setPkt;
                            iter->second->CreateSetPkt(setPkt);
                            iter->second->CreateAssetPkt(setPkt);
                            SendTo(setPkt, from);
                        }
                    }
                    else if(table.objects.contains(index)){
                        sf::Packet setPkt; 
                        table.objects.at(index)->CreateSetPkt(setPkt);
                        table.objects.at(index)->CreateAssetPkt(setPkt);
                        SendTo(setPkt, from);
                    } else {
                        SendQuery(index, from);
                    }
                } else {
                    TPrintln("Received Erroneous QueryObj Packet From "s + from.ToString(), 0, true);
                }
                break;
            case PktHeader::SetUser:

                break;
            case PktHeader::SetUserColor:

                break;
        }
    }else{
        TPrintln("Recieved Packet With No Header From "s + from.ToString(), 0, true);
    }
}

void CmdLineThread(std::stop_token stoken){
    {//Set Thread Name
        std::lock_guard thread_msg_name_map_lock(thread_msg_name_map_m);
        thread_msg_name_map.insert_or_assign(std::this_thread::get_id(), "CmdLine");
    }
    std::string line;
    while(!stoken.stop_requested()){
        std::getline(std::cin, line);
        static std::regex rgx("[^ \n\r\t]+");
        std::sregex_token_iterator iter(line.begin(), line.end(), rgx);
        std::sregex_token_iterator end;

        if(line == "/obj") {
            TPrintln("Asking For Objects Lock", mut_msg_priority);
            std::lock_guard obj_lock(table.objects_m);
            TPrintln("Received Objects Lock", mut_msg_priority);
            TPrintln("Table Objects:", cmd_msg_priority);
            for(auto iter = table.objects.begin(); iter != table.objects.end(); iter++){
                TPrintln("\t"s + iter->second->ToString(), cmd_msg_priority);
            }
            TPrintln("Released Objects Lock", mut_msg_priority);
        }
        else if(iter != end && iter->str() == "/asset"){
            iter++;
            if(iter != end){
                uint64_t index = std::stoi(iter->str());
                iter++;
                if(iter != end){
                    std::string asset_name = iter->str();
                    TPrintln("Asking For Objects Lock", mut_msg_priority);
                    std::lock_guard obj_lock(table.objects_m);
                    TPrintln("Received Objects Lock", mut_msg_priority);
                    if(table.objects.contains(index)){
                        table.objects.at(index)->SetAsset(asset_name, true);
                        sf::Packet pkt;
                        table.objects.at(index)->CreateAssetPkt(pkt);
                        SendAll(pkt);
                    } else {
                        TPrintln("Cannot Find Object With Index : "s + std::to_string(index), 0, true);
                    }
                    TPrintln("Released Objects Lock", mut_msg_priority);
                }
            } else {
                TPrintln("/asset Needs an Index!", 0, true);
            }
        } else {
            TPrintln("Unknown Command!", 0, true);
        }
    }

    TPrintln("Stopping...", -1);
}

int server_main(std::stop_token stoken, int argc, const char** argv) {
    {//Set Thread Name
        std::lock_guard thread_msg_name_map_lock(thread_msg_name_map_m);
        thread_msg_name_map.insert_or_assign(std::this_thread::get_id(), "Server");
    }

    TPrintln("Launching Listener...", -1);
    std::jthread listen_thread(ListenThread);
    TPrintln("Listener Launched!", -1);

    TPrintln("Launching CmdLine...", -1);
    std::jthread cmdline_thread(CmdLineThread);
    TPrintln("CmdLine Launched!", -1);

    while(!stoken.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard selector_lock(client_pool.selector_m);
        if(client_pool.selector.wait(sf::seconds(1.0f)) && !stoken.stop_requested()){
            std::lock_guard connections_lock(client_pool.connections_m);
            for(auto conn_iter = client_pool.connections.begin(); conn_iter != client_pool.connections.end(); conn_iter++){
                if(client_pool.selector.isReady(*conn_iter->socket)){
                    sf::Packet pkt;
                    sf::Socket::Status status = (*conn_iter).socket->receive(pkt);
                    if(status == sf::Socket::Done){
                        ProcessPacket(pkt, *conn_iter);
                    }
                    else if(status == sf::Socket::Disconnected){
                        client_pool.selector.remove(*conn_iter->socket);
                        TPrintln(conn_iter->ToString() + " Disconnected!"s);
                        client_pool.connections.erase(conn_iter);
                        break;
                    }
                    else {
                        TPrintln("Received Erroneous Packet From Client "s + conn_iter->socket->getRemoteAddress().toString(), 0, true);
                    }
                }
            }
        }
    }
    TPrintln("Stopping...", -1);

    return 0;
}