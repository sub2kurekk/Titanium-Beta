#pragma once
#include <vector>
#include <functional>
#include <algorithm>

namespace titanium {

template <typename... Args>
class Event {
public:
    using Callback = std::function<void(Args...)>;

    void listen(Callback cb) {
        callbacks.push_back(std::move(cb));
    }

    void remove(Callback cb) {
        auto it = std::remove_if(callbacks.begin(), callbacks.end(),
            [&](const Callback& c) { return c.target_type() == cb.target_type(); });
        callbacks.erase(it, callbacks.end());
    }

    void trigger(Args... args) const {
        for (auto& cb : callbacks)
            cb(args...);
    }

private:
    std::vector<Callback> callbacks;
};

} // namespace titanium
