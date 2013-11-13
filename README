= JumpBox =

Welcome to JumpBox

== Installation Notes ==

=== libfutil / rendezvous ==

This code requires libfutil and optionally (but default on) also rendezvous.

These code libraries have to be placed in ../libfutil and ../rendezvous respectively.
(thus a 'for i in jumpbox rendezvous libfutil; do git clone https://git.safdef.org/git/$i; done' does the trick)

=== Mac OSX ===

Use "xcode-select --install" to ensure the XCode Command Line Tools (CLT) are installed.

For Rendezvous one needs the "pbc" library from Stanford's Crypto department:
 http://crypto.stanford.edu/pbc/
and also the libjansson library

Fortunately, these are available in Homebrew:
 brew install pbc
 brew install jansson

== Environment Variables ==

The following DJB_FORCED_* environment variables override either the preferences
as they are configured or the result retrieved from ACS and thus allows easier
testing by bypassing various components.

* DJB_FORCED_HOSTNAME
	defines the hostname that we force connections to (DJB-URI)

* DJB_FORCED_JUMPBOXADDRESS
	defines the 'JumpBox Address' to be used for StegoTorus
	this can be used to bypass the DJB Proxy while being able to start from DJB.

* DJB_FORCED_STEGMETHOD
	defines the Steg Method (json|http) to be used for StegoTorus

* DJB_FORCED_SHAREDSECRET
	defines the Shared Secret to be used for StegoTorus

Generic (libfutil):

* SAFDEF_LOG_LEVEL
	defines the logging level (debug is very verbose).

== Running with StegoTorus ==

Testing JumpBox with StegoTorus can be achieved by the following steps.

In Chrome/Chromium open the extensions page:
 chrome://extensions
Then select 'unpack and load' and select the JumpBox plugin directory (plugin/chrome/)
or if already correctly installed, hit "Reload" to update to the current version.

Start DJB:
{{{
export DJB_FORCED_HOSTNAME=127.0.0.1:8080
export SAFDEF_LOG_LEVEL=debug
server/djb run
}}}

In Chrome/Chromium configure the Plugin by clicking on the JumpBox button and selecting
"Preferences", fill in the rendezvous server and select the amount of circuits.

Then start a Stegotorus client in persist JSON mode (can also be performed from the Plugin).
{{{
./stegotorus --log-min-severity=warn chop socks --trace-packets --persist-mode 127.0.0.1:1080 127.0.0.1:6543 json
}}}

That lets StegoTorus be a SOCKS server on 127.0.0.1:1080 while connecting to JumpBox at 6543.

Then start the Stegotorus server in non-persist mode (this typically runs behind DGW):
{{{
./stegotorus --log-min-severity=warn chop server --trace-packets 64.62.249.222:443 127.0.0.1:8080 json
}}}

That lets ST accept HTP at 127.0.0.1:8080 (matching DJB_FORCED_HOSTNAME above) and makes
it use 64.62.249.222:443 as a Tor Bridge

Configure the Tor client with:
{{{
SocksPort 9060 # local client applications (SOCKS port for WebBrowsers etc)
SocksListenAddress 127.0.0.1 # accept connections only from localhost

SafeLogging 0
Log info file ./info.log
Log debug file ./debug.log

# Where the SOCKS proxy is (StegoTorus client)
Socks4Proxy 127.0.0.1:1080

# A fake Bridge line, gets overruled by StegoTorus)
Bridge 127.0.0.1:9060
UseBridges 1
}}}

and start a Tor client:
{{{
bin/tor -f etc/tor/torrc
}}}

That should make things move.

== Running with StegoTorus (debug) ==

Connections will get lost and lldb does not like those SIGPIPEs, to make it continue use:
{{{
process handle SIGUSR2 -n true -p true -s false
}}}

