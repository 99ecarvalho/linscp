# Golden-output tests

The scripts in `cases/` run over SFTP, WebDAV(S), FTP, FTPES, FTPS and S3
against throwaway servers on 127.0.0.1, and the XML session logs (`/xmllog`)
are compared. `expected/` holds the logs real WinSCP wrote on Windows; ctest
`golden` runs linscp against them. See `run_golden.py` for what the servers
and the normalisation do.

## Capturing the corpus with real WinSCP (Windows)

Needs: a checkout of this repository, Python 3.9+, rclone on `PATH`
(<https://rclone.org/downloads/>) -- the version the corpus records in
`expected/SERVERS.txt`, when recapturing -- and WinSCP (installed or
portable). The committed corpus came from WinSCP 6.5.7 (`CLIENT.txt`).
No administrator rights, and nothing listens beyond 127.0.0.1.

    cd <checkout>
    python port\tests\golden\run_golden.py capture --client "C:\Program Files (x86)\WinSCP\WinSCP.com" --out port\tests\golden\expected

It prints one line per protocol and case. Commit `expected/` (the `.xml`
files; the `.out` files beside them are the console output, for reading, and are not committed). Capture
again whenever a case is added or changed.

## Checking linscp against it

ctest `golden` does, with rclone of the corpus' version, which it expects
at `LINSCP_RCLONE` (default `<build>/tools/rclone`):

    port/tools/fetch_rclone.py --corpus port/tests/golden/expected --dest <build>/tools

Without it the test skips and says why: a different rclone answers some
requests differently, which would read as a client difference.

## Comparing two captures

    run_golden.py compare <expected-dir> <actual-dir>

A difference listed in `known-differences.txt` is printed but does not fail.
