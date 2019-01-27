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

/**
 *                           interface, singleton
 *                          +---------------------+
 *     main() - - - - - - - |   IbaseTerminator   |
 *                          +---------------------+
 *                              /\
 *                              ||
 *     interface                ||
 *   +------------+         +--------------+
 *   | IbaseClass | - - - - | class bulk   |
 *   +------------+         +--------------+
 *     /\      /\
 *     ||      ||
 * +-------+  +---------+
 * | saver |  | printer |
 * +-------+  +---------+
 *
 */

using vector_string = std::vector<std::string>;

class IbaseClass
{
public:
    using type_to_handle = struct {
    // send by copy, not by ref, because we do things asynchronically
    // and main thread is cleared vector_string right after thread
    // notification. Or it should wait all other tasks, which is
    // quite dummy in multithread system.
        const vector_string vs;
        const std::time_t t;
    };
    std::condition_variable cv;
    std::mutex cv_m;
    std::queue<type_to_handle> qMsg;

    std::vector<std::thread> vThread;
    size_t threads;

    IbaseClass(size_t threads = 1) : threads(threads) {}
    virtual ~IbaseClass(void)
    {
        std::cout << "virtual ~IbaseClass()" << std::endl;
    }

    void start_threads(void)
    {
        for (size_t i = 0; i < threads; ++i)
            vThread.push_back(std::thread(&IbaseClass::work, this));
    }

    void notify(type_to_handle &ht)
    {
        for (auto &a : ht.vs)
            std::cout << a << std::endl;
        qMsg.push(ht);
        cv.notify_one();
    }

    virtual void handle(const type_to_handle &ht)
    {
        (void) ht;
        throw;
    }
    void work(void)
    {
        std::cout << std::this_thread::get_id() << " thread started! " << std::endl;
        while(1) {
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait(lk, [this](){ return !this->qMsg.empty(); });
            auto m = qMsg.front();
            qMsg.pop();
            lk.unlock();
            std::cout << std::this_thread::get_id() << " thread handled! " << std::endl;
            handle(m);
        }
    }

protected:
    std::string output_string_make(const vector_string &vs);
};

std::string IbaseClass::output_string_make(const vector_string &vs)
{
    bool first = true;
    std::string s("bulk: ");
    for (const auto &si : vs) {
        if (!first)
            s += ", ";
        else
            first = false;
        s += si;
    }
    s += '\n';
    return s;
}

using namespace std::chrono;
class saver : public IbaseClass
{
public:
    saver(size_t threads = 1) : IbaseClass(threads) {}
    void handle(const type_to_handle &ht) override
    {
        std::hash<std::thread::id> hash_thread_id;
//        auto thread_id = std::this_thread::get_id();
//        milliseconds ms = duration_cast< milliseconds >(
//            system_clock::now().time_since_epoch()
//        );

        size_t hash = hash_thread_id(std::this_thread::get_id());// ^ std::hash<milliseconds>(ms);
        std::string filename = "bulk" + std::to_string(ht.t) + "_" + std::to_string(hash) + ".log";

        std::fstream fs;
        fs.open (filename, std::fstream::in | std::fstream::out | std::fstream::app);
        fs << output_string_make(ht.vs);
        fs.close();
    }
    ~saver() {
        std::cout << "~saver()" << std::endl;
    }
};

class printer : public IbaseClass
{
public:
    printer(size_t threads = 1) : IbaseClass(threads) {}
    void handle(const type_to_handle &ht) override
    {
        std::cout << output_string_make(ht.vs);
    }
    ~printer() {
        std::cout << "~printer()" << std::endl;
    }
};

class bulk : public IbaseTerminator
{
    const size_t bulk_size;
    vector_string vs;
    std::list<IbaseClass *> lHandler;
    size_t brace_cnt;
    std::time_t time_first_chunk;

public:
    bulk(size_t size) : bulk_size(size), brace_cnt(0), time_first_chunk(0)
    {
        vs.reserve(bulk_size);
    }

