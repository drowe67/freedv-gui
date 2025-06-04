#!/usr/bin/python3

# This software is Copyright (C) 2012 by James C. Ahlstrom, and is
# licensed for use under the GNU General Public License (GPL).
# See http://www.opensource.org.
# Note that there is NO WARRANTY AT ALL.  USE AT YOUR OWN RISK!!

# Henning Paul, DC4HP, ported this software to Python3 and added improvements in April, 2022.
# Thanks Henning!!!

# Modified by Mooneer Salem (4/2/2025) to support killing processes by PID
# when "radio" goes from TX to RX. This is to enable further testing of RADE EOO
# (namely ensuring that EOO is actually sent to the radio by the OS audio drivers
# before the radio goes back to receive).

import sys
import time
import socket
import traceback
import string
import os
import signal

_RIGCTL_PORT = 4532

# Choose which port to use here:
PORT = 4575
#PORT = _RIGCTL_PORT

# This module creates a Hamlib TCP server that implements the rigctl protocol.  To start the server,
# run "python hamlibserver.py" from a command line.  To exit the server, type control-C.  Connect a
# client to the server using localhost and port 4575.  The TCP server will imitate a software defined
# radio, and you can get and set the frequency, etc.

# Only the commands dump_state, freq, mode, ptt and vfo are implemented.

# This is not a real hardware server.  It is meant as sample code to show how to implement the protocol
# in SDR control software.  You can test it with "rigctl -m 2 -r localhost:4575".

#RIGCTLD_PROT_VER
#rig_model
#0
#rxstartf rxendf rxmodes rxlow_power rxhigh_power rxvfo rxant
#0 0 0 0 0 0 0
#txstartf txendf txmodes txlow_power txhigh_power txvfo txant
#0 0 0 0 0 0 0
#modes tuningsteps
#0 0
#modes bandwidth
#0 0
#max_rit
#max_xit
#max_ifshift
#0
#preamp1 preamp2 
#attenuator1 attenuator2
#
#has_get_func
#has_set_func
#has_get_level
#has_set_level
#has_get_parm
#has_set_parm

#modes definitions:
# 0 AM -- Amplitude Modulation
# 1 CW -- CW "normal" sideband
# 2 USB -- Upper Side Band
# 3 LSB -- Lower Side Band
# 4 RTTY -- Radio Teletype
# 5 FM -- "narrow" band FM
# 6 WFM -- broadcast wide FM
# 7 CW "reverse" sideband
# 8 RTTY "reverse" sideband
# 9 AMS -- Amplitude Modulation Synchronous
#10 PKTLSB -- Packet/Digital LSB mode (dedicated port)
#11 PKTUSB -- Packet/Digital USB mode (dedicated port)
#12 PKTFM -- Packet/Digital FM mode (dedicated port)
#13 ECSSUSB -- Exalted Carrier Single Sideband USB
#14 ECSSLSB -- Exalted Carrier Single Sideband LSB
#15 FAX -- Facsimile Mode
#16 SAM -- Synchronous AM double sideband
#17 SAL -- Synchronous AM lower sideband
#18 SAH -- Synchronous AM upper (higher) sideband
#19 DSB -- Double sideband suppressed carrier
#21 FMN -- FM Narrow Kenwood ts990s
#22 PKTAM -- Packet/Digital AM mode e.g. IC7300
#23 P25 -- APCO/P25 VHF,UHF digital mode IC-R8600
#24 D-Star -- VHF,UHF digital mode IC-R8600
#25 dPMR -- digital PMR, VHF,UHF digital mode IC-R8600
#26 NXDN-VN -- VHF,UHF digital mode IC-R8600
#27 NXDN-N -- VHF,UHF digital mode IC-R8600
#28 DCR -- VHF,UHF digital mode IC-R8600
#29 AM-N -- Narrow band AM mode IC-R30
#30 PSK - Kenwood PSK and others
#31 PSKR - Kenwood PSKR and others
#32 DD Mode IC-9700
#33 Yaesu C4FM mode
#34 Yaesu DATA-FM-N
#35 Unfiltered as in PowerSDR
#36 CWN -- Narrow band CW (FT-736R)
#37 IQ mode for a couple of kit rigs

# A possible response to the "dump_state" request
dump1 = """ 2
2
2
150000.000000 1500000000.000000 0x1ff -1 -1 0x10000003 0x3
0 0 0 0 0 0 0
0 0 0 0 0 0 0
0x1ff 1
0x1ff 0
0 0
0x1e 2400
0x2 500
0x1 8000
0x1 2400
0x20 15000
0x20 8000
0x40 230000
0 0
9990
9990
10000
0
10 
10 20 30 
0x3effffff
0x3effffff
0x7fffffff
0x7fffffff
0x7fffffff
0x7fffffff
"""

