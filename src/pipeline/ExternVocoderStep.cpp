//=========================================================================
// Name:            ExternVocoderStep.h
// Purpose:         Describes a demodulation step in the audio pipeline.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include <sstream>
#include <vector>
#include <cassert>
#include <functional>
#include "ExternVocoderStep.h"
#include "codec2_fifo.h"
#include "../defines.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif // _WIN32

ExternVocoderStep::ExternVocoderStep(std::string scriptPath, int workingSampleRate, int outputSampleRate)
    : sampleRate_(workingSampleRate)
    , outputSampleRate_(outputSampleRate)
        
#ifdef _WIN32
    , recvProcessHandle_(nullptr)
    , receiveStdoutHandle_(nullptr)
    , receiveStdinHandle_(nullptr)
    , receiveStderrHandle_(nullptr)
#else
    , recvProcessId_(0)
    , receiveStdoutFd_(-1)
    , receiveStdinFd_(-1)
#endif // _WIN32
        
    , inputSampleFifo_(nullptr)
    , outputSampleFifo_(nullptr)
    , isExiting_(false)
    , scriptPath_(scriptPath)
{
    // Create FIFOs so we don't lose any samples during run
    inputSampleFifo_ = codec2_fifo_create(16384);
    assert(inputSampleFifo_ != nullptr);
    
    outputSampleFifo_ = codec2_fifo_create(16384);
    assert(outputSampleFifo_ != nullptr);
    
    // Create process thread
    vocoderProcessHandlerThread_ = std::thread(std::bind(&ExternVocoderStep::threadEntry_, this));
}

ExternVocoderStep::~ExternVocoderStep()
{
    // Stop processing audio samples
    isExiting_ = true;
    vocoderProcessHandlerThread_.join();
    
    if (inputSampleFifo_ != nullptr)
    {
        codec2_fifo_free(inputSampleFifo_);
    }
    
    if (outputSampleFifo_ != nullptr)
    {
        codec2_fifo_free(outputSampleFifo_);
    }
}

int ExternVocoderStep::getInputSampleRate() const
{
    return sampleRate_;
}

int ExternVocoderStep::getOutputSampleRate() const
{
    return outputSampleRate_;
}

std::shared_ptr<short> ExternVocoderStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    const int MAX_OUTPUT_SAMPLES = 1024;
    
    *numOutputSamples = 0;
    short* outputSamples = nullptr;

    // Write input samples to thread for processing
    codec2_fifo_write(inputSampleFifo_, inputSamples.get(), numInputSamples);
    
    // Read and return output samples from thread.
    *numOutputSamples = codec2_fifo_used(outputSampleFifo_);
    if (*numOutputSamples > 0)
    {
        *numOutputSamples = std::min(*numOutputSamples, MAX_OUTPUT_SAMPLES);
        outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);
        
        codec2_fifo_read(outputSampleFifo_, outputSamples, *numOutputSamples);
    }

    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}

