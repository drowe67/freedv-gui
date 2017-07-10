#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "serialport.h"

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
    if (com_handle != COM_HANDLE_INVALID) {
        closeport();
    }

    m_useRTS = useRTS;
    m_RTSPos = RTSPos;
    m_useDTR = useDTR;
    m_DTRPos = DTRPos;

#ifdef _WIN32
	{
		COMMCONFIG CC;
		DWORD CCsize=sizeof(CC);
		COMMTIMEOUTS timeouts;
		DCB	dcb;

		if(GetDefaultCommConfigA(name, &CC, &CCsize)) {
			CC.dcb.fOutxCtsFlow		= FALSE;
			CC.dcb.fOutxDsrFlow		= FALSE;
			CC.dcb.fDtrControl		= DTR_CONTROL_DISABLE;
			CC.dcb.fDsrSensitivity	= FALSE;
			CC.dcb.fRtsControl		= RTS_CONTROL_DISABLE;
			SetDefaultCommConfigA(name, &CC, CCsize);
		}

		if((com_handle=CreateFileA(name
			,GENERIC_READ|GENERIC_WRITE 	/* Access */
			,0								/* Share mode */
			,NULL							/* Security attributes */
			,OPEN_EXISTING					/* Create access */
			,FILE_ATTRIBUTE_NORMAL			/* File attributes */
			,NULL							/* Template */
			))==INVALID_HANDLE_VALUE)
			return false;

		if(GetCommTimeouts(com_handle, &timeouts)) {
			timeouts.ReadIntervalTimeout=MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier=0;
			timeouts.ReadTotalTimeoutConstant=0;		// No-wait read timeout
			timeouts.WriteTotalTimeoutMultiplier=0;
			timeouts.WriteTotalTimeoutConstant=5000;	// 5 seconds
			SetCommTimeouts(com_handle,&timeouts);
		}

		/* Force N-8-1 mode: */
		if(GetCommState(com_handle, &dcb)==TRUE) {
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
			SetCommState(com_handle, &dcb);
		}
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
