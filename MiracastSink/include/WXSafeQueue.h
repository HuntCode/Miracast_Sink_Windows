/*
*安全队列
*/
#ifndef WX_SAVEQUEUE_H
#define WX_SAVEQUEUE_H

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>
#include <thread>

template<typename T>
class WXSafeQueue
{
private:
    mutable std::mutex m_mut;
    std::queue<T> m_queue;
    std::condition_variable m_data_cond;

public:
	WXSafeQueue() {}
	WXSafeQueue(const WXSafeQueue&) = delete;

    void Push(T data)
    {
        std::unique_lock<std::mutex> locker(m_mut);
        m_queue.push(data);
        locker.unlock();
        m_data_cond.notify_one();
    }

	void WaitForPop(T&t, int ms = -1)
    {
        std::unique_lock<std::mutex> ul(m_mut);
		if (ms == -1){
			m_data_cond.wait(ul, [this] {return !m_queue.empty(); });
			t = m_queue.front();
			m_queue.pop();
		}
		else
		{
			m_data_cond.wait_for(ul, std::chrono::milliseconds(ms), [this] {return !m_queue.empty(); });
			if (!m_queue.empty()) {
				t = m_queue.front();
				m_queue.pop();
			}
		}
    }

    std::shared_ptr<T> WaitForPop()
    {
        std::unique_lock<std::mutex> ul(m_mut);
        m_data_cond.wait_for(ul, std::chrono::milliseconds(100), [this] {return !m_queue.empty(); });

        std::shared_ptr<T> res;
		if (!m_queue.empty()){
            res = std::make_shared<T>(m_queue.front());
			m_queue.pop();
		}

        return res;
    }

    bool TryPop(T &t)
    {
        std::lock_guard<std::mutex> lg(m_mut);
        if (m_queue.empty())
            return false;

        t = m_queue.front();
        m_queue.pop();
        return true;
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lg(m_mut);

        while (m_queue.size() > 0)
        {
            m_queue.pop();
        }
    }

    std::shared_ptr<T> TryPop()
    {
        std::lock_guard<std::mutex> lg(m_mut);
        if (m_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(std::make_shared<T>(m_queue.front()));
        m_queue.pop();
        return res;
    }

    bool IsEmpty()
    {
        std::lock_guard<std::mutex> lg(m_mut);
        return m_queue.empty();
    }

    int Size()
    {
        std::lock_guard<std::mutex> lg(m_mut);
        return m_queue.size();
    }
};

#endif // WX_SAVEQUEUE_H