#ifdef _WIN32
void ExternVocoderStep::openProcess_()
{
    // Ensure that handles are inherited by the child process.
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 
    
    HANDLE tmpStdoutWrHandle = NULL;
    HANDLE tmpStderrWrHandle = NULL;
    HANDLE tmpStdinRdHandle = NULL;
    
    // Create a pipe for the child process's STDOUT. 
    if (!CreatePipe(&receiveStdoutHandle_, &tmpStdoutWrHandle, &saAttr, 0) || 
        !SetHandleInformation(receiveStdoutHandle_, HANDLE_FLAG_INHERIT, 0))
    {
        fprintf(stderr, "WARNING: cannot create pipe for stdout!\n");
        if (receiveStdoutHandle_ != nullptr) 
        {
            CloseHandle(receiveStdoutHandle_);
            CloseHandle(tmpStdoutWrHandle);
            receiveStdoutHandle_ = nullptr;
        }
        return;
    }
    
    // Create a pipe for the child process's STDERR. 
    if (!CreatePipe(&receiveStderrHandle_, &tmpStderrWrHandle, &saAttr, 0) || 
        !SetHandleInformation(receiveStderrHandle_, HANDLE_FLAG_INHERIT, 0))
    {
        fprintf(stderr, "WARNING: cannot create pipe for stdout!\n");
        if (receiveStderrHandle_ != nullptr) 
        {
            CloseHandle(receiveStderrHandle_);
            CloseHandle(tmpStderrWrHandle);
            receiveStderrHandle_ = nullptr;
        }
        return;
    }

    // Create a pipe for the child process's STDIN. 
    if (!CreatePipe(&tmpStdinRdHandle, &receiveStdinHandle_, &saAttr, 0) ||
        !SetHandleInformation(receiveStdinHandle_, HANDLE_FLAG_INHERIT, 0))
    {
        fprintf(stderr, "WARNING: cannot create pipe for stdin!\n");
        if (receiveStdinHandle_ != nullptr) 
        {
            CloseHandle(receiveStdinHandle_);
            CloseHandle(tmpStdinRdHandle);
            receiveStdinHandle_ = nullptr;
        }
        return;
    }
    
    // Create process
    PROCESS_INFORMATION piProcInfo; 
    STARTUPINFOA siStartInfo;
    
    // Set up members of the PROCESS_INFORMATION structure. 
    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
 
    // Set up members of the STARTUPINFO structure. 
    // This structure specifies the STDIN and STDOUT handles for redirection.
    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO); 
    siStartInfo.hStdError = tmpStderrWrHandle;
    siStartInfo.hStdOutput = tmpStdoutWrHandle;
    siStartInfo.hStdInput = tmpStdinRdHandle;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    
    BOOL success = CreateProcessA(NULL, 
          (char*)scriptPath_.c_str(),     // command line 
          NULL,          // process security attributes 
          NULL,          // primary thread security attributes 
          TRUE,          // handles are inherited 
          0,             // creation flags 
          NULL,          // use parent's environment 
          NULL,          // use parent's current directory 
          &siStartInfo,  // STARTUPINFO pointer 
          &piProcInfo);  // receives PROCESS_INFORMATION 
    
    CloseHandle(tmpStdinRdHandle);
    CloseHandle(tmpStdoutWrHandle);
    CloseHandle(tmpStderrWrHandle);
    
    if (!success)
    {
        fprintf(stderr, "WARNING: cannot run %s\n", scriptPath_.c_str());
        
        CloseHandle(receiveStdinHandle_);
        CloseHandle(receiveStdoutHandle_);
        CloseHandle(receiveStderrHandle_);
        receiveStdinHandle_ = NULL;
        receiveStdoutHandle_ = NULL;
        receiveStderrHandle_ = NULL;
        
        return;
    }
    
    // Save process handle so we can terminate it later
    recvProcessHandle_ = piProcInfo.hProcess;
}

void ExternVocoderStep::closeProcess_()
{
    if (recvProcessHandle_ != NULL)
    {
        CloseHandle(receiveStdinHandle_);
        CloseHandle(receiveStdoutHandle_);
        CloseHandle(receiveStderrHandle_);
        TerminateProcess(recvProcessHandle_, 0);
    
        // Make sure process has actually terminated
        WaitForSingleObject(recvProcessHandle_, INFINITE);
    }
}