    void add_handler(IbaseClass &handler)
    {
        lHandler.push_back(&handler);
    }

    void flush(void)
    {
        if (vs.size() == 0)
            return;

        IbaseClass::type_to_handle ht = {vs, time_first_chunk};
        for (const auto &h : lHandler) {
            h->notify(ht);
        }
        std::this_thread::sleep_for (std::chrono::seconds(1));// dbg_
        vs.clear();
        time_first_chunk = 0;
    }

    void add(std::string &s);
    void signal_callback_handler(int signum);
    bool is_full(void);
    bool is_empty(void);
    void parse_line(std::string &line);
    ~bulk(void) { flush(); /*TODO: add free threads*/ }
};


#if 0

std::condition_variable cv;
std::mutex cv_m;
std::queue<std::string> msgs;

void worker(std::queue<std::string> &q)
{
    std::unique_lock<std::mutex> lk(cv_m);
    std::cout << std::this_thread::get_id() << " waiting... " << std::endl;
    cv.wait(lk, [&q](){ return !q.empty(); });
//    auto m = q.front();
//    q.pop();
    lk.unlock();

    std::cout << std::this_thread::get_id() << q.size() << " pop " << m << std::endl;
}

void start_threads(void)
{
    std::thread t1(worker, std::ref(msgs)), t2(worker, std::ref(msgs)), t3(worker, std::ref(msgs));

    {
        std::lock_guard<std::mutex> lk(cv_m);
        msgs.push("cmd1");
        msgs.push("cmd2");
    }
    cv.notify_one();

    {
        std::lock_guard<std::mutex> lk(cv_m);
        msgs.push("cmd3");
        msgs.push("cmd4");
    }
    cv.notify_one();

    t1.join();
    t2.join();
    t3.join();
}



//class bulk_worker
//{
//public:
//    virtual void dowork(void);
//};

#endif  // 0




int main(int argc, char ** argv)
{
    std::srand(time(NULL));

    printer printerHandler(1);
    saver saverHandler(2);

    if (argc != 2)
    {
        std::cerr << "Incorrect number of arguments: " << argc - 1 << ", expected: 1" << std::endl;
        return -4;
    }

    size_t j = 0;
    std::string arg = argv[1];
    try {
        std::size_t pos;
        j = std::stoi(arg, &pos);
        if (pos < arg.size()) {
            std::cerr << "Trailing characters after number: " << arg << '\n';
            return -3;
        }
    } catch (std::invalid_argument const &ex) {
        std::cerr << "Invalid number: " << arg << '\n';
        return -1;
    } catch (std::out_of_range const &ex) {
        std::cerr << "Number out of range: " << arg << '\n';
        return -2;
    }

    class bulk b{j};
    b.add_handler(printerHandler);
    b.add_handler(saverHandler);
    printerHandler.start_threads();
    saverHandler.start_threads();

    // handle SIGINT, SIGTERM
    terminator::getInstance().add_signal_handler(b);

    for(std::string line; std::getline(std::cin, line);)
    {
        b.parse_line(line);
    }

    return 0;
}

void bulk::parse_line(std::string &line)
{
    if (line == "{")
    {
        if (!is_empty() && (brace_cnt == 0))
            flush();
        ++brace_cnt;
        return;
    }
    else if (line == "}")
    {
        if (brace_cnt > 0)
        {
            --brace_cnt;
            if (brace_cnt == 0)
            {
                flush();
                return;
            }
        }
    }
    else
        add(line);

    if (is_full() && !brace_cnt)
        flush();
}

void bulk::add(std::string &s)
{
    if (time_first_chunk == 0)
        time_first_chunk = std::time(0);
    vs.push_back(s);
}

void bulk::signal_callback_handler(int signum)
{
    if ((signum == SIGINT) || (signum == SIGTERM))
        flush();
}

bool bulk::is_full(void)
{
    return vs.size() >= bulk_size;
}

bool bulk::is_empty(void)
{
    return vs.size() == 0;
}

