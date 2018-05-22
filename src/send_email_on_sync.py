#!/usr/bin/env python
#
#   FreeDV Sync Notification Script
#
from __future__ import print_function
import argparse
import smtplib
import socket
import sys
import time


# Clean way of printing to stderr...
def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='FreeDV Sync to E-Mail Gateway')
    parser.add_argument('email_addr', type=str, help="User e-mail address.")
    parser.add_argument('email_pass', type=str, help="User e-mail SMTP password.")
    parser.add_argument('--limit', type=int, default=5, help="Limit e-mails to once every X minutes. Default = 5")
    parser.add_argument('--listen_port', type=int, default=3000, help="Listen on port X. Default: 3000")
    parser.add_argument('--smtp_server', type=str, default="smtp.gmail.com", help="E-Mail SMTP Server. Default: gmail")
    parser.add_argument('--smtp_port', type=int, default=587, help="SMTP Port. Default: 587 (SSL)")
    args = parser.parse_args()


    # Create a UDP/IP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Bind the socket to the port
    server_address = ('localhost', args.listen_port)
    eprint("Starting up on %s port %s" % (server_address[0], server_address[1]))
    sock.bind(server_address)

    _last_email = 0

    while True:
        eprint('\nwaiting to receive message')
        data, address = sock.recvfrom(4096)
        
        eprint('received %s bytes from %s' % (len(data), address))
        eprint(data)
        
        if data:
            # Check if we are allowed to send.
            _now = time.time()
            if (_now - _last_email) > args.limit*60:
                try:
                    # send gmail
                    server = smtplib.SMTP(args.smtp_server, args.smtp_port)
                    server.starttls()
                    server.login(args.email_addr, args.email_pass)
             
                    msg = "FreeDV has sync"
                    server.sendmail(args.email_addr, args.email_addr, msg)
                    server.quit()

                except Exception as e:
                    # Lazy exception handling...
                    eprint("Error when sending e-mail - %s" % str(e))

                _last_email = time.time()
            else:
                eprint("Error: Not long enough since last e-mail.")
