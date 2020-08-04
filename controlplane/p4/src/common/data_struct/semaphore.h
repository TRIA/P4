// Copied from https://riptutorial.com/cplusplus/example/30142/semaphore-cplusplus-11

#include <iostream>
#include <mutex>
#include <condition_variable>

class Semaphore
{
public:
    Semaphore(int count_ = 0) : count(count_) {}

    inline void notify(int tid)
    {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        std::cout << "thread " << tid << " notify" << std::endl;
        // Notify the waiting thread
        cv.notify_one();
    }

    inline void wait(int tid)
    {
        std::unique_lock<std::mutex> lock(mtx);
        while (count == 0)
        {
            std::cout << "thread " << tid << " wait" << std::endl;
            // Wait on the mutex until notify is called
            cv.wait(lock);
            std::cout << "thread " << tid << " run" << std::endl;
        }
        count--;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
};