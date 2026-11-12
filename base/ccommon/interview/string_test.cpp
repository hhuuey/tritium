//
// Created by huanghh on 2023/2/13.
//

#include <iostream>
#include <string>
#include <algorithm> // std::replace

using namespace std;

int main()
{
    string s = "hello_world";

    string p = s.substr(6, 2); // 第一参数(开始的第一个字符的位置)默认参数 0， 第二参数(字符串的总数)默认参数 string::npos
    cout << p << endl;

    string pp(10, 'c'); // 初始化 10个c
    cout << pp << endl;

    pp.insert(1, "hu"); //  在下标1前面插入字符串h
    cout << pp << endl;

    size_t it = pp.find("uc"); // 返回搜索字符串位置
    cout << it << endl;

    pp.erase(3, 2); //从下标1开始长度为2的字符串删除
    std::cout << pp << endl;

    pp.replace(4, 2,"oo"); // 从下标4开始长度为2的字符串替换成"oo"
    cout << pp << endl;

    replace(pp.begin(), pp.end(), 'u', 'o'); // algorithm里面的，替换的只能是单个字符串
    cout << pp << endl;

    // 查找字符串中出现"oc"的所有位置
    size_t pos = 0;
    while ((pos = pp.find("oc", pos)) != string::npos) { // find从pos位置开始找
        cout << pos << " ";
        ++pos;
    }
    cout << endl;

    // 替换字符串中"oc"， 变成"om"
    size_t i = 0;
    while ((i = pp.find("oc")) != string::npos) {
        pp.replace(i, 2, "om");
    }
    cout << pp << endl;

    return 0;
}