# Another possible response to the "dump_state" request
dump2 = """ 0
2
2
150000.000000 30000000.000000  0x900af -1 -1 0x10 000003 0x3
0 0 0 0 0 0 0
150000.000000 30000000.000000  0x900af -1 -1 0x10 000003 0x3
0 0 0 0 0 0 0
0 0
0 0
0
0
0
0


0x0
0x0
0x0
0x0
0x0
0
"""

class HamlibHandler:
  """This class is created for each connection to the server.  It services requests from each client"""
  SingleLetters = {		# convert single-letter commands to long commands
    'f':'freq',
    'm':'mode',
    't':'ptt',
    'v':'vfo',
    's':'split_vfo',
    'i':'split_freq',
    'x':'split_mode'
    }
  def __init__(self, app, sock, address, pid):
    self.app = app		# Reference back to the "hardware"
    self.sock = sock
    sock.settimeout(0.0)
    self.address = address
    self.pid = pid
    self.received = b''
    h = self.Handlers = {}
    h[''] = self.ErrProtocol
    h['dump_state']	= self.DumpState
    h['get_freq']	= self.GetFreq
    h['set_freq']	= self.SetFreq
    h['get_mode']	= self.GetMode
    h['set_mode']	= self.SetMode
    h['get_vfo']	= self.GetVfo
    h['set_vfo']	= self.SetVfo
    h['get_ptt']	= self.GetPtt
    h['set_ptt']	= self.SetPtt
    h['get_split_vfo']	= self.GetSplitVfo
    h['set_split_vfo']	= self.SetSplitVfo
    h['get_split_freq']	= self.GetSplitFreq
    h['set_split_freq']	= self.SetSplitFreq
    h['get_split_mode']	= self.GetSplitMode
    h['set_split_mode']	= self.SetSplitMode
  def Send(self, text):
    """Send text back to the client."""
    try:
      self.sock.sendall(bytearray(text.encode()))
    except socket.error:
      self.sock.close()
      self.sock = None
  
  def Reply(self, *args):	# args is name, value, name, value, ..., int
    """Create a string reply of name, value pairs, and an ending integer code."""
    if self.extended:			# Use extended format
      t = "%s:" % self.cmd		# Extended format echoes the command and parameters
      for param in self.params:
        t = "%s %s" % (t, param)
      t += self.extended
      for i in range(0, len(args) - 1, 2):
        t = "%s%s: %s%c" % (t, args[i], args[i+1], self.extended)
      t += "RPRT %d\n" % args[-1]
    elif len(args) > 1:		# Use simple format
      t = ''
      for i in range(1, len(args) - 1, 2):
        t = "%s%s\n" % (t, args[i])
    else:		# No names; just the required integer code
      t = "RPRT %d\n" % args[0]
    print('Reply', t)
    self.Send(t)
  def ErrParam(self):		# Invalid parameter
    self.Reply(-1)
  def UnImplemented(self):	# Command not implemented
    self.Reply(-4)
  def ErrProtocol(self):	# Protocol error
    self.Reply(-8)
  def Process(self):
    """This is the main processing loop, and is called frequently.  It reads and satisfies requests."""
    if not self.sock:
      return 0
    try:	# Read any data from the socket
      text = self.sock.recv(1024)
    except socket.timeout:	# This does not work
      pass
    except socket.error:	# Nothing to read
      pass
    else:					# We got some characters
      self.received += text
    if b'\n' in self.received:	# A complete command ending with newline is available
      cmd, self.received = self.received.split(b'\n', 1)	# Split off the command, save any further characters
    else:
      return 1
    cmd = cmd.decode()
    cmd = cmd.strip()		# Here is our command
    print('Get', cmd)
    if not cmd:			# ??? Indicates a closed connection?
      print('empty command')
      self.sock.close()
      self.sock = None
      return 0
    # Parse the command and call the appropriate handler
    if cmd[0] == '+':			# rigctld Extended Response Protocol
      self.extended = '\n'
      cmd = cmd[1:].strip()
    elif cmd[0] in ';|,':		# rigctld Extended Response Protocol
      self.extended = cmd[0]
      cmd = cmd[1:].strip()
    else:
      self.extended = None
    if cmd[0:1] == '\\':		# long form command starting with backslash
      args = cmd[1:].split()
      self.cmd = args[0]
      self.params = args[1:]
      self.Handlers.get(self.cmd, self.UnImplemented)()
    else:						# single-letter command
      self.params = cmd[1:].strip()
      cmd = cmd[0:1]
      try:
        t = self.SingleLetters[cmd.lower()]
      except KeyError:
        self.UnImplemented()
      else:
        if cmd in string.ascii_uppercase:
          self.cmd = 'set_' + t
        else:
          self.cmd = 'get_' + t
        self.Handlers.get(self.cmd, self.UnImplemented)()
    return 1
  # These are the handlers for each request
  def DumpState(self):
    self.Send(dump2)
  def GetFreq(self):
    self.Reply('Frequency', self.app.freq, 0)
  def SetFreq(self):
    try:
      x = float(self.params)
      self.Reply(0)
    except:
      self.ErrParam()
    else:
      x = int(x + 0.5)
      self.app.freq = x
  def GetMode(self):
    self.Reply('Mode', self.app.mode, 'Passband', self.app.bandwidth, 0)
  def SetMode(self):
    try:
      mode, bw = self.params.split()
      bw = int(float(bw) + 0.5)
      self.Reply(0)
    except:
      self.ErrParam()
    else:
      self.app.mode = mode
      self.app.bandwidth = bw
  def GetVfo(self):
    self.Reply('VFO', self.app.vfo, 0)
  def SetVfo(self):
    try:
      x = self.params.upper()
      self.Reply(0)
    except:
      self.ErrParam()
    else:
      self.app.vfo = x
  def GetPtt(self):
    self.Reply('PTT', self.app.ptt, 0)
  def SetPtt(self):
    try:
      x = int(self.params)
      self.Reply(0)
    except:
      self.ErrParam()
    else:
      if (not x) and self.app.ptt:
          # Sleep for 20ms to match typical SDR behavior + 5ms to account for varying system load.
          # Example: Flex 6000/8000 (https://community.flexradio.com/discussion/8028104/question-regarding-tx-delay)
          time.sleep(25 / 1000)
          os.kill(self.pid, signal.SIGTERM)
      if x:
        self.app.ptt = 1
      else:
        self.app.ptt = 0
  def GetSplitVfo(self):
    self.Reply('SPLIT', self.app.splitenable, 'TXVFO',  self.app.txvfo, 0)
  def SetSplitVfo(self):
    try:
      splitenable, txvfo = self.params.split()
      self.Reply(0)
    except:
      self.ErrParam()
    else:
      self.app.splitenable = splitenable
      self.app.txvfo = txvfo
  def GetSplitFreq(self):
    self.Reply('TX Frequency', self.app.txfreq, 0)
  def SetSplitFreq(self):
    try:
      x = float(self.params)
      self.Reply(0)
    except:
      self.ErrParam()
    else:
      x = int(x + 0.5)
      self.app.txfreq = x
  def GetSplitMode(self):
    self.Reply('TX Mode', self.app.txmode, 'TX Passband', self.app.txbandwidth, 0)
  def SetSplitMode(self):
    try:
      mode, bw = self.params.split()
      bw = int(float(bw) + 0.5)
      self.Reply(0)
    except:
      self.ErrParam()
    else:
      self.app.txmode = mode
      self.app.txbandwidth = bw

