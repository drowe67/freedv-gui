import smtplib
import socket
import sys

if len(sys.argv) != 3:
  print 'usage: %s email_addr email_pass' % sys.argv[0]
  exit(0)
  
email_addr = sys.argv[1]
email_pass = sys.argv[2]

# Create a TCP/IP socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to the port

server_address = ('localhost', 3000)
print >>sys.stderr, 'starting up on %s port %s' % server_address
sock.bind(server_address)

while True:
    print >>sys.stderr, '\nwaiting to receive message'
    data, address = sock.recvfrom(4096)
    
    print >>sys.stderr, 'received %s bytes from %s' % (len(data), address)
    print >>sys.stderr, data
    
    if data:
        # send gmail
        server = smtplib.SMTP('smtp.gmail.com', 587)
        server.starttls()
        server.login(email_addr, email_pass)
 
        msg = "FreeDV has sync"
        server.sendmail(email_addr, email_addr, msg)
        server.quit()

