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
#include <tlhelp32.h>
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
    if (numInputSamples > 0)
    {
        codec2_fifo_write(inputSampleFifo_, inputSamples.get(), numInputSamples);
    }
 
    // Read and return output samples from thread.
    *numOutputSamples = codec2_fifo_used(outputSampleFifo_);
    *numOutputSamples = std::min(*numOutputSamples, numInputSamples * getOutputSampleRate() / getInputSampleRate());
    if (*numOutputSamples > 0)
    {
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
    
    // Create pipes for the child process's stdin/stdout/stderr.
    CreateAsyncPipe_(&receiveStdoutHandle_, &tmpStdoutWrHandle);
    CreateAsyncPipe_(&receiveStderrHandle_, &tmpStderrWrHandle);
    CreateAsyncPipe_(&tmpStdinRdHandle, &receiveStdinHandle_, true);
    
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
    recvProcessId_ = piProcInfo.dwProcessId;
}

void ExternVocoderStep::closeProcess_()
{
    if (recvProcessHandle_ != NULL)
    {
        //KillProcessTree_(recvProcessId_);

        // Make sure process has actually terminated
        TerminateProcess(recvProcessHandle_, 0);
        WaitForSingleObject(recvProcessHandle_, 1000);
        CloseHandle(recvProcessHandle_);

        // Close pipe handles
        CloseHandle(receiveStdinHandle_);
        CloseHandle(receiveStdoutHandle_);
        CloseHandle(receiveStderrHandle_);
    }
}

void ExternVocoderStep::KillProcessTree_(DWORD myprocID)
{
    PROCESSENTRY32 pe;

    memset(&pe, 0, sizeof(PROCESSENTRY32));
    pe.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (::Process32First(hSnap, &pe))
    {
        do // Recursion
        {
            if (pe.th32ParentProcessID == myprocID)
                KillProcessTree_(pe.th32ProcessID);
        } while (::Process32Next(hSnap, &pe));
    }


    // kill the main process
    HANDLE hProc = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, myprocID);

    if (hProc)
    {
        fprintf(stderr, "killing process ID %lu\n", myprocID);
        ::TerminateProcess(hProc, 0);
        ::CloseHandle(hProc);
    }
}

void ExternVocoderStep::threadEntry_()
{
    const int NUM_SAMPLES_TO_READ_WRITE = 512;
    const int BYTES_TO_READ_WRITE = NUM_SAMPLES_TO_READ_WRITE * sizeof(short);
    const int STDERR_BYTES_TO_READ = 4096;
 
    openProcess_();

    // Kick off reading from stdout/stderr
    char* stdoutBuffer = new char[BYTES_TO_READ_WRITE];
    assert(stdoutBuffer != nullptr);
    char* stderrBuffer = new char[STDERR_BYTES_TO_READ];
    assert(stderrBuffer != nullptr);
    FileReadBuffer* stdoutRead = new FileReadBuffer();
    assert(stdoutRead != nullptr);
    FileReadBuffer* stderrRead = new FileReadBuffer();
    assert(stderrRead != nullptr);

    std::function<void(char*, size_t)> onStdoutRead = [&](char* buf, size_t bytesAvailable) {
        codec2_fifo_write(outputSampleFifo_, (short*)&buf[0], bytesAvailable / sizeof(short));
    };

    std::function<void(char*, size_t)> onStderrRead = [&](char* buf, size_t bytesAvailable) {
        std::string tmp(buf, bytesAvailable);
        size_t start_pos = 0;
        while((start_pos = tmp.find("\n", start_pos)) != std::string::npos) {
            tmp.replace(start_pos, 1, "\r\n");
            start_pos += 2;
        }
        fprintf(stderr, "%s", tmp.c_str());
    };

    InitFileReadBuffer_(stdoutRead, receiveStdoutHandle_, stdoutBuffer, BYTES_TO_READ_WRITE, onStdoutRead);
    ScheduleFileRead_(stdoutRead);
    InitFileReadBuffer_(stderrRead, receiveStderrHandle_, stderrBuffer, STDERR_BYTES_TO_READ, onStderrRead);
    ScheduleFileRead_(stderrRead);

    while (!isExiting_ && !stdoutRead->eof && !stderrRead->eof)
    {
        // Write data to STDIN if available
        int used = codec2_fifo_used(inputSampleFifo_);
        if (used > 0)
        {
            used = std::min(used, 2048); // 4K max buffer size
            short val[used];
            codec2_fifo_read(inputSampleFifo_, val, used);
            WriteFile(receiveStdinHandle_, val, used * sizeof(short), NULL, NULL);
        }
        else
        {
            // Wait 10ms if we didn't do anything in this round
            Sleep(10);
        }
    }
    
    closeProcess_();

    // Make sure eof is actually set
    for (int count = 0; count < 5; count++)
    {
        if (stdoutRead->eof && stderrRead->eof) break;
        Sleep(1000);
    }

    delete stdoutRead;
    delete[] stdoutBuffer;
    delete stderrRead;
    delete[] stderrBuffer;
}

