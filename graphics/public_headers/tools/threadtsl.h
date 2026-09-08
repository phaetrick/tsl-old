#pragma once

#include <thread>
#include <mutex>
#include <atomic>

namespace tsl{
    class Thread {
    public:
        Thread() = default;
       
        enum{
            LowPriority = 0,
            NormalPriority = 1,
            HighPriority = 2,
            RealtimePriority =3
        };

        void startThread(int prio){
            exitRequest.store(false); 
            thread = std::thread(&Thread::func, this, prio);
        }
        bool isThreadRunning(){
            return thread.joinable();
        }

        void signalThreadShouldExit(){
            exitRequest.store(true, std::memory_order_release);
        }

        void waitForThreadToExit(int timeoutMillis = 0){
            if(thread.joinable()){
                thread.join();
            }
        }
        
        virtual void run() = 0;
        virtual void stop() = 0;

        // Implemented in src/tools/threadtsl.cpp -- out of line because the
        // Windows path needs <windows.h>, which never belongs in a public
        // header. Accepts either the enum above or an Android nice value
        // (more negative = higher, e.g. -19 = urgent audio).
        static void setPriority (const int prior);


    protected:
        bool threadShouldExit(){
            return exitRequest.load(std::memory_order_acquire);
        }
        private:
        std::atomic<bool> exitRequest{};


        std::thread thread;
        std::recursive_mutex mutex;
        void func(int prio){
            setPriority(prio);
            run();
        }
    };
}


