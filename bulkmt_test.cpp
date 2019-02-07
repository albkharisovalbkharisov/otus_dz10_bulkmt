#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>

#define BASE_TERMINATE  0

class base
{
public:
    std::condition_variable cv;
    std::atomic<bool> exit;
    std::mutex cv_m;
    std::queue<int> qMsg;
    std::thread thread;
    virtual void eat() { std::cout << "EAT BASE" << std::endl; }

    base() : exit(false), thread(std::thread(&base::work, this)) {}

    void terminate(void)
    {
        exit = true;
        cv.notify_all();
        if (thread.joinable()){
            thread.join();
        }
        else
            std::cout << "terminate() : join..." << " can't!" << std::endl;
    }

    virtual ~base(void)
    {
        std::cout << "~base()" << std::endl;
#if BASE_TERMINATE == 1
        terminate();
#endif
    }

    void notify(int t)
    {
        qMsg.push(t);
        cv.notify_one();
    }

    void work()
    {
        std::cout << " thread started! " << std::endl;
        while(1) {
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait(lk, [this]() { return !this->qMsg.empty() || exit; });
            if (exit && qMsg.empty()) break;
            qMsg.front();
            qMsg.pop();
            lk.unlock();
            if (this)
                this->eat();        // THE GIST ! THE POINT! THE CORE!
        }
    }
};

struct derived : public base
{
    void eat() override {
        std::cout << "EAT DERIVED" << std::endl;
    }
    ~derived() {
        std::cout << "~derived()" << std::endl;
#if BASE_TERMINATE == 0
        terminate();
#endif
    }
};


int main()
{
    try {
        derived test{};
        test.notify(15);
    }
    catch (std::exception &e) { std::cout << "CATCH WHAT???  " << e.what() << std::endl; }
    return 0;
}

