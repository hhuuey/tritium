#include "base/singleton.h"

#include <assert.h>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


template<class BaseMsg>
class RegisterBase {
    using FuncPtr = std::function<std::unique_ptr<BaseMsg>()>;
    Singleton(RegisterBase)

public:
    template<class T>
    struct Register {
        Register(const std::string &s) {
            register_.emplace(s, [] () { return std::make_unique<T>();});
        }

        template<class... Args>
        Register(const std::string &s, Args ...args) {
            register_.emplace(s, [&args...] () { return std::make_unique<T>(args...);});
        }
    };

    std::unique_ptr<BaseMsg> Generate(const std::string &s) {
        assert(RegisterBase::Instance().register_.find(s) != RegisterBase::Instance().register_.end());
        return RegisterBase::Instance().register_.at(s)();
    }

private:
    static std::unordered_map<std::string, FuncPtr> register_;
};

template <class BaseMsg>
std::unordered_map<std::string, typename RegisterBase<BaseMsg>::FuncPtr> RegisterBase<BaseMsg>::register_;
#define RegisterMsg(BaseMsg, DevMsg, ...) static RegisterBase<BaseMsg>::Register<DevMsg> g_msg_##BaseMsg##_##DevMsg##_(#DevMsg, ##__VA_ARGS__);







