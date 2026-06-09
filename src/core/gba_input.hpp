#pragma once

namespace czgba::core {

struct GbaInputState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool a = false;
    bool b = false;
    bool l = false;
    bool r = false;
    bool start = false;
    bool select = false;
};

} // namespace czgba::core
