#include <iostream>
#include <unistd.h> 
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <thread>

int main()
{
    int fdin[2], fdout[2], n;
    pid_t p;

    pipe(fdin);
    pipe(fdout);

    p = fork();

    if (p > 0)
    {
        std::cout << "Parent entered!" << std::endl;
        
        close(fdin[0]);
        close(fdout[1]);

        std::string msg;

        // Set of descriptors to poll
        fd_set rfds;

        std::thread th([&]()
        {
            while (true)
            {
                // Zeroed set
                FD_ZERO(&rfds);
                // Add fdout[0] to poll set
                FD_SET(fdout[0], &rfds);

                int retval;
            
                retval = select(fdout[1] + 1, &rfds, NULL, NULL, NULL);
                
                int nbytes;
                ioctl(fdout[0], FIONREAD, &nbytes);

                char* bytes = new char [nbytes];
                int readed = read(fdout[0], bytes, nbytes);
                
                std::cout << retval << " " << FD_ISSET(fdout[0], &rfds) << " " << readed << std::endl;

                if (bytes[0] == 'f' || readed == 0)
                {
                    delete[] bytes;
                    break;
                }

                std::cout << "Parent! " << bytes << std::endl;
                delete[] bytes;
            }
        });

        while (true)
        {
            getline(std::cin, msg);
            
            msg += "\n";
            write(fdin[1], msg.c_str(), msg.length());

            if (msg == "f\n")
                break;
        }

        th.join();

        int a;
        waitpid(p, &a, 0);

        close(fdin[1]);
        close(fdout[0]);
    }
    else
    {
        std::cout << "Child entered!" << std::endl;

        close(STDOUT_FILENO);
        close(STDIN_FILENO);
        close(STDERR_FILENO);

        close(fdin[1]);
        close(fdout[0]);

        dup2(fdin[0], STDIN_FILENO);
        dup2(fdout[1], STDOUT_FILENO);

        close(fdin[0]);
        close(fdout[1]);

        system("psql 2>&1");
        // // Set of descriptors to poll
        // fd_set rfds;

        // while (true)
        // {
        //     // Zeroed set
        //     FD_ZERO(&rfds);
        //     // Add fdin[0] to poll set
        //     FD_SET(fdin[0], &rfds);

        //     int retval;
        
        //     retval = select(fdin[1] + 1, &rfds, NULL, NULL, NULL);
        //     std::cout << retval << " " << FD_ISSET(fdin[0], &rfds) << std::endl;
        //     int nbytes;
        //     ioctl(fdin[0], FIONREAD, &nbytes);

        //     char* bytes = new char [nbytes];
        //     read(fdin[0], bytes, nbytes);
            
        //     std::cout << "Child! " << bytes << std::endl;
            
        //     write(fdout[1], bytes, nbytes);

        //     if (bytes[0] == 'f')
        //     {
        //         delete[] bytes;
        //         break;
        //     }

        //     delete[] bytes;
        // }
    }
}