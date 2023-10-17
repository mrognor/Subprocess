#include <iostream>
#include <thread>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <Windows.h>
#include <winioctl.h>

class Subprocess
{
private:
    HANDLE pipeToChild[2] = {NULL, NULL};
    HANDLE pipeFromChild[2] = {NULL, NULL};
public:
    void Launch(std::string programmName)
    {
        SECURITY_ATTRIBUTES saAttr;

        // Set the bInheritHandle flag so pipe handles are inherited.
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        // Create a pipe for the child process's STDOUT. Read from 0, write to 1
        CreatePipe(&pipeFromChild[0], &pipeFromChild[1], &saAttr, 0);

        // Ensure the read handle to the pipe for STDOUT is not inherited.
        SetHandleInformation(pipeFromChild[0], HANDLE_FLAG_INHERIT, 0);

        // Create a pipe for the child process's STDIN.
        CreatePipe(&pipeToChild[0], &pipeToChild[1], &saAttr, 0);

        // Ensure the write handle to the pipe for STDIN is not inherited.
        SetHandleInformation(pipeToChild[1], HANDLE_FLAG_INHERIT, 0);

        PROCESS_INFORMATION piProcInfo;
        STARTUPINFOA siStartInfo;
        BOOL bSuccess = FALSE;

        // Set up members of the PROCESS_INFORMATION structure.

        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        // Set up members of the STARTUPINFO structure.
        // This structure specifies the STDIN and STDOUT handles for redirection.

        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.hStdError = pipeFromChild[1];
        siStartInfo.hStdOutput = pipeFromChild[1];
        siStartInfo.hStdInput = pipeToChild[0];
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        // Create the child process.
        bSuccess = CreateProcessA(NULL,
                                (LPSTR)programmName.c_str(),    // command line
                                NULL,         // process security attributes
                                NULL,         // primary thread security attributes
                                TRUE,         // handles are inherited
                                0,            // creation flags
                                NULL,         // use parent's environment
                                NULL,         // use parent's current directory
                                &siStartInfo, // STARTUPINFO pointer
                                &piProcInfo); // receives PROCESS_INFORMATIONx
    }

    void SendData(std::string data, bool isRequiredNewLineSymbol = true)
    {
        if (isRequiredNewLineSymbol)
            data += '\n';

        DWORD dwWritten; 
        BOOL bSuccess = FALSE;
        bSuccess = WriteFile(pipeToChild[1], data.c_str(), data.length(), &dwWritten, NULL);
        std::cout << bSuccess << " " << data.length() << std::endl;
    }
};

// g++ subprocess.cpp -lpthread
int main()
{
    Subprocess s;

    s.Launch(".\\test.exe");
    s.SendData("hello gans");
    PeekNamedPipe; // Check how many bytes inside pipe
    WaitNamedPipeA; // Wait data on pipe
}