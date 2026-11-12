//
// Created by huang on 2023/2/4.
//

#include <iostream>

using namespace std;

struct list {
    int data;
    list *next = nullptr;
    list(int val) : data(val) {}
};

list *reverse_list(list *head) {
    list *pre = nullptr;
    list *mid = head;
    while (mid) {
        list *next = mid->next;
        mid->next = pre;
        pre = mid;
        mid = next;
    }
    return pre;
}

int main(int argc, char *argv[]) {
    list* head = new list(1);
    head->next = new list(2);
    head->next->next = new list(3);
    auto it = reverse_list(head);
    while (it) {
        std::cout << it->data << std::endl;
        it = it->next;
    }

//    std::cout << argv[1] << std::endl;
    std::cout << "hhh come on!" << std::endl;
    return 0;
}