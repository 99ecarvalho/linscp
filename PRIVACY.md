# Privacy Policy

Last updated: October 21, 2026

This policy describes the linscp desktop application and command-line client.
It does not cover the linscp website, which has its own [privacy policy](https://linscp.org/manual/about/privacy/).

## Summary

linscp does not collect usage analytics or telemetry. It does not automatically
send crash reports, diagnostic logs, or bug reports. It does not require an
account.

linscp does store configuration and other information locally on your device,
and it makes network connections when you use its file-transfer features,
request an update check, or download an extension. Details are below.

## Network Connections

### File-transfer servers

When you connect to a server or run a queued or scheduled transfer, linscp
communicates with the server you configured. That server can receive your
network address, account name, and protocol requests, and can learn which files
you list, transfer, edit, or otherwise access. The information visible to a
server depends on the protocol and the actions you take. The server operator's
privacy practices apply to that activity.

### Update checks

You can check for updates manually. You can also enable periodic checks in
**Options > Preferences > Updates**. Periodic checks are off by default in a
fresh linscp configuration. On Windows, linscp may use an update interval from
shared application settings if one is already configured. Some distribution
builds disable linscp's update check.

An update check requests release information from the GitHub API. The request
identifies the linscp version in its HTTP user-agent and necessarily exposes
your network address to GitHub and any network intermediaries. It does not
include your saved sites, credentials, file names, or transfer history. GitHub
handles the request under its own [privacy statement](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement).

### Extension downloads and web links

If you choose to add an extension from a URL, linscp downloads it from that
address. The server and intermediaries can receive your network address and
standard request information. Review an extension before using it.

When you open a website link from linscp, the page is opened in your browser.
That website may collect information according to its own policy. Opening a
link does not send your linscp settings or transfer history to that site.

## Bug Reports and Support

linscp does not automatically create or send bug reports. If you choose to
report a bug through [GitHub Issues](https://github.com/99ecarvalho/linscp/issues)
or ask a question in [GitHub Discussions](https://github.com/99ecarvalho/linscp/discussions),
you decide what information to include. Issues are generally public. GitHub
processes information associated with your account and visit under its own
privacy policy.

Logs are not uploaded by linscp. If you attach or paste a log into a report,
review it first: it may contain server names, user names, paths, and file names.
For security reports, use the private channels described in [SECURITY.md](SECURITY.md).

## Information Stored Locally

Depending on the features you use, linscp stores the following on your device:

- Preferences, saved sites, and session information.
- Passwords only when you choose to save them. Without a master password,
  stored passwords are obfuscated, not strongly encrypted. A master password
  protects saved passwords with encryption.
- Server host keys that you have accepted, so linscp can detect a changed key.
- Recent paths, commands, and search terms used to populate history lists.
- Temporary copies of files opened for editing.
- Logs, only if you enable logging in **Options > Preferences > Logging**.
  Logs can contain identifying connection and file information.

The exact storage location depends on your operating system, installation, and
portable-mode settings. The **Clean Up** command in the Login window's **Tools**
menu lets you remove supported categories of stored data. You can also remove
configuration files or registry data yourself; doing so may delete saved sites
and preferences.

## Changes and Contact

This policy may be updated as linscp changes. The date at the top indicates
when it was last revised. For privacy questions, please contact
[ecorreia@apliant.com.br](mailto:ecorreia@apliant.com.br).
