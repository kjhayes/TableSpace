#include "object.hpp"

#include<random>

#include "tprint.hpp"
#include "pkt_headers.hpp"
#include "asset.hpp"

using namespace std::literals::string_literals;

Object::Object(const sf::Vector2f& pos) : position(pos) {
    index = (uint64_t)this;
    this->asset_name = "DefaultAsset.png";
}
std::string Object::ToString() const {
    return "[Object_"s + std::to_string(index) + " : {"s + std::to_string(position.x) + ", "s + std::to_string(position.y) + "}]"s;    
}
void Object::CreateSetPkt(sf::Packet& pkt) const {
    pkt << PktHeader::SetObj << this->index << (float)position.x << (float)position.y;
}
void Object::CreateAssetPkt(sf::Packet& pkt) const {
    pkt << PktHeader::AssetObj << this->index << asset_name;
}
bool Object::SetAsset(const std::string& asset_name, bool server){
    this->asset_name = asset_name;
    if(server){return true;}
    return texture.loadFromFile(asset_path + asset_name);
}

void Object::Draw(sf::RenderWindow& window) const {
    sf::Sprite sprite(texture);
    sprite.setOrigin(sf::Vector2f(texture.getSize().x * 0.5f, texture.getSize().y * 0.5f));
    sprite.setPosition(position);
    window.draw(sprite);
}

Object* Table::s_CreateObj(const sf::Vector2f& position) {
    TPrintln("Asking For Objects Lock", mut_msg_priority);
    std::lock_guard objects_lock(objects_m);
    TPrintln("Received Objects Lock", mut_msg_priority);
    Object* obj = new Object(position);
    uint64_t index = (uint64_t)obj;
    objects.emplace(index, obj);
    TPrintln("Created Object_"s+std::to_string(index));
    TPrintln("Released Objects Lock", mut_msg_priority);
    return obj;
}
Object* Table::c_CreateObj(const uint64_t& index, const sf::Vector2f& position) {
    TPrintln("Asking For Objects Lock", mut_msg_priority);
    std::lock_guard objects_lock(objects_m);
    TPrintln("Received Objects Lock", mut_msg_priority);
    Object* obj = new Object(position);
    obj->index = index;
    objects.emplace(index, obj);
    TPrintln("Released Objects Lock", mut_msg_priority);
    return obj;
}
bool Table::SetObj(const uint64_t& index, const sf::Vector2f& position) {
    TPrintln("Asking For Objects Lock", mut_msg_priority);
    std::lock_guard objects_lock(objects_m);
    TPrintln("Received Objects Lock", mut_msg_priority);
    if(objects.contains(index)){
        objects.at(index)->position = position;
        return true;
    }
    TPrintln("Released Objects Lock", mut_msg_priority);
    return false;
}

bool Table::DeleteObj(const uint64_t& index) {
    TPrintln("Asking For Objects Lock", mut_msg_priority);
    std::lock_guard objects_lock(objects_m);
    TPrintln("Received Objects Lock", mut_msg_priority);
    if(objects.contains(index)){
        delete objects.at(index);
        objects.erase(index);
        return true;
    }
    TPrintln("Released Objects Lock", mut_msg_priority);
    return false;
}