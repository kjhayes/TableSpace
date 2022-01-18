#ifndef PKT_HEADERS_HPP
#define PKT_HEADERS_HPP

#include<cstdint>

// The Server is allowed to insert multiple packets into one, that is that one packet can end, and then in the same packet there may be another header with more info, to be recursively processed, the client does not have the same privillege as they could send overly large packets to slow down the server for others.

using PktHeaderUnderlying = uint16_t;
enum PktHeader : PktHeaderUnderlying {
    // From Client: Request (index 0 tells the server to create a new object at an availible index), From Server: Command (should never have index 0)
    SetObj, // pkt << uint64_t index << float position.x << float position.y //Also indicates object existance
    DeleteObj, // pkt << uint64_t index
    AssetObj, // pkt << uint64_t index << std::string asset

    SetUser, // pkt << uint64_t index, std::string name
    SetUserColor, // pkt << uint64_t index << uint8_t r << uint8_t g << uint8_t b 

    // From Client, signals needs info on obj index referenced (0 means request all objects), From Server, signals a client obj does not exist
    QueryObj, // pkt << uint64_t index
    SetObjFlags, // pkt << uint64_t index, uint64_t flags
};

#endif