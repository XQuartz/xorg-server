# Reporting Security Issues

Please notify us of any security issues by sending mail to
<xorg-security@lists.x.org>.

See https://www.x.org/wiki/Development/Security/Organization/
for more information about the X.Org security team.

# Learning about Security Fixes

X.Org announces security bugs and bug fix releases on the xorg-announce
mailing list.  See the archives at https://lists.x.org/archives/xorg-announce/
and see https://lists.x.org/mailman/listinfo/xorg-announce to subscribe.

Security advisories are also listed on our wiki at
https://www.x.org/wiki/Development/Security/ and mailed to the
https://oss-security.openwall.org/wiki/mailing-lists/oss-security mailing list.

# Security model and trust boundaries

Only the Xorg server is expected to run with elevated privileges.
(Some distros may run Xorg with a wrapper to only grant these privileges when
necessary.)  The Xorg server usually requires root access to hardware devices
and I/O registers when using a UMS (Userspace Mode Setting) driver, and not
when using a KMS (Kernel Mode Setting) driver, or drivers which do not require
actual hardware access (such as xf86-video-dummy).

All other X servers (Xephyr, Xnest, Xvfb, etc.) are expected to run with only
the privileges of the user who started the server.  They should not require
direct access to any devices.

The Xorg server uses configuration files to control various aspects of its
operation (see the xorg.conf(5) man page), including specifying loadable
object modules to run code from with the full privileges of the X server.
There is no attempt to sandbox these modules - they are considered to be fully
trusted, and thus anyone who can edit a config file is considered to be fully
trusted - a module being able to control or crash the X server is not considered
a security vulnerability (though a crash would be a non-security bug in the
module).  The configuration file loading mechanism takes steps to verify that
config files are owned by trusted users before reading them, and failure to do
so would be considered a security vulnerability.

Access control for which clients can connect to the X server is provided by
a number of mechanisms, see the Xsecurity(7) man page for details.  Once a
client is authenticated via those mechanisms and has an active connection,
we do not consider it a security vulnerability for them to be able to take
any actions described in the X11 protocol or extension specifications, such
as changing monitor configurations or killing other clients, though we will
accept non-security bug reports for clients doing so in a manner or via
requests not documented in the protocol specs as doing those operations.

## Client-controlled server crashes

Some bugs enable an authenticated client to trigger a server crash via a
protocol request. If the server crash does not lead to information leakage or
possible privilege escalation the bug is not considered a security issue.  One
example for such issues are client-triggered NULL-pointer dereferences in
the server.

For these issues a public issue and/or merge request should be filed in our
GitLab: https://gitlab.freedesktop.org/xorg/xserver/

## Client-controlled Denial of Service (DoS)

Some bugs enable an authenticated client to trigger a server condition where
the server is too busy processing the client's request and stops responding to
other clients. This is considered a regular bug and is not considered a
security issue, a client can achieve the same DoS using the `GrabServer` core
protocol request.

For these issues a public issue and/or merge request should be filed in our
GitLab: https://gitlab.freedesktop.org/xorg/xserver/
