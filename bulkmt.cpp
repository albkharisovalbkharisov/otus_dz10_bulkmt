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

struct worker
{
    std::thread thread;
    std::string name;
    worker(std::thread t, std::string &s) : thread(std::move(t)), name(s) {}
    worker(std::thread t, const char *s)  : thread(std::move(t)), name(s) {}
};

class IbaseClass
{
public:
    using type_to_handle = struct {
        const vector_string vs;
        const std::time_t t;
    };
    std::condition_variable cv;
    std::mutex cv_m;
    std::queue<type_to_handle> qMsg;
    std::vector<worker> vThread;
    bool exit;

    template<typename ...Names>
    IbaseClass(Names... names) : exit(false)
    {
        const char * dummy[] = {(const char*)(names)...};
        vThread.reserve(sizeof...(Names));
        auto it = vThread.begin();
        for (auto &s : dummy)
            vThread.emplace(it++, std::thread(), s);
    }

    virtual ~IbaseClass(void)
    {
        std::cout << "virtual ~IbaseClass() : join..." << std::endl;
        exit = true;
        cv.notify_all();
        for (auto &a : vThread) {
            a.thread.join();
        }
        std::cout << "virtual ~IbaseClass() : all joined" << std::endl;
    }

    void start_threads(void)
    {
        for (auto &t : vThread)
            t.thread = std::thread(&IbaseClass::work, this, std::ref(t.name));   // move thread is OK
    }

    void notify(type_to_handle &ht)
    {
        qMsg.push(ht);
        cv.notify_one();
    }

    virtual void handle(const type_to_handle &ht)
    {
        (void) ht;
        throw;
    }
    void work(std::string &name)
    {
        {
            std::lock_guard<std::mutex>l(cv_m);
            std::cout << name << " thread started! " << std::endl;
        }
        while(!exit) {
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait(lk, [this](){ return !this->qMsg.empty() || exit; });
            if (exit) break;
            auto m = qMsg.front();
            qMsg.pop();
            lk.unlock();
            std::cout << name << " thread handled! " << std::endl;
            handle(m);
        }
        {
            std::lock_guard<std::mutex>l(cv_m);
            std::cout << name << " thread exit! " << std::endl;
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
    template<typename ...Names>
    saver(Names... names) : IbaseClass(names...) {}
    void handle(const type_to_handle &ht) override
    {
        std::hash<std::thread::id> hash_thread_id;

        size_t hash = hash_thread_id(std::this_thread::get_id()) ^ std::hash<int>()(std::rand());
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
    template<typename ...Names>
    printer(Names... names) : IbaseClass(names...) {}
//    printer(size_t threads = 1) : IbaseClass(threads) {}
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
//        std::this_thread::sleep_for (std::chrono::seconds(1));// dbg_
        vs.clear();
        time_first_chunk = 0;
    }

    void add(std::string &s);
    void signal_callback_handler(int signum);
    bool is_full(void);
    bool is_empty(void);
    void parse_line(std::string &line);
    ~bulk(void) {
        flush();
    }
};


int main(int argc, char ** argv)
{
#if 0
    (void) argc;
    (void) argv;
    std::srand(time(NULL));

    saver saverHandler("_saver1", "_saver2", "_saver3");
    printer printerHandler("_print");

    printerHandler.start_threads();
    saverHandler.start_threads();
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    return 0;

#else
    printer printerHandler("_print1");
    saver saverHandler("_saver1", "_saver2");

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

    try {
        for(std::string line; std::getline(std::cin, line); ) {
            b.parse_line(line);
        }
    } catch (std::exception& e) {
        std::cout << "EXCEPTION!!!" << e.what() << std::endl;
    }
    return 0;
#endif
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

