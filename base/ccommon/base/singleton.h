#ifndef TRITIUM_SINGLETON_H
#define TRITIUM_SINGLETON_H

namespace Tritium {

#define Singleton(ClassName)   \
public:                        \
static ClassName& Instance() {  \
    static ClassName instance; \
    return instance;\
}\
private:\
    ClassName() = default;\
    ClassName(const ClassName&) = delete;\
    ClassName(ClassName&&) = delete;\
    ClassName& operator =(const ClassName& other) = delete;

}

#endif //TRITIUM_SINGLETON_H