class App:
  """This is the main application class.  It listens for connectons from clients and creates a server for each one."""
  def __init__(self, pid):
    self.pid = pid
    self.hamlib_clients = []
    self.hamlib_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        self.hamlib_socket.bind(('localhost', PORT))
    except socket.error:
        print("could not open listening socket")
        sys.exit(-1)
    self.hamlib_socket.settimeout(0.0)
    self.hamlib_socket.listen(0)
    
    # This is the state of the "hardware"
    self.freq = 21200500
    self.mode = 'USB'
    self.bandwidth = 2400
    self.vfo = "VFO"
    self.ptt = 0
    self.splitenable = 0
    self.txvfo = 'VFO'
    self.txfreq = 21200500
    self.txmode = 'USB'
    self.txbandwidth = 2400
  def Run(self):
    while 1:
      time.sleep(0.01)
      try:
        conn, address = self.hamlib_socket.accept()
      except socket.error:
        pass
      else:
        print('Connection from', address)
        self.hamlib_clients.append(HamlibHandler(self, conn, address, self.pid))
      for client in self.hamlib_clients:
        ret = client.Process()
        if not ret:		# False return indicates a closed connection; remove the server
          self.hamlib_clients.remove(client)
          print('Remove', client.address)
          break
      

if __name__ == "__main__":
  try:
    if len(sys.argv) != 2:
        raise RuntimeError("A PID for the process to kill on TX->RX is required")
    App(int(sys.argv[1])).Run()
  except KeyboardInterrupt:
    sys.exit(0)
