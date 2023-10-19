#include <iostream>
#include <string>

int main()
{
    std::string msg;

    while (true)
    {
        std::getline(std::cin, msg);

        if (msg == "f")
            break;
            
        if (msg == "12")
            std::cout << "yes" << std::endl;
        else
            std::cout << "no" << std::endl;
    }
}