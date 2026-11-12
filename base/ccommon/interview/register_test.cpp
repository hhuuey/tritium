#include "template/auto_register.h"
#include <iostream>
#include <thread>

class Msg {
public:
    virtual void Print() {
        std::cout << "MSG" << std::endl;
    };
};
template class RegisterBase<Msg>;

class Msg1 : public Msg {
public:
    void Print() override {
        std::cout << "Msg1" << std::endl;
    }
};


class Msg2 : public Msg {
public:
    void Print() override {
        std::cout << "Msg2" << std::endl;
    }
};


int main() {
    RegisterMsg(Msg, Msg1);
    RegisterMsg(Msg, Msg2);
    RegisterBase<Msg>::Instance().Generate("Msg1")->Print();
    RegisterBase<Msg>::Instance().Generate("Msg2")->Print();
    std::this_thread::sleep_for(std::chrono::microseconds(5000));
    getchar();
    return 0;
}
