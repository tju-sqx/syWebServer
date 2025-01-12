#ifndef MODERN_THREADPOOL_H
#define MODERN_THREADPOOL_H

#include <utility>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

#include "../log/log.h"
#include "../CGImysql/sql_connection_pool.h"

template<typename T>
class ModernThreadPool {
public:
    ModernThreadPool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~ModernThreadPool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    void worker();
    void process_task(std::pair<T *, int> task);

private:
    int m_thread_number_;
    int m_max_request_;
    std::vector<std::unique_ptr<std::thread>> m_threads_;
    std::mutex m_mutex_;
    std::condition_variable m_condition_var_;
    int m_actor_model_;
    std::queue<std::pair<T *, int>> m_tasks_;
    connection_pool *m_connPool_;
    bool m_stop_;
};

template<typename T>
ModernThreadPool<T>::ModernThreadPool(int actor_model, connection_pool *connPool, int thread_number, int max_request)
: m_actor_model_(actor_model), m_connPool_(connPool), m_thread_number_(thread_number), m_max_request_(max_request){
    try {
        for (int i = 0; i < m_thread_number_; ++i) {
            m_threads_.emplace_back(std::make_unique<std::thread>([this] {this->worker();}));
        }
        m_stop_ = false;
        //LOG_INFO("deal with the client(%d)", m_stop_);
    } catch(...){
        m_stop_ = true;
        for (auto& thread: m_threads_) {
            if (thread->joinable()) {
                thread->join();
            }
        }
        throw;        
    }
}

template<typename T>
ModernThreadPool<T>::~ModernThreadPool(){
    {
        std::unique_lock<std::mutex> lock(m_mutex_);
        m_stop_ = true;
    }

    for (auto& thread: m_threads_) {
        if (thread->joinable()) {
            thread->join();
        }
    }
}

template<typename T>
bool ModernThreadPool<T>::append(T *request, int state) {
    std::unique_lock<std::mutex> lock(m_mutex_);
    
    if(m_tasks_.size() >= m_max_request_) {
        return false;
    }

    request->m_state = state;
    m_tasks_.emplace(request, state);
    m_condition_var_.notify_one();
    return true;
}

template<typename T>
bool ModernThreadPool<T>::append_p(T *request) {
    std::unique_lock<std::mutex> lock(m_mutex_);
    
    if(m_tasks_.size() >= m_max_request_) {
        return false;
    }

    m_tasks_.emplace(request, request->m_state);
    m_condition_var_.notify_one();
    return true;
}




template<typename T>
void ModernThreadPool<T>::worker(){
    while(true) {
        std::unique_lock<std::mutex> lock(m_mutex_);

        m_condition_var_.wait(lock, [this]() {
            return m_stop_ || !m_tasks_.empty();
        });

        if(m_stop_ && m_tasks_.empty()) {
            break;
        }

        if(!m_tasks_.empty()) {
            auto task = std::move(m_tasks_.front());
            m_tasks_.pop();
            lock.unlock();
            process_task(task);
        }
    }
}

template <typename T>
void ModernThreadPool<T>::process_task(std::pair<T *, int> task){
    auto request = task.first;
    if(!request) {
        return;
    }

    if (m_actor_model_ == 1) {
        if (request->m_state == 0) {
            if (request->read_once()) {
                request->improv = 1;
                connectionRAII mysqlcon(&request->mysql, m_connPool_);
                request->process();
            } else {
                request->improv = 1;
                request->timer_flag = 1;
            }
        } else {
            if (request->write()) {
                request->improv = 1;
            } else {
                request->improv = 1;
                request->timer_flag = 1;
            }
        }
    } else {
        connectionRAII mysqlcon(&request->mysql, m_connPool_);
        request->process();
    }
}
#endif