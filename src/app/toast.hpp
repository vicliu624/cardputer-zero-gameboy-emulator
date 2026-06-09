#pragma once

#include <chrono>
#include <string>

namespace czgba::app {

class Toast {
public:
    void show(std::string text, std::chrono::milliseconds duration = std::chrono::milliseconds(1500));
    void update(std::chrono::milliseconds elapsed);
    const std::string& text() const;
    bool visible() const;
    void clear();

private:
    std::string text_;
    std::chrono::milliseconds remaining_{0};
};

} // namespace czgba::app
