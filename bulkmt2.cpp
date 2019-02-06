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

template<bool count_lines_too = false>
class dbg_counter
{
    std::atomic<size_t> line_counter;
    std::atomic<size_t> cmd_counter;
    std::atomic<size_t> bulk_counter;
public:
    dbg_counter(void) : line_counter(0), cmd_counter(0), bulk_counter(0) {}
    dbg_counter(const dbg_counter &that)
    {
        this->line_counter.store(that.line_counter);
        this->cmd_counter.store(that.cmd_counter);
        this->bulk_counter.store(that.bulk_counter);
    }
/*
    dbg_counter(const dbg_counter &&that)
    {
        this->line_counter.store(that.line_counter);
        this->cmd_counter.store(that.cmd_counter);
        this->bulk_counter.store(that.bulk_counter);
    }

    dbg_counter& operator=(dbg_counter &&that)
    {
        this->line_counter.store(that.line_counter);
        this->cmd_counter.store(that.cmd_counter);
        this->bulk_counter.store(that.bulk_counter);
        return *this;
    }
*/
    void line_inc(size_t i = 1) { line_counter += i; }
    void cmd_inc(size_t i = 1)  { cmd_counter  += i; }
    void bulk_inc(size_t i = 1) { bulk_counter += i; }

    void print_counters(const std::string &thread_name) const
    {
        std::cout << "thread " << thread_name << ": ";
        if (count_lines_too)
            std::cout << line_counter << " lines, ";
        std::cout << cmd_counter << " commands, ";
        std::cout << bulk_counter << " bulks" << std::endl;
    }
    ~dbg_counter(void) { std::cout << "~dbg_counter() " << std::endl; }
};


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
    dbg_counter<false> dbg;
    worker(std::thread t, const char *s) : thread(std::move(t)), name(s) {}
    worker(const char *s) : name(s) {}
    ~worker(void) { std::cout << "~worker()" << std::endl; }
//    worker(std::thread t, std::string &s) : thread(std::move(t)), name(s) {}
/*
    worker(const worker &that)
    {
        thread = that.thread;
        name = that.name;
        dbg = that.dbg;
    }
    worker& operator=(worker &&that)
    {
        thread = std::move(that.thread);
        name = std::move(that.name);
        dbg = std::move(that.dbg);
        return *this;
    }
*/
};

class test2
{
public:
    test2(){ std::cout << "test2()" << std::endl; }
    ~test2(){ std::cout << "~test2()" << std::endl; }
};

class test
{
public:
    test(){ std::cout << "test()" << std::endl; }
    ~test(){ std::cout << "~test()" << std::endl; }
};

class IbaseClass
{
public:
    using type_to_handle = struct {
        const vector_string vs;
        const std::time_t t;
    };
    test test_;
    std::condition_variable cv;
    std::mutex cv_m;
    std::queue<type_to_handle> qMsg;
    std::list<worker> vThread;
    std::atomic<bool> exit;
    test2 test2_;
    virtual void handle(const type_to_handle &ht) { (void) ht; throw; }

    template<typename ...Names>
    IbaseClass(Names... names) : exit(false)
    {
        const char * dummy[] = {(const char*)(names)...};
//        vThread.reserve(sizeof...(Names));
//        worker w{std::thread(), std::string("123"), dbg_counter<false>()};
//        auto it = vThread.begin();
        for (auto &s : dummy)
//            vThread.push_back(&w);
//            vThread.emplace(it++, std::thread(), s, dbg_counter<false>());
            vThread.emplace_back(s);
    }

    void terminate(void)
    {
        std::cout << "terminate() : join..." << std::endl;
        exit = true;
//        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "terminate() : notify..." << std::endl;
        cv.notify_all();
//        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        for (auto &a : vThread) {
            if (a.thread.joinable()){
                a.thread.join();
                std::cout << "terminate() : join..." << a.name << std::endl;
            }
            else
                std::cout << "terminate() : join..." << a.name << " can't!" << std::endl;
        }
        std::cout << "terminate() : all joined" << std::endl;
    }

    virtual ~IbaseClass(void)
    {
        std::cout << "~IbaseClass()" << std::endl;
        //terminate();
        std::cout << "vThread.size() = " << vThread.size() << std::endl;
        for (const auto &a : vThread)
            a.dbg.print_counters(a.name);
        std::cout << "~IbaseClass() after print" << std::endl;
//        exit = true;
//        cv.notify_all();
//        for (auto &a : vThread) {
//            a.thread.join();
//        }
//        std::cout << "virtual ~IbaseClass() : all joined" << std::endl;
    }

    void start_threads(void)
    {
        for (auto &w : vThread)
            w.thread = std::thread(&IbaseClass::work, this, std::ref(w));   // moving thread is OK
    }

    void notify(type_to_handle &ht)
    {
        qMsg.push(ht);
        cv.notify_one();
    }

    void work(struct worker &w)
    {
            std::cout << w.name << " thread started! " << std::endl;
        {
            std::lock_guard<std::mutex>l(cv_m);
//            std::cout << w.name << " thread started! " << std::endl;
        }
        while(1) {
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait(lk, [this](){ return !this->qMsg.empty() || exit; });
            if (exit && qMsg.empty()) break;
            std::cout << w.name << " got job! " << std::endl;
            auto m = qMsg.front();
            qMsg.pop();
            lk.unlock();
            w.dbg.bulk_inc();
            w.dbg.cmd_inc(m.vs.size());
//            std::cout << w.name << " thread handled! " << std::endl;
            this->handle(m);
        }
        std::cout << w.name << " ended! " << std::endl;
    }
};

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
        for (auto &a : ht.vs)
            fs << a;
        fs << '\n';
        fs.close();
    }
    ~saver() {
        std::cout << "~saver()" << std::endl;
//        terminate();
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
//        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        std::cout << output_string_make(ht.vs);
    }
    ~printer() {
        std::cout << "~printer()" << std::endl;
        terminate();
    }

private:
    std::string output_string_make(const vector_string &vs)
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
};

class bulk : public IbaseTerminator//, public dbg_counter<true>
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

//        bulk_inc();
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
//        flush();
//        print_counters(std::string("main"));
    }
};


void bulk::parse_line(std::string &line)
{
//    line_inc();
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
//    cmd_inc();
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



int main(int argc, char ** argv)
{
    try {
//    printer printerHandler("_print1");
//    saver saverHandler("_saver1", "_saver2");

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

//    class bulk b{j};
    printer printerHandler("_print1");
//    b.add_handler(printerHandler);
//    b.add_handler(saverHandler);
    printerHandler.start_threads();
//    saverHandler.start_threads();

    // handle SIGINT, SIGTERM
//    terminator::getInstance().add_signal_handler(b);

//    for(std::string line; std::getline(std::cin, line); ) {
//        b.parse_line(line);
//    }

//    ~b.bulk();
//    std::cout << std::endl;

//    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    IbaseClass::type_to_handle test = {{"1", "2", "3"}, 0};
    printerHandler.notify(test);



    }
    catch (std::exception &e) { std::cout << "CATCH WHAT???  " << e.what() << std::endl; }
    return 0;
}

