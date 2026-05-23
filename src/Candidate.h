#pragma once

#include "ArrowInput.h"

namespace arrowinput {

struct Candidate {
    std::wstring code;
    std::wstring text;
    std::wstring comment;
    int weight = 0;
    int user_weight = 0;
};

}  // namespace arrowinput
