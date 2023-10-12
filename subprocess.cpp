#include <iostream>
#include <unistd.h> 
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <thread>
#include <queue>

class Subprocess
{
private:
    int PipeToChild[2];
    int PipeFromChild[2];
    int n;
    pid_t Pid;
    std::queue<std::string> MessagesQueue;
    std::thread th;

public:
    void Launch(std::string programmName)
    {
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

            th = std::thread([&]()
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
                        int nbytes;
                        ioctl(PipeFromChild[0], FIONREAD, &nbytes);

                        // Allocate array
                        char* bytes = new char [nbytes];

                        // Read data from pipe
                        int readed = read(PipeFromChild[0], bytes, nbytes);

                        // EOF
                        if (readed == 0)
                        {
                            delete[] bytes;
                            break;
                        }

                        if (readed != nbytes)
                        {
                            std::cerr << "Failed to read data from pipe!" << std::endl;
                            delete[] bytes;
                            return;
                        }

                        MessagesQueue.push(std::string(bytes));
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

            close(PipeToChild[1]);
            close(PipeFromChild[0]);

            dup2(PipeToChild[0], STDIN_FILENO);
            dup2(PipeFromChild[1], STDOUT_FILENO);

            close(PipeToChild[0]);
            close(PipeFromChild[1]);

            std::string command = programmName + " 2>&1";
            
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
        if (isRequiredNewLineSymbol)
            data += '\n';

        write(PipeToChild[1], data.c_str(), data.length());
    }

    std::string GetData()
    {
        std::string res = "";
        if (!MessagesQueue.empty())
        {
            res = MessagesQueue.front();
            MessagesQueue.pop();
        }
        return res;
    }

    void StopProcess(std::string commandToSendToProcessToStop, bool isRequiredNewLineSymbol = true)
    {
        SendData(commandToSendToProcessToStop, isRequiredNewLineSymbol);
        th.join();

        int a;
        waitpid(Pid, &a, 0);
        close(PipeToChild[1]);
        close(PipeFromChild[0]);
    }

    ~Subprocess()
    {
        if (th.joinable())
        {
            std::cerr << "Thread was not joined! Higly possible thar you dont stop process" << std::endl;
        }
    }
};

// g++ subprocess.cpp -lpthread
int main()
{
    // Что будет если процесс закончиться, а я попрбую записать данные туда. 
    Subprocess s;
    
    s.Launch("bc");

    std::string msg;
    while (true)
    {
        getline(std::cin, msg);
        
        if (msg == "f")
        {
            s.StopProcess("quit");
            break;
        }

        s.SendData(msg);
        sleep(1);

        std::string res = s.GetData();

        if (!res.empty())
            res.pop_back();

        std::cout << res << std::endl;
    }
}