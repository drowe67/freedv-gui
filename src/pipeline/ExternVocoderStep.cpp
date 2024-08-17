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

#include <sys/wait.h>
#include <sstream>
#include <vector>
#include <cassert>
#include "ExternVocoderStep.h"
#include "codec2_fifo.h"
#include "../defines.h"

ExternVocoderStep::ExternVocoderStep(std::string scriptPath, int workingSampleRate, int outputSampleRate)
    : sampleRate_(workingSampleRate)
    , outputSampleRate_(outputSampleRate)
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
    int flags = fcntl(receiveStdoutFd_, F_GETFL, 0);
    fcntl(receiveStdoutFd_, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(receiveStdinFd_, F_GETFL, 0);
    fcntl(receiveStdinFd_, F_SETFL, flags | O_NONBLOCK);

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
        std::stringstream ss(scriptPath);
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
        fprintf(stderr, "WARNING: could not run %s (errno %d)\n", scriptPath.c_str(), errno);
        exit(-1);
    }
    else
    {
        close(stdoutPipes[1]);
        close(stdinPipes[0]);
    }
}

ExternVocoderStep::~ExternVocoderStep()
{
    // Close pipes and wait for process to terminate.
    close(receiveStdoutFd_);
    close(receiveStdinFd_);
    waitpid(recvProcessId_, NULL, 0);
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
    const int MIN_NUM_SAMPLES_TO_READ = 1024;
    int samplesToRead = std::max(MIN_NUM_SAMPLES_TO_READ, numInputSamples);

    *numOutputSamples = 0;
    short* outputSamples = nullptr;
    
    short output_buf[samplesToRead];
    short* inputPtr = inputSamples.get();

    if (numInputSamples > 0)
    {
        write(receiveStdinFd_, inputPtr, numInputSamples * sizeof(short));
    }
    *numOutputSamples = read(receiveStdoutFd_, output_buf, samplesToRead * sizeof(short)); 
    if (*numOutputSamples == -1)
    {
        *numOutputSamples = 0;
    }
    else
    {
        *numOutputSamples /= sizeof(short);

        outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);
        
        memcpy(outputSamples, output_buf, *numOutputSamples * sizeof(short));
    }

    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
