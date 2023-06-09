#pragma once

#include <mutex>
#include <queue>

template <typename T> class SimpleThreadSafeQueue
{
public:
    void Push(T&& ele)
    {
        std::unique_lock<std::mutex> lck(mutex_);
        queue_.push(std::move(ele));
    }

    bool Pop(T& out)
    {
        std::unique_lock<std::mutex> lck(mutex_);
        if (queue_.empty())
        {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

private:
    std::mutex mutex_{};
    std::queue<T> queue_{};
};