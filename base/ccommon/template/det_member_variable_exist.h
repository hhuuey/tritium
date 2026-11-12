#ifndef TRITUM_DET_MEMBER_VARIABLE_EXIST_H
#define TRITUM_DET_MEMBER_VARIABLE_EXIST_H

//// 判断一个类内部是否有变量名称为MemberName的成员， 提供判断有无以及赋值的两个方法
// struct Base {
//   MemberType MemberName;
// }
//
//// 注册
// Register(MemberType, MemberName)
//// 注册完成后提供两个方法
// has_MemberName<decltype(a)>::value // bool值判断对象a的结构体中是否存在变量名为 MemberName的成员变量 // 注意不检查变量名的类型
// has_MemberName<decltype(a)>::set_header(a, conv); // 对象a的结构体中变量名为MemberName的成员变量 赋值为conv

#define Register(MemberType, MemberName) \
template <class T, class = int> \
struct has_##MemberName : std::false_type { \
    static void set_##MemberName(T &t, MemberType val) {} \
}; \
template <class T> \
struct has_##MemberName<T, decltype((void)T::MemberName, 0)>: std::true_type { \
    static void set_##MemberName(T &t, MemberType val) { \
        t.MemberName = val; \
    } \
};

#endif //TRITUM_DET_MEMBER_VARIABLE_EXIST_H
