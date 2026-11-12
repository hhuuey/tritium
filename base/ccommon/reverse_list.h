//
// Created by huanghh on 2023/2/12.
//

#ifndef TRITUM_REVERSE_LIST_H
#define TRITUM_REVERSE_LIST_H

struct mlist
{
    mlist(int data) : data_(data), next_(nullptr) {} // next也要初始化
    int data_;
    mlist *next_;
};

mlist *reverse_list(mlist *head) // 如果要直接更改head的值就要加引用
{
    mlist *prev = nullptr; // 第一次的prev是nullptr
    mlist *mid = head;
    while (mid)
    { // 每一轮开始的时候， prev已经逆序好，mid还没
        auto next = mid->next_;
        mid->next_ = prev;
        prev = mid;
        mid = next;
    }
    return prev;
}

mlist *reverse_list_recurve(mlist *head) // head之后都是逆序，head的next指向空指针，返回的是最终的那个头
{
    if (!head || !head->next_)
        return head; // 停止条件
    auto new_head = reverse_list_recurve(head->next_);
    head->next_->next_ = head;
    head->next_ = nullptr;
    return new_head;
}

#endif // TRITUM_REVERSE_LIST_H
