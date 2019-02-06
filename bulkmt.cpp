#include <cstdlib>
#include <iostream>
#include <vector>
#include <list>
#include <fstream>
#include <ctime>
#include <string>
#include <signal.h>
//#include "terminator.h"
#include <stdexcept>

#include <chrono>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>

#define BASE_TERMINATE  1

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
#if BASE_TERMINATE == 1
        terminate();
#endif
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
            cv.wait(lk, [this]() { return !this->qMsg.empty() || exit; });
            if (exit && qMsg.empty()) break;
            std::cout << " got job! " << std::endl;
            auto m = qMsg.front();
            qMsg.pop();
            lk.unlock();
            std::cout << " thread handled! this = " << this << std::endl;

            size_t **ptr = reinterpret_cast<size_t **>(this);
            for (int i = 0; i < 5; ++i) {
                std::cout << i << " : " << ptr[i] << std::endl;
            }
            std::cout << std::endl;
            size_t **ptrT = (size_t **) ptr[0];
            for (int i = 0; i < 5; ++i) {
                std::cout << &ptrT[i] << " : " << ptrT[i] << std::endl;
            }

            std::cout << " thread handled! this = " << this << std::endl;
            if (this)
                this->handle(m);
            std::cout << " end of loop " << this << std::endl;
        }
        std::cout << " ended! " << std::endl;
    }
};

struct printer2 : public IbaseClass
{
    void handle(int t) override { std::cout << "tlen" << std::endl;}
    ~printer2() {
        std::cout << "~printer()" << std::endl;
#if BASE_TERMINATE == 0
        terminate();
#endif
    }
};

struct printer : public IbaseClass
{
    printer() {}
    void handle(int t) override
    {
        std::cout << "handle" << std::endl;
        std::cout << t << std::endl;
    }
    ~printer() {
        std::cout << "~printer()" << std::endl;
#if BASE_TERMINATE == 0
        terminate();
#endif
    }
};



int main()
{
    try {
        {
        printer2 printerHandler2{};
        printerHandler2.start_threads();
        printerHandler2.notify(15);
        std::cout << &printerHandler2 << std::endl;
        }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
        printer printerHandler{};
        printerHandler.start_threads();
        printerHandler.notify(15);
        std::cout << &printerHandler << std::endl;
    }
    catch (std::exception &e) { std::cout << "CATCH WHAT???  " << e.what() << std::endl; }
    return 0;
}

