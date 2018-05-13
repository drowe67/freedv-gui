#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "serialport.h"

#ifdef _WIN32
#include <strsafe.h>
#endif

Serialport::Serialport() {
    com_handle = COM_HANDLE_INVALID;
}

Serialport::~Serialport() {
    if (isopen()) {
        closeport();
    }
}

// returns true if comm port opened OK, false if there was a problem

bool Serialport::openport(const char name[], bool useRTS, bool RTSPos, bool useDTR, bool DTRPos)
{
    fprintf(stderr, "starting openport(), name: %s strlen(name): %d useRTS: %d RTSPos: %d useDTR: %d DTRPos: %d\n",
            name, (int)strlen(name), useRTS, RTSPos, useDTR, DTRPos);

    if (com_handle != COM_HANDLE_INVALID) {
        closeport();
        fprintf(stderr, "comm_handle invalid, closing\n");
    }

    m_useRTS = useRTS;
    m_RTSPos = RTSPos;
    m_useDTR = useDTR;
    m_DTRPos = DTRPos;

    
#ifdef _WIN32
    {
        COMMTIMEOUTS timeouts;
        DCB	dcb;
        TCHAR  lpszFunction[100];
	
        // As per:
        //   [1] https://support.microsoft.com/en-us/help/115831/howto-specify-serial-ports-larger-than-com9
        //   [2] Hamlib lib/termios.c, win32_serial_open()

	/*  
            To test change of COM port for USB serial device on Windows

            1/ Run->devmgmnt.msc
            2/ Change COM port Ports (COM & LPT) -> Serial Device -> Properties Tab -> Advanced
            3/ Unplug USB serial devce and plug in again.  This is really important.  FreeDV won't recognise
               new COM port number until this is done.
            4/ Test PTT on FreeDV Tools->PTT
	*/

        TCHAR  nameWithStrangePrefix[100];
        StringCchPrintf(nameWithStrangePrefix, 100, "\\\\.\\%s", name);
        fputs("nameWithStrangePrefix: ", stderr);
	fputs(nameWithStrangePrefix, stderr);
	fprintf(stderr,"\n");

#ifdef NOT_USED

        COMMCONFIG CC;
        DWORD CCsize=sizeof(CC);
	memset(&CC, 0, CCsize);
	CC.dwSize = CCsize;

	/* Commented out by David May 13 2018, as it was failing after
           "name" changed to "nameWithStrangePrefix" that is reqd for
           support of COM ports above 9.  I am not sure if this is
           needed as I can't see anything similar in Hamlib */
	
        if(GetDefaultCommConfigA(nameWithStrangePrefix, &CC, &CCsize)) {
	    fprintf(stderr, "GetDefaultCommConfigA OK\n");         
            CC.dcb.fOutxCtsFlow		= FALSE;
            CC.dcb.fOutxDsrFlow		= FALSE;
            CC.dcb.fDtrControl		= DTR_CONTROL_DISABLE;
            CC.dcb.fDsrSensitivity	= FALSE;
            CC.dcb.fRtsControl		= RTS_CONTROL_DISABLE;
            if (!SetDefaultCommConfigA(nameWithStrangePrefix, &CC, CCsize)) {
		StringCchPrintf(lpszFunction, 100, "%s", "SetDefaultCommConfigA");
                goto error;
            }
            fprintf(stderr, "SetDefaultCommConfigA OK\n");
        } else {
	     StringCchPrintf(lpszFunction, 100, "%s", "GetDefaultCommConfigA");
             goto error;
        }
#endif
	
        if((com_handle=CreateFileA(nameWithStrangePrefix
                                   ,GENERIC_READ | GENERIC_WRITE/* Access */
                                   ,0				/* Share mode */
                                   ,NULL		 	/* Security attributes */
                                   ,OPEN_EXISTING		/* Create access */
                                   ,0                           /* File attributes */
                                   ,NULL		        /* Template */
                                   ))==INVALID_HANDLE_VALUE) {
	    StringCchPrintf(lpszFunction, 100, "%s", "CreateFileA");
	    goto error;
	}
        fprintf(stderr, "CreateFileA OK\n");
	
        if(GetCommTimeouts(com_handle, &timeouts)) {
 	    fprintf(stderr, "GetCommTimeouts OK\n");
            timeouts.ReadIntervalTimeout=MAXDWORD;
            timeouts.ReadTotalTimeoutMultiplier=0;
            timeouts.ReadTotalTimeoutConstant=0;		// No-wait read timeout
            timeouts.WriteTotalTimeoutMultiplier=0;
            timeouts.WriteTotalTimeoutConstant=5000;	// 5 seconds
	    if (!SetCommTimeouts(com_handle,&timeouts)) {
	      StringCchPrintf(lpszFunction, 100, "%s", "SetCommTimeouts");
              goto error;	      
	    }
	    fprintf(stderr, "SetCommTimeouts OK\n");
        } else {
	    StringCchPrintf(lpszFunction, 100, "%s", "GetCommTimeouts");
            goto error;
        }

        /* Force N-8-1 mode: */
        if(GetCommState(com_handle, &dcb)==TRUE) {
	    fprintf(stderr, "GetCommState OK\n");
	    
            dcb.ByteSize		= 8;
            dcb.Parity			= NOPARITY;
            dcb.StopBits		= ONESTOPBIT;
            dcb.DCBlength		= sizeof(DCB);
            dcb.fBinary			= TRUE;
            dcb.fOutxCtsFlow	= FALSE;
            dcb.fOutxDsrFlow	= FALSE;
            dcb.fDtrControl		= DTR_CONTROL_DISABLE;
            dcb.fDsrSensitivity	= FALSE;
            dcb.fTXContinueOnXoff= TRUE;
            dcb.fOutX			= FALSE;
            dcb.fInX			= FALSE;
            dcb.fRtsControl		= RTS_CONTROL_DISABLE;
            dcb.fAbortOnError	= FALSE;
            if (!SetCommState(com_handle, &dcb)) {
  	        StringCchPrintf(lpszFunction, 100, "%s", "SetCommState");
                goto error;           
            }
	    fprintf(stderr, "SetCommState OK\n");
        } else {
  	    StringCchPrintf(lpszFunction, 100, "%s", "GetCommState");
            goto error;           
        }
	
	return true;
	
    error:
	fprintf(stderr, "%s failed\n", lpszFunction);
	
        // Retrieve the system error message for the last-error code

        LPVOID lpMsgBuf;
        LPVOID lpDisplayBuf;
        DWORD dw = GetLastError(); 

        FormatMessage(
                      FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      dw,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &lpMsgBuf,
                      0, NULL );

        // Display the error message

        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
                                          (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
        StringCchPrintf((LPTSTR)lpDisplayBuf, 
                        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                        TEXT("%s failed with error %d: %s"), 
                        lpszFunction, dw, lpMsgBuf); 
        MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

        LocalFree(lpMsgBuf);
        LocalFree(lpDisplayBuf);

	return false;
    }
#else
	{
		struct termios t;

		if((com_handle=open(name, O_NONBLOCK|O_RDWR))== COM_HANDLE_INVALID)
			return false;

		if(tcgetattr(com_handle, &t)==-1) {
			close(com_handle);
			com_handle = COM_HANDLE_INVALID;
			return false;
		}

		t.c_iflag = (
					  IGNBRK   /* ignore BREAK condition */
					| IGNPAR   /* ignore (discard) parity errors */
					);
		t.c_oflag = 0;	/* No output processing */
		t.c_cflag = (
					  CS8         /* 8 bits */
					| CREAD       /* enable receiver */

		/*
		Fun snippet from the FreeBSD manpage:

			 If CREAD is set, the receiver is enabled.  Otherwise, no character is
			 received.  Not all hardware supports this bit.  In fact, this flag is
			 pretty silly and if it were not part of the termios specification it
			 would be omitted.
		*/
					| CLOCAL      /* ignore modem status lines */
					);

		t.c_lflag = 0;	/* No local modes */
		if(tcsetattr(com_handle, TCSANOW, &t)==-1) {
			close(com_handle);
			com_handle = COM_HANDLE_INVALID;
			return false;
		}
		
	}
#endif
	return true;
}


// fixme: this takes about one second to close under Linux

void Serialport::closeport()
{
#ifdef _WIN32
	CloseHandle(com_handle);
#else
	close(com_handle);
#endif
        com_handle = COM_HANDLE_INVALID;
}

//----------------------------------------------------------------
// (raise|lower)(RTS|DTR)()
//
// Raises/lowers the specified signal
//----------------------------------------------------------------

void Serialport::raiseDTR(void)
{
	if(com_handle == COM_HANDLE_INVALID)
		return;
#ifdef _WIN32
	EscapeCommFunction(com_handle, SETDTR);
#else
	{	// For C89 happiness
		int flags = TIOCM_DTR;
		ioctl(com_handle, TIOCMBIS, &flags);
	}
#endif
}

void Serialport::raiseRTS(void)
{
	if(com_handle == COM_HANDLE_INVALID)
		return;
#ifdef _WIN32
	EscapeCommFunction(com_handle, SETRTS);
#else
	{	// For C89 happiness
		int flags = TIOCM_RTS;
		ioctl(com_handle, TIOCMBIS, &flags);
	}
#endif
}

void Serialport::lowerDTR(void)
{
	if(com_handle == COM_HANDLE_INVALID)
		return;
#ifdef _WIN32
	EscapeCommFunction(com_handle, CLRDTR);
#else
	{	// For C89 happiness
		int flags = TIOCM_DTR;
		ioctl(com_handle, TIOCMBIC, &flags);
	}
#endif
}

void Serialport::lowerRTS(void)
{
	if(com_handle == COM_HANDLE_INVALID)
		return;
#ifdef _WIN32
	EscapeCommFunction(com_handle, CLRRTS);
#else
	{	// For C89 happiness
		int flags = TIOCM_RTS;
		ioctl(com_handle, TIOCMBIC, &flags);
	}
#endif
}

void Serialport::ptt(bool tx) {

   /*  Truth table:

          g_tx   RTSPos   RTS
          -------------------
          0      1        0
          1      1        1
          0      0        1
          1      0        0

          exclusive NOR
    */

    if (com_handle != COM_HANDLE_INVALID) {
        if (m_useRTS) {
            //fprintf(stderr, "g_tx: %d m_boolRTSPos: %d serialLine: %d\n", g_tx, wxGetApp().m_boolRTSPos, g_tx == wxGetApp().m_boolRTSPos);
            if (tx == m_RTSPos)
                raiseRTS();
            else
                lowerRTS();
        }
        if (m_useDTR) {
            //fprintf(stderr, "g_tx: %d m_boolDTRPos: %d serialLine: %d\n", g_tx, wxGetApp().m_boolDTRPos, g_tx == wxGetApp().m_boolDTRPos);
            if (tx == m_DTRPos)
                raiseDTR();
            else
                lowerDTR();
        }
 
    }
}
