#ifndef OBJECT_TYPE_HPP
#define OBJECT_TYPE_HPP

#include<cstdint>
#include<unordered_map>

#include<SFML/System.hpp>
#include<SFML/Network.hpp>
#include<SFML/Graphics.hpp>
#include<mutex>

struct Object {
    uint64_t index;
    sf::Vector2f position;
    std::string asset_name;

    Object(const sf::Vector2f& pos);

    std::string ToString() const;
    void CreateSetPkt(sf::Packet& pkt) const;
    void CreateAssetPkt(sf::Packet& pkt) const;

    bool SetAsset(const std::string& name, bool server = false);

    sf::Texture texture;
    void Draw(sf::RenderWindow& window) const;
};

struct Table {
    std::recursive_mutex objects_m;
    std::unordered_map<uint64_t, Object*> objects;

    Object* s_CreateObj(const sf::Vector2f& position); //Server
    Object* c_CreateObj(const uint64_t& index, const sf::Vector2f& position); //Client
    bool SetObj(const uint64_t& index, const sf::Vector2f& position);
    bool DeleteObj(const uint64_t& index);
};

#endif