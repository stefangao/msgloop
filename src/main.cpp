#include <iostream>
#include "msgloop.h"

static void onInit()
{
    llshell::setTimer(2000, [](int tid, const void* userData) {
        std::cout << "time out (" << tid << ")" << std::endl;
    }, nullptr, false);
}

int main()
{
    llshell::postCallback(nullptr, [](int, const void* userData) {
        onInit();
    });
    llshell::run_msgloop();
    return 0;
}
