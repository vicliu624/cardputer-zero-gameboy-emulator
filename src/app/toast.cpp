#include "app/toast.hpp"

#include <utility>

namespace czgba::app {

void Toast::show(std::string text, std::chrono::milliseconds duration)
{
    text_ = std::move(text);
    remaining_ = duration;
}

void Toast::update(std::chrono::milliseconds elapsed)
{
    if (elapsed >= remaining_) {
        clear();
        return;
    }
    remaining_ -= elapsed;
}

const std::string& Toast::text() const
{
    return text_;
}

bool Toast::visible() const
{
    return !text_.empty() && remaining_.count() > 0;
}

void Toast::clear()
{
    text_.clear();
    remaining_ = std::chrono::milliseconds(0);
}

} // namespace czgba::app
