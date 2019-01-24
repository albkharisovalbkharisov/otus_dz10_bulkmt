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

#define SAVE_EACH_BULK_TO_SEPARATE_FILE 1

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
        const vector_string &vs;
        const std::time_t t;
    };

    virtual void handle(const type_to_handle &ht) = 0;

protected:
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


class saver : public IbaseClass
{
public:
    void handle(const type_to_handle &ht) override
    {
        std::string filename = "bulk" + std::to_string(ht.t) + ".log";

        std::fstream fs;
        fs.open (filename, std::fstream::in | std::fstream::out | std::fstream::app);
        fs << output_string_make(ht.vs);
        fs.close();
    }
};

class printer : public IbaseClass
{
    void handle(const type_to_handle &ht) override
    {
        std::cout << output_string_make(ht.vs);
    }
};

class bulk : public IbaseTerminator, public threadable
{
    const size_t bulk_size;
    vector_string vs;
    std::list<std::tuple<IbaseClass *, std::string>> lHandler;
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
            h->handle(ht);
        }

        vs.clear();
#if (SAVE_EACH_BULK_TO_SEPARATE_FILE == 1)
        time_first_chunk = 0;
#endif  // (SAVE_EACH_BULK_TO_SEPARATE_FILE == 1)
    }

    void add(std::string &&s)
    {
        // remembering first block coming can be done with "static" variable
        // inside this function (see commit b51ab33), but it's not quite
        // correct, because otherwise we can have only one "time_first_chunk"
        // variable for all bulk class instances
        if (time_first_chunk == 0)
            time_first_chunk = std::time(0);
        vs.push_back(s);
    }

    void signal_callback_handler(int signum)
    {
        if ((signum == SIGINT) || (signum == SIGTERM))
            flush();
    }

    bool is_full(void) { return vs.size() >= bulk_size; }
    bool is_empty(void) { return vs.size() == 0; }
    ~bulk(void) { flush(); }

    friend std::istream& operator>>(std::istream&, bulk&);
};

std::istream& operator>>(std::istream& is, bulk& this_)
{
    std::string s;
    std::getline(is, s);

    if (s == "{")
    {
        if (!this_.is_empty() && (this_.brace_cnt == 0))
            this_.flush();
        ++this_.brace_cnt;
        return is;
    }
    else if (s == "}")
    {
        if (this_.brace_cnt > 0)
        {
            --this_.brace_cnt;
            if (this_.brace_cnt == 0)
            {
                this_.flush();
                return is;
            }
        }
    }
    else
        this_.add(std::move(s));

    if (this_.is_full() && !this_.brace_cnt)
        this_.flush();

    return is;
}





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



class threadable
{
public:
    virtual void dowork(void);
};






int main(int argc, char ** argv)
{
    printer printerHandler;
    saver saverHandler;

    if (argc != 2)
    {
        std::cerr << "Incorrect number of arguments: " << argc - 1 << ", expected: 1" << std::endl;
        return -4;
    }

//    std::thread t1(worker, std::ref(msgs)), t2(worker, std::ref(msgs)), t3(worker, std::ref(msgs));
    std::thread t1(worker, std::ref(msgs)), t2(worker, std::ref(msgs)), t3(worker, std::ref(msgs));
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

    // handle SIGINT, SIGTERM
    terminator::getInstance().add_signal_handler(b);

    while (1)
    {
        std::cin >> b;
    }

    return 0;
}

