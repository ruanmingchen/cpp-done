//
// Created by root on 23-5-9.
//

#include "concurrent_map.h"

template <class KEY_T = std::string>
class MyComp : public CompInterface<KEY_T>
{
public:
    bool operator()(KEY_T key1, KEY_T key2) override { return (key1 == key2); }
};

int main()
{
    ConcurrentMap<std::string, std::string, MyComp<>> test_;
    test_.Insert("123", "456");
}