void ExternVocoderStep::CreateAsyncPipe_(HANDLE* outRead, HANDLE* outWrite, bool inv)
{
    const int PIPE_SIZE = (4096 + 24); // https://github.com/brettwooldridge/NuProcess/issues/117

    // Generate unique name for the pipe.
    UUID uuid;
    UuidCreate(&uuid);
    char* uuidAsString;
    UuidToStringA(&uuid, (RPC_CSTR*)&uuidAsString);
    char pipeName[1024];
    sprintf(pipeName, "\\\\.\\pipe\\freedv-%s", uuidAsString);
    RpcStringFreeA((RPC_CSTR*)&uuidAsString);

    // Create a pipe. The "instances" parameter is set to 2 because we call this function twice below.
    constexpr DWORD Instances = 2;
    // Create the named pipe. This will return the handle we use for reading from the pipe.
    HANDLE read = CreateNamedPipeA(
        pipeName,
        // Set FILE_FLAG_OVERLAPPED to enable async I/O for reading from the pipe.
        // Note that we still need to set PIPE_WAIT.
        (inv ? PIPE_ACCESS_OUTBOUND : PIPE_ACCESS_INBOUND) | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        Instances,
        // in-bound buffer size
        inv ? 0 : PIPE_SIZE,
        // out-going buffer size
        inv ? PIPE_SIZE : 0,
        // default timeout for some functions we're not using
        0,
        nullptr
    );
    if (read == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Failed to create named pipe (error %lu)", GetLastError());
        return;
    }

    // Now create a handle for the other end of the pipe. We are going to pass that handle to the
    // process we are creating, so we need to specify that the handle can be inherited.
    // Also note that we are NOT setting FILE_FLAG_OVERLAPPED. We could set it, but that's not relevant
    // for our end of the pipe. (We do not expect async writes.)
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    HANDLE write = CreateFileA(pipeName, (inv ? GENERIC_READ : GENERIC_WRITE), 0, &saAttr, OPEN_EXISTING, 0, 0);
    if (write == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Failed to open named pipe (error %lu)", GetLastError());
        CloseHandle(read);
        return;
    }

    if (inv)
    {
        *outRead = write;
        *outWrite = read;
    }
    else
    {
        *outRead = read;
        *outWrite = write;
    }
}

