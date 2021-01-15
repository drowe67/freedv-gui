/*
  Sending simple message via UDP

  http://cool-emerald.blogspot.com.au/2018/01/udptcp-socket-programming-with-wxwidgets.html#udp
*/

#include "main.h"

// UDP socket available to send messages
wxDatagramSocket *g_sock;

void UDPInit(void) {
    // Create the socket

    wxIPV4address addr_rx;
    addr_rx.AnyAddress();
    //addr_rx.Service(3000);
    g_sock = new wxDatagramSocket(addr_rx);

    // We use IsOk() here to see if the server is really listening

    if (!g_sock->IsOk()) {
        fprintf(stderr, "UDPInit: Could not listen at the specified port !\n");
        return;
    }

    wxIPV4address addrReal;
    if (!g_sock->GetLocal(addrReal)){
        fprintf(stderr, "UDPInit: Couldn't get the address we bound to\n");
    }
    else {
        fprintf(stderr, "Server listening at %s:%u \n", (const char*)addrReal.IPAddress().c_str(), addrReal.Service());
    }
}

void UDPSend(int port, char *buf, unsigned int n) {
    fprintf(stderr, "UDPSend buf: %s n: %d\n", buf, n);

    wxIPV4address addr_tx;
    addr_tx.Hostname("localhost");
    addr_tx.Service(port);

    if ( g_sock->SendTo(addr_tx, (const void*)buf, n).LastCount() != n ) {
        fprintf(stderr, "UDPSend: failed to send data");
        return;
    }
}
