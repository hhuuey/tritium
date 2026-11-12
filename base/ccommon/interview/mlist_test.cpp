//
// Created by huang on 2023/2/4.
//

#include "reverse_list.h"

#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
    std::cout << "hhh come on!" << std::endl;

    auto a = new mlist(1);
    a->next_ = new mlist(2);

    auto b = a->next_;
    b->next_ = new mlist(3);

    auto c = b->next_;
    c->next_ = new mlist(4);

    auto head = a;
    while (head != nullptr) {
        std::cout << head->data_ << std::endl;
        head = head->next_;
    }
    std::cout << ".." << std::endl;

    auto aa = reverse_list(a);
    while (aa != nullptr) {
        std::cout << aa->data_ << std::endl;
        aa = aa->next_;
    }

//    auto new_a = reverse_list_recurve(a);
//    while (new_a != nullptr) {
//        std::cout << new_a->data_ << std::endl;
//        new_a = new_a->next_;
//    }
    return 0;
}