void ExternVocoderStep::ScheduleFileRead_(FileReadBuffer* readBuffer)
{
    // Prepare the threadpool for an I/O request on our handle.
    StartThreadpoolIo(readBuffer->Io);
    BOOL success = ReadFile(
        readBuffer->FileHandle,
        readBuffer->Buffer,
        readBuffer->BufferSize,
        nullptr,
        &readBuffer->Overlapped
    );
    if (!success ) {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            // Async operation is in progress. This is NOT a failure state.
            return;
        }
        // Since we have started an I/O request above but nothing happened, we need to cancel it.
        CancelThreadpoolIo(readBuffer->Io);

        if (error == ERROR_INVALID_USER_BUFFER || error == ERROR_NOT_ENOUGH_MEMORY) {
            // Too many outstanding async I/O requests, try again after 10 ms.
            // The timer length is given in 100ns increments, negative values indicate relative
            // values. FILETIME is actually an unsigned value. Sigh.
            constexpr int ToMicro = 10;
            constexpr int ToMilli = 1000;
            constexpr int64_t Delay = -(10 * ToMicro * ToMilli);
            FILETIME timerLength{};
            timerLength.dwHighDateTime = (Delay >> 32) & 0xFFFFFFFF;
            timerLength.dwLowDateTime = Delay & 0xFFFFFFFF;
            SetThreadpoolTimer(readBuffer->Timer, &timerLength, 0, 0);
            return;
        }
        CloseThreadpoolTimer(readBuffer->Timer);
        CloseThreadpoolIo(readBuffer->Io);
        if (error == ERROR_BROKEN_PIPE)
        {
            readBuffer->eof = true;
            return;
        }
        if(error != ERROR_OPERATION_ABORTED)
        {
            fprintf(stderr, "ReadFile async failed, error code %lu", error);
        }
    }
}

void CALLBACK ExternVocoderStep::FileReadComplete_(
    PTP_CALLBACK_INSTANCE instance,
    void* context,
    void* overlapped,
    ULONG ioResult,
    ULONG_PTR numBytesRead,
    PTP_IO io
    )
{
    FileReadBuffer* readBuffer = (FileReadBuffer*)context;
    if (ioResult == ERROR_OPERATION_ABORTED) {
        // This can happen when someone manually aborts the I/O request.
        CloseThreadpoolTimer(readBuffer->Timer);
        CloseThreadpoolIo(readBuffer->Io);
        return;
    }
    const bool isEof = ioResult == ERROR_HANDLE_EOF || ioResult == ERROR_BROKEN_PIPE;
    if (!(isEof || ioResult == NO_ERROR))
    {
        fprintf(stderr, "Got error result %lu while handling I/O callback", ioResult);
        readBuffer->eof = true;
        return;
    }

    readBuffer->onDataRead((char*)readBuffer->Buffer, numBytesRead);

    if (isEof) {
        CloseThreadpoolTimer(readBuffer->Timer);
        CloseThreadpoolIo(readBuffer->Io);

        readBuffer->eof = true;
    } else {
        // continue reading
        ScheduleFileRead_(readBuffer);
    }
}

void ExternVocoderStep::InitFileReadBuffer_(FileReadBuffer* readBuffer, HANDLE handle, void* buffer, size_t bufferSize, std::function<void(char*, size_t)> onDataRead)
{
    ZeroMemory(readBuffer, sizeof(*readBuffer));
    readBuffer->onDataRead = onDataRead;
    readBuffer->Buffer = buffer;
    readBuffer->BufferSize = bufferSize;
    readBuffer->FileHandle = handle;
    readBuffer->Io = CreateThreadpoolIo(handle, &FileReadComplete_, readBuffer, nullptr);
    if (!(readBuffer->Io))
    {
        fprintf(stderr, "CreateThreadpoolIo failed, error code %lu", GetLastError());
        return;
    }

    // This local struct just exists so I can declare a function here that we can pass to the timer below.
    struct Tmp {
        static void CALLBACK RetryScheduleFileRead(
            PTP_CALLBACK_INSTANCE instance,
            void* context,
            PTP_TIMER wait
        ) {
            ScheduleFileRead_((FileReadBuffer*)context);
        }
    };

    readBuffer->Timer = CreateThreadpoolTimer(&Tmp::RetryScheduleFileRead, readBuffer, nullptr);
    if (!(readBuffer->Timer))
    {
        fprintf(stderr, "CreateThreadpoolTimer failed, error code %lu", GetLastError());
    }
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