void ExternVocoderStep::threadEntry_()
{
    const int NUM_SAMPLES_TO_READ_WRITE = 1;
    
    openProcess_();
    
    while (!isExiting_)
    {
        bool operationCompleted = false;
        
        // Read data from STDERR if available
        DWORD bytesAvailable = 0;
        if (PeekNamedPipe(receiveStderrHandle_, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0)
        {
            char buf[bytesAvailable];
            ReadFile(receiveStderrHandle_, buf, bytesAvailable, NULL, NULL);
            fprintf(stderr, "%s", buf);
            
            operationCompleted = true;
        }
        
        // Read data from STDOUT if available
        bytesAvailable = 0;
        if (PeekNamedPipe(receiveStdoutHandle_, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0)
        {
            char buf[bytesAvailable];
            ReadFile(receiveStdoutHandle_, buf, bytesAvailable, NULL, NULL);
            codec2_fifo_write(outputSampleFifo_, (short*)&buf[0], bytesAvailable / sizeof(short));
            
            operationCompleted = true;
        }
        
        // Write data to STDIN if available
        if (codec2_fifo_used(inputSampleFifo_) >= NUM_SAMPLES_TO_READ_WRITE)
        {
            short val[NUM_SAMPLES_TO_READ_WRITE];
            codec2_fifo_read(inputSampleFifo_, val, NUM_SAMPLES_TO_READ_WRITE);
            WriteFile(receiveStdinHandle_, val, NUM_SAMPLES_TO_READ_WRITE * sizeof(short), NULL, NULL);
            
            operationCompleted = true;
        }
        
        // Wait 10ms if we didn't do anything in this round
        if (!operationCompleted)
        {
            Sleep(10);
        }
    }
    
    closeProcess_();
}

#else
void ExternVocoderStep::openProcess_()
{
    // Create pipes for stdin/stdout
    int stdinPipes[2];
    int stdoutPipes[2];
    int rv = pipe(stdinPipes);
    assert(rv == 0);
    rv = pipe(stdoutPipes);
    assert(rv == 0);

    receiveStdoutFd_ = stdoutPipes[0];
    receiveStdinFd_ = stdinPipes[1];

    // Make pipes non-blocking.
    /*int flags = fcntl(receiveStdoutFd_, F_GETFL, 0);
    fcntl(receiveStdoutFd_, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(receiveStdinFd_, F_GETFL, 0);
    fcntl(receiveStdinFd_, F_SETFL, flags | O_NONBLOCK);*/

    // Start external process
    recvProcessId_ = fork();
    if (recvProcessId_ == 0)
    {
        // Child process, redirect stdin/stdout and execute receiver
        rv = dup2(stdoutPipes[1], STDOUT_FILENO);
        assert(rv != -1);
        rv = dup2(stdinPipes[0], STDIN_FILENO);
        assert(rv != -1);

        close(stdoutPipes[0]);
        close(stdinPipes[1]);

        // Tokenize and generate an argv for exec()
        std::vector<std::string> args;
        std::stringstream ss(scriptPath_);
        std::string tmp;
        while(std::getline(ss, tmp, ' '))
        {
            args.push_back(tmp);
        }        

        char** argv = new char*[args.size() + 1];
        assert(argv != nullptr);
        int index = 0;
        for (auto& arg : args)
        {
            fprintf(stderr, "arg %d: %s\n", index, arg.c_str());
            argv[index] = new char[arg.size() + 1];
            assert(argv[index] != nullptr);

            memset(argv[index], 0, arg.size() + 1);
            strncpy(argv[index], arg.c_str(), arg.size());

            index++;
        }

        argv[index] = nullptr;

        // Should not normally return.
        execv(argv[0], argv);
        fprintf(stderr, "WARNING: could not run %s (errno %d)\n", scriptPath_.c_str(), errno);
        exit(-1);
    }
    else
    {
        close(stdoutPipes[1]);
        close(stdinPipes[0]);
    }
}

void ExternVocoderStep::closeProcess_()
{
    // Close pipes and kill process. Hopefully the process
    // will die on its own but if it doesn't, force kill it.
    close(receiveStdoutFd_);
    close(receiveStdinFd_);

    kill(recvProcessId_, SIGTERM);
    for (int count = 0; count < 5 && waitpid(recvProcessId_, NULL, WNOHANG) <= 0; count++)
    {
        sleep(1);
    }
    kill(recvProcessId_, SIGKILL);
    waitpid(recvProcessId_, NULL, 0);
}

void ExternVocoderStep::threadEntry_()
{
    const int NUM_SAMPLES_TO_READ_WRITE = 1;
    
    openProcess_();
    
    while (!isExiting_)
    {
        fd_set processReadFds;
        fd_set processWriteFds;
        FD_ZERO(&processReadFds);
        FD_ZERO(&processWriteFds);
        
        FD_SET(receiveStdinFd_, &processWriteFds);
        FD_SET(receiveStdoutFd_, &processReadFds);
        
        // 10ms timeout
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000;
        
        int rv = select(std::max(receiveStdinFd_, receiveStdoutFd_) + 1, &processReadFds, &processWriteFds, nullptr, &tv);
        if (rv > 0)
        {
            if (FD_ISSET(receiveStdinFd_, &processWriteFds) && codec2_fifo_used(inputSampleFifo_) >= NUM_SAMPLES_TO_READ_WRITE)
            {
                // Can write to process
                short val[NUM_SAMPLES_TO_READ_WRITE];
                codec2_fifo_read(inputSampleFifo_, val, NUM_SAMPLES_TO_READ_WRITE);
                write(receiveStdinFd_, val, NUM_SAMPLES_TO_READ_WRITE * sizeof(short));
            }
            
            if (FD_ISSET(receiveStdoutFd_, &processReadFds))
            {
                short output_buf[NUM_SAMPLES_TO_READ_WRITE];
                if ((rv = read(receiveStdoutFd_, output_buf, NUM_SAMPLES_TO_READ_WRITE * sizeof(short))) > 0)
                {
                    codec2_fifo_write(outputSampleFifo_, output_buf, rv / sizeof(short));
                }
            }
        }
    }
    
    closeProcess_();
}
#endif // _WIN32
