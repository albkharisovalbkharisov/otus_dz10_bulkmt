// terminator.h
#pragma once
#include <iostream>
#include <signal.h>
#include <list>

void handle_signal(int signum);
class IbaseTerminator
{
public:
    virtual void signal_callback_handler(int signum) = 0;
};

class terminator
{
    std::list<IbaseTerminator *> lHandler;
    // singleton
    terminator(void)
    {
        signal(SIGINT, handle_signal);
        signal(SIGTERM, handle_signal);
    }
    terminator(const terminator &);
    terminator& operator=(terminator &);
public:
    static terminator& getInstance(void)
    {
        static terminator instance;
        return instance;
    }

    void add_signal_handler(IbaseTerminator &handler)
    {
        lHandler.push_back(&handler);
    }

    void handle_all_signals(int signum)
    {
        for (const auto &h : lHandler){
            h->signal_callback_handler(signum);
        }
        switch (signum)
        {
        case SIGINT:
        /*case SIGKILL: can't be handled*/
        case SIGTERM:
            exit(signum);
            break;
        default:
            std::cerr << "unhandled signal " << signum << std::endl;
            break;
        }
    }
};

