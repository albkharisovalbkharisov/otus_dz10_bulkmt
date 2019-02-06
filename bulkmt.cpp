#include <cstdlib>
#include <iostream>
#include <vector>
#include <list>
#include <fstream>
#include <ctime>
#include <string>
#include <signal.h>
#include "terminator.h"
#include <stdexcept>

#include <chrono>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>

struct worker
{
    std::thread thread;
//    worker(std::thread t) : thread(std::move(t)) {}
    worker(std::thread t) : thread(std::move(t)) {}
    ~worker(void) { std::cout << "~worker()" << std::endl; }
};

class IbaseClass
{
public:
    std::condition_variable cv;
    std::mutex cv_m;
    std::queue<int> qMsg;
    std::list<worker> vThread;
    std::atomic<bool> exit;
    virtual void handle(int t) { (void) t; throw; }

    IbaseClass() : exit(false)
    {
        vThread.emplace_back(std::thread());
    }

    void terminate(void)
    {
        std::cout << "terminate()" << std::endl;
        exit = true;
        std::cout << "terminate() : notify..." << std::endl;
        cv.notify_all();
        for (auto &a : vThread) {
            if (a.thread.joinable()){
                std::cout << "terminate() : join..." << std::endl;
                a.thread.join();
            }
            else
                std::cout << "terminate() : join..." << " can't!" << std::endl;
        }
        std::cout << "terminate() : all joined" << std::endl;
    }

    virtual ~IbaseClass(void)
    {
        std::cout << "~IbaseClass()" << std::endl;
        //terminate();
        std::cout << "~IbaseClass() end" << std::endl;
    }

    void start_threads(void)
    {
        for (auto &w : vThread)
            w.thread = std::thread(&IbaseClass::work, this, std::ref(w));   // moving thread is OK
    }

    void notify(int t)
    {
        qMsg.push(t);
        cv.notify_one();
    }

    void work(struct worker &w)
    {
        (void) w;
        std::cout << " thread started! " << std::endl;
        while(1) {
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait(lk, [this](){ return !this->qMsg.empty() || exit; });
            if (exit && qMsg.empty()) break;
            std::cout << " got job! " << std::endl;
            auto m = qMsg.front();
            qMsg.pop();
            lk.unlock();
            std::cout << " thread handled! " << std::endl;
            this->handle(m);
        }
        std::cout << " ended! " << std::endl;
    }
};

class printer : public IbaseClass
{
public:
    printer() {}
    void handle(int t) override
    {
        std::cout << t << std::endl;
    }
    ~printer() {
        std::cout << "~printer()" << std::endl;
        terminate();
    }
};



int main()
{
    try {
        printer printerHandler{};
        printerHandler.start_threads();
        printerHandler.notify(15);
    }
    catch (std::exception &e) { std::cout << "CATCH WHAT???  " << e.what() << std::endl; }
    return 0;
}

