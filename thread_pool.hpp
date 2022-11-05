#ifndef MLAMBDA_H
#define MLAMBDA_H

#include <map>
#include <vector>
#include <condition_variable>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <future>
#include <iostream>

namespace SZPP11{

class thread_pool{

public:
    using task = std::function<void(void)>;

private:
    unsigned short m_thread_num = 1;

    std::mutex m_task_mutex;
    std::condition_variable m_task_condition;

    std::vector<std::thread> m_workers;
    std::queue<task> m_tasks;

public:
    thread_pool(int num):m_thread_num(num)
    {
        auto work = [this](){
            while (true) {
                task t;

                {
                    // add {}
                    // if wait return true, release mutex and run a task
                    // if not, mutex will be lock until t() return
                    // if t never end (e.g. while(true){}), programe will pause at another run(t) cause waiting mutex
                    std::unique_lock<std::mutex> _lck(m_task_mutex);
                    m_task_condition.wait(_lck, [this]{
                        return !this->m_tasks.empty();
                    });
                    t = std::move(m_tasks.front()); // may not necessary to use stdmove?
                    /// @todo test constractor efficiency for std::function<void(void)> type
                    m_tasks.pop();
                }

                t();

            }// while(1) loop
        };// lambda def

        for(int i=0;i<m_thread_num;i++)
        {
            std::thread _t(work);
            m_workers.emplace_back(std::move(_t));
        }
    }

    template<class F, class... Args>
    auto run(F && f,Args && ... args) -> std::future<decltype (f(args...))>
    {
        using ret_type = decltype(f(args...));

//        auto task = std::make_shared<std::packaged_task<type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto run_func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        using smart_task_ret_type = std::packaged_task<ret_type()>;
        std::shared_ptr<smart_task_ret_type> task = std::make_shared<smart_task_ret_type>(run_func);

        std::future<ret_type> future = task->get_future(); // packged_task->get_future();

        /// @todo can if not use shared_ptr
//        smart_task_ret_type s = [f,args...] () -> ret_type{f(args...);};

        {
            std::unique_lock<std::mutex> _lck(m_task_mutex);
            m_tasks.emplace([task]{
                (*task)();
            });
        }

        m_task_condition.notify_one();
        return future;
    }


};

}



#endif // MLAMBDA_H
