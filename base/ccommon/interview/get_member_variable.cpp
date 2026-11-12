#include "template/det_member_variable_exist.h"
#include <iostream>
#include <string>

struct A {
    std::string header;
};

struct B {
    std::string header1;
};

Register(std::string, header)

int main ()
{
    const std::string conv = "huang";
    A a;
    if (has_header<decltype(a)>::value) {
        has_header<decltype(a)>::set_header(a, conv);
    }

    B b;
    if (has_header<decltype(b)>::value) {
        has_header<decltype(b)>::set_header(b, conv);
    }
    return 0;
}

//比较不优雅的实现方式
//template <class T>
//std::string get_timestamp(T&a, std::decay_t<decltype(a.header1)>* = nullptr) {
//    return "";
//}
//
//template <class T>
//std::string get_timestamp(T&a, std::decay_t<decltype(a.header)>* = nullptr) {
//    return a.header;
//}
//int main ()
//{
//    A a;
//    B b;
//    get_timestamp(a);
//    get_timestamp(b);
//    return 0;
//}
