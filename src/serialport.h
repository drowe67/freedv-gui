#ifndef SERIALPORT_H
#define SERIALPORT_H

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#endif

#include <functional>
#include <wx/thread.h>

// Serial ports called com port for historic reasons, especially on Windows machines 

#ifdef _WIN32
#define COM_HANDLE_INVALID			INVALID_HANDLE_VALUE
typedef HANDLE      com_handle_t;
#else
#define COM_HANDLE_INVALID			-1
typedef int         com_handle_t;
#endif

#define PTT_INPUT_MONITORING_TIME_MS 10

class Serialport {

    public:
        Serialport();
        ~Serialport();
        bool openport(const char port[], bool useRTS, bool RTSPos, bool useDTR, bool DTRPos);
        bool isopen() {return (com_handle != COM_HANDLE_INVALID);}
        void closeport();
        void ptt(bool tx); 
        
        void enablePttInputMonitoring(bool ctsPos, std::function<void(bool)> pttChangeFn);
        
    private:
        class PttMonitorThread : public wxThread
        {
        public:
            PttMonitorThread(Serialport* port, bool ctsPos)
                : wxThread()
                , m_port(port)
                , m_ctsPos(ctsPos) { }
            virtual ~PttMonitorThread() { }
            
        protected:
            virtual ExitCode Entry();
            
        private:
            Serialport* m_port;
            bool m_ctsPos;
        };       
        
        com_handle_t  com_handle;
        bool          m_useRTS, m_RTSPos, m_useDTR, m_DTRPos;
        Serialport::PttMonitorThread* m_pttMonitoringThread;
        bool m_currentPttInputState;
        bool m_pttMonitorThreadEnding;
        std::function<void(bool)> m_pttChangeFn;
        
        void raiseDTR(void);
        void lowerDTR(void);
        void raiseRTS(void);
        void lowerRTS(void);
        
        bool getCTS(void); 
};

#endif /* SERIALPORT_H */
