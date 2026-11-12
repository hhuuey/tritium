#ifndef TRITUM_M_STRUCT_H
#define TRITUM_M_STRUCT_H

#include <unordered_map>
#include <list>
#include <iterator>
#include <stack>
namespace tritium {

// 循环队列
// 牺牲一个存储单元 （Q.rear + 1）% MaxSize == Q.front
// 增设一个size或者bool形的tag
template<class T>
struct recycle_queue {
    recycle_queue(const size_t max_size) : max_size_(max_size) {
        data_ = new T[max_size_];
    }
    ~recycle_queue() {
        delete []data_;
    }
    size_t size() const { return cur_size_; }
    bool empty() const { return  size() == 0; }
    bool push(const T data) {
        if (size() == max_size_) return false;
        data_[rear_] = data;
        front_ = (front_ + 1) % max_size_;
        cur_size_ ++;
        return true;
    }
    bool pop() {
        if (empty()) return false;
        rear_ = (rear_ + 1) % max_size_;
        cur_size_ --;
        return true;
    }
    T front() { return data_[front_]; }
    T back() { return data_[rear_]; }

private:
    const size_t max_size_;
    size_t cur_size_;
    T *data_;
    size_t front_ = 0;
    size_t rear_ = 0;
};


//请你设计并实现一个满足LRU (最近最少使用) 缓存约束的数据结构.
//实现LRUCache类
//1. LRUCache(int capacity) 以正整数作为容量 capacity 初始化 LRU 缓存
//2. int get(int key) 如果关键字key存在于缓存中, 则返回关键字的值, 否则返回 -1.
//3. void put(int key, int value)如果关键字key 已经存在, 则变更其数据值value;
// 如果不存在, 则向缓存中插入该组key-value.如果插入操作导致关键字数量超过capacity, 则应该逐出最久未使用的关键字.
//函数 get 和 put 必须以 O(1) 的平均时间复杂度运行.

template<class Key, class Value>
struct LRUCache {
    LRUCache(size_t capacity) : capacity_(capacity){}
    bool get(const Key &key, Value& value) {
        if (index_.find(key) == index_.end()) {
            return false;
        }
        value = index_[key]->second;
        // 把旧位置的key删除， 插入到链表的头上
        cash_.erase(index_[key]);
        cash_.push_front(std::make_pair(key, value));
        index_[key] = cash_.begin();
        return true;
    }

    void put(const Key &key, const Value &value) {
        if (index_.find(key) != index_.end()) {
            // 找到了也要挪到第一个使用的位置
            cash_.erase(index_[key]);
        } else {
            if (cash_.size() == capacity_) {
                index_.erase(cash_.back().first);
                cash_.pop_back();
            }
        }
        cash_.push_front(std::make_pair(key, value));
        index_[key] = cash_.begin();
    }

private:
    size_t capacity_;
    std::list<std::pair<Key, Value>> cash_;
    std::unordered_map<Key, std::list<Key,Value>::iterator> index_; // Key重載==, 傳入hash_struct(结构体重载括号，重载参数为Key)
};
}

// 栈中实现o1复杂度的的双栈实现
// 定义栈的数据结构，请在该类型中实现一个能够得到栈的最小元素的 min 函数在该栈中，调用 min、push 及 pop 的时间复杂度都是 O(1)。
template<class T>
struct min_stack {
    void push(const T& val) {
        data_.push(val);
        if (min_value_.empty()) min_value_.push(data_.top());
        else min_value_.push(std::min(data_.top(), min_value_.top()));
    }

    void pop() {
        data_.pop();
        min_value_.pop();
    }

    T min() {
        return min_value_.top();
    }

    T top() {
        return data_.top();
    }

private:
    std::stack<T> data_;
    std::stack<T> min_value_;
};


#endif //TRITUM_M_STRUCT_H
