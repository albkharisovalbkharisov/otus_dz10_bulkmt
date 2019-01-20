#include "terminator.h"

void handle_signal(int signum)
{
    terminator::getInstance().handle_all_signals(signum);
}

