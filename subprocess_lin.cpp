#include <iostream>
#include <unistd.h> 
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <thread>
#include <queue>
#include <atomic>
#include <condition_variable>

class Subprocess
{
private:
    int PipeToChild[2];
    int PipeFromChild[2];
    pid_t Pid;
    std::queue<std::string> MessagesQueue;
    std::thread Th;
    std::atomic_uint64_t DataCounter;
    std::condition_variable Cv;
    std::mutex Mtx;
    std::atomic_bool IsProcessEnded;

public:
    void Launch(std::string programmName)
    {
        DataCounter.store(0);
        IsProcessEnded.store(false);

        if (pipe(PipeToChild))
        {
            std::cerr << "Failed to create pipe!" << std::endl;
            return;
        }

        if (pipe(PipeFromChild))
        {
            std::cerr << "Failed to create pipe!" << std::endl;
            return;
        }

        // Create fork of current process
        Pid = fork();

        // Parent process
        if (Pid > (pid_t)0)
        {
            // Close unrequired pipes ends
            close(PipeToChild[0]);
            close(PipeFromChild[1]);

            Th = std::thread([&]()
            {
                // Array of descriptors to poll
                fd_set requiredPipeEndsToPoll;

                while (true)
                {
                    // Zeroed set
                    FD_ZERO(&requiredPipeEndsToPoll);
                    // Add fdout[0] to poll set
                    FD_SET(PipeFromChild[0], &requiredPipeEndsToPoll);
      
                    // Wait data in PipeFromChild
                    if (select(PipeFromChild[1] + 1, &requiredPipeEndsToPoll, NULL, NULL, NULL) == -1)
                    {
                        std::cerr << "Failed to select data from pipe! Errno: " << errno << std::endl;
                        return;
                    }

                    // Check if it is data from child inside pipe
                    if(FD_ISSET(PipeFromChild[0], &requiredPipeEndsToPoll))
                    {
                        // Get available data  
                        int availableBytes;
                        ioctl(PipeFromChild[0], FIONREAD, &availableBytes);

                        // Allocate array
                        char* bytes = new char [availableBytes];

                        // Read data from pipe
                        int readed = read(PipeFromChild[0], bytes, availableBytes);

                        // EOF and process finishing
                        if (readed == 0)
                        {
                            // Thread safety append data to queue and trigger wait function
                            while (!Mtx.try_lock())
                                Cv.notify_all();
                            
                            IsProcessEnded.store(true);
                            Mtx.unlock();

                            delete[] bytes;
                            break;
                        }
                        else
                        {
                            if (readed != availableBytes)
                            {
                                std::cerr << "Failed to read data from pipe!" << std::endl;
                                delete[] bytes;
                                return;
                            }
                            else
                            {
                                // Thread safety append data to queue and trigger wait function
                                while (!Mtx.try_lock())
                                    Cv.notify_all();

                                MessagesQueue.push(std::string(bytes, readed));
                                DataCounter.fetch_add(1);
                                Mtx.unlock();
                            }
                        }

                        delete[] bytes;
                    }
                }
            });
            return;
        }

        // Child process
        if (Pid == (pid_t)0)
        {
            close(STDOUT_FILENO);
            close(STDIN_FILENO);
            close(STDERR_FILENO);

            close(PipeToChild[1]);
            close(PipeFromChild[0]);

            dup2(PipeToChild[0], STDIN_FILENO);
            dup2(PipeFromChild[1], STDOUT_FILENO);
            dup2(PipeFromChild[1], STDERR_FILENO);

            close(PipeToChild[0]);
            close(PipeFromChild[1]);

            std::string command = programmName;
            
            system(command.c_str());

            exit(EXIT_SUCCESS);
        }
        
        // Error
        if (Pid < (pid_t)0)
        {
            std::cerr << "Failed to create fork!" << std::endl;
            return;
        }
    }

    void SendData(std::string data, bool isRequiredNewLineSymbol = true)
    {
        Mtx.lock();
        
        if (IsProcessEnded.load())
        {
            Mtx.unlock();
            return;
        }

        if (isRequiredNewLineSymbol)
            data += '\n';

        write(PipeToChild[1], data.c_str(), data.length());
        Mtx.unlock();
    }

    std::string GetData(bool isRemoveNewLineSymbols = true)
    {
        std::string res = "";
        Mtx.lock();
        if (!MessagesQueue.empty())
        {
            res = MessagesQueue.front();
            MessagesQueue.pop();
            DataCounter.fetch_sub(1);
        }
        Mtx.unlock();

        if (isRemoveNewLineSymbols)
        {
            // Remove LF from res
            if(!res.empty())
                res.pop_back();
        }

        return res;
    }

    std::string WaitData(bool isRemoveNewLineSymbols = true)
    {
        std::string res = "";
        std::mutex cvMtx;
        std::unique_lock <std::mutex> lk(cvMtx);

StartWaiting:
        Mtx.lock();

        if (IsProcessEnded.load())
        {
            Mtx.unlock();
            return "";
        }

        if (!MessagesQueue.empty())
        {
            res = MessagesQueue.front();
            MessagesQueue.pop();
            DataCounter.fetch_sub(1);
            Mtx.unlock();
        }
        else
        {
            Cv.wait(lk);
            Mtx.unlock();
            goto StartWaiting;
        }

        if (isRemoveNewLineSymbols)
        {
            // Remove LF from res
            if(!res.empty())
                res.pop_back();
        }

        return res;
    }

    template <class T = std::chrono::milliseconds>
    std::string WaitDataFor(uint64_t timeToWait, bool isRemoveNewLineSymbols = true)
    {
        std::string res = "";
        std::mutex cvMtx;
        std::unique_lock <std::mutex> lk(cvMtx);

StartWaiting:
        Mtx.lock();

        if (IsProcessEnded.load())
        {
            Mtx.unlock();
            return "";
        }

        if (!MessagesQueue.empty())
        {
            res = MessagesQueue.front();
            MessagesQueue.pop();
            DataCounter.fetch_sub(1);
            Mtx.unlock();
        }
        else
        {
            std::cv_status res = Cv.wait_for(lk, T(timeToWait));
            Mtx.unlock();

            if (res == std::cv_status::timeout)
                return "";

            goto StartWaiting;
        }

        if (isRemoveNewLineSymbols)
        {
            // Remove LF from res
            if(!res.empty())
                res.pop_back();
        }

        return res;
    }

    void StopProcess(std::string commandToSendToProcessToStop = "", bool isRequiredNewLineSymbol = true)
    {
        SendData(commandToSendToProcessToStop, isRequiredNewLineSymbol);
        Th.join();

        int a;
        waitpid(Pid, &a, 0);
        close(PipeToChild[1]);
        close(PipeFromChild[0]);
    }

    bool IsData()
    {
        return DataCounter.load() > 1; 
    }

    bool GetIsProcessEnded()
    {
        return IsProcessEnded.load();
    }

    ~Subprocess()
    {
        if (Th.joinable())
        {
            std::cerr << "Thread was not joined! Higly possible thar you dont stop process" << std::endl;
        }
    }
};

// g++ subprocess.cpp -lpthread
int main()
{
    Subprocess s;

    s.Launch("./test.out");

    std::string msg;
    std::string res;

    for (int i = 0; i < 20; ++i)
    {
        if (s.GetIsProcessEnded())
        {
            std::cout << "Process ended before getting password!" << std::endl;
            break;
        }

        s.SendData(std::to_string(i));
        res = s.WaitData();
        
        if (res == "yes")
        {
            std::cout << i << std::endl;
            break;
        }
    }

    s.StopProcess("f");
}