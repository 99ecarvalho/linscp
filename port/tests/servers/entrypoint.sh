#!/bin/sh
# Starts the servers fixtures.py asked for, on 127.0.0.1 (the host's network),
# serving $ROOT (mounted at the same path as on the host), as user linscp
# (password "secret", the host user's uid and gid, so that the files the
# servers write are the test's). The TLS certificate and key and the SSH
# host key are the test's, mounted in /keys. Ports by variable, 0: not run.
set -e
: "${ROOT:?}" "${UID_GID:?}"
# The servers run in the background; each one's process, to check below.
pids=""
uid=${UID_GID%:*}
gid=${UID_GID#*:}
groupadd -o -g "$gid" linscp
useradd -o -u "$uid" -g "$gid" -d "$ROOT" -s /bin/sh -M linscp
echo 'linscp:secret' | chpasswd

if [ "${SSH_PORT:-0}" != 0 ]; then
  mkdir -p /run/sshd
  cat > /etc/ssh/linscp_sshd_config <<CONF
Port $SSH_PORT
ListenAddress 127.0.0.1
HostKey /keys/ssh_host_ed25519_key
PasswordAuthentication yes
KbdInteractiveAuthentication no
PubkeyAuthentication yes
AuthorizedKeysFile /keys/authorized_keys
StrictModes no
UsePAM yes
Subsystem sftp internal-sftp
LogLevel ERROR
CONF
  /usr/sbin/sshd -f /etc/ssh/linscp_sshd_config
fi

# vsftpd: the root as "/" (chrooted, writable), passive ports on loopback;
# TLS explicit where the port says FTP, implicit where it says FTPS.
vsftpd_conf() {
  cat <<CONF
listen=YES
listen_address=127.0.0.1
listen_port=$1
anonymous_enable=NO
local_enable=YES
write_enable=YES
local_umask=022
chroot_local_user=YES
allow_writeable_chroot=YES
local_root=$ROOT
pasv_enable=YES
pasv_address=127.0.0.1
pasv_min_port=$2
pasv_max_port=$3
seccomp_sandbox=NO
secure_chroot_dir=/var/run/vsftpd/empty
pam_service_name=vsftpd
ssl_enable=YES
rsa_cert_file=/keys/server.crt
rsa_private_key_file=/keys/server.key
allow_anon_ssl=NO
force_local_logins_ssl=NO
force_local_data_ssl=NO
implicit_ssl=$4
CONF
}
mkdir -p /var/run/vsftpd/empty
if [ "${FTP_PORT:-0}" != 0 ]; then
  vsftpd_conf "$FTP_PORT" "$FTP_PASV_MIN" "$FTP_PASV_MAX" NO > /etc/vsftpd-explicit.conf
  /usr/sbin/vsftpd /etc/vsftpd-explicit.conf &
  pids="$pids $!"
fi
if [ "${FTPS_PORT:-0}" != 0 ]; then
  vsftpd_conf "$FTPS_PORT" "$FTPS_PASV_MIN" "$FTPS_PASV_MAX" YES > /etc/vsftpd-implicit.conf
  /usr/sbin/vsftpd /etc/vsftpd-implicit.conf &
  pids="$pids $!"
fi

# Apache: WebDAV on the root, basic authentication, a lock database (LOCK).
if [ "${DAV_PORT:-0}" != 0 ] || [ "${DAVS_PORT:-0}" != 0 ]; then
  mkdir -p /run/linscp-apache /var/lock/linscp-dav
  chown linscp:linscp /run/linscp-apache /var/lock/linscp-dav
  htpasswd -bc /run/linscp-apache/htpasswd linscp secret 2>/dev/null
  modules=/usr/lib/apache2/modules
  {
    for m in mpm_event authn_core authn_file auth_basic authz_core authz_user mime dir alias dav dav_fs socache_shmcb ssl; do
      echo "LoadModule ${m}_module $modules/mod_${m}.so"
    done
    cat <<CONF
ServerName 127.0.0.1
User linscp
Group linscp
PidFile /run/linscp-apache/httpd.pid
ErrorLog /dev/stderr
LogLevel warn
TypesConfig /etc/mime.types
DAVLockDB /var/lock/linscp-dav/DAVLock
SSLSessionCache shmcb:/run/linscp-apache/ssl_scache(512000)
DocumentRoot "$ROOT"
<Directory "$ROOT">
  Dav On
  Options Indexes
  AuthType Basic
  AuthName "linscp"
  AuthUserFile /run/linscp-apache/htpasswd
  Require valid-user
</Directory>
CONF
    if [ "${DAV_PORT:-0}" != 0 ]; then
      echo "Listen 127.0.0.1:$DAV_PORT"
    fi
    if [ "${DAVS_PORT:-0}" != 0 ]; then
      cat <<CONF
Listen 127.0.0.1:$DAVS_PORT
<VirtualHost 127.0.0.1:$DAVS_PORT>
  SSLEngine on
  SSLCertificateFile /keys/server.crt
  SSLCertificateKeyFile /keys/server.key
</VirtualHost>
CONF
    fi
  } > /run/linscp-apache/httpd.conf
  /usr/sbin/apache2 -f /run/linscp-apache/httpd.conf -k start
fi

# versitygw: S3 over $S3_ROOT (default $ROOT), its directories the buckets.
if [ "${S3_PORT:-0}" != 0 ]; then
  setpriv --reuid="$uid" --regid="$gid" --clear-groups \
    /usr/local/bin/versitygw --access "${S3_ACCESS:-linscp}" --secret secret --port "127.0.0.1:$S3_PORT" \
      posix "${S3_ROOT:-$ROOT}" &
  pids="$pids $!"
fi

# A port another process took meanwhile: a server that could not listen has
# exited by now (sshd and Apache fail at once, set -e). Then the fixture
# tries other ports.
sleep 1
for pid in $pids; do
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "linscp-servers: a server did not start" >&2
    exit 1
  fi
done
echo "linscp-servers: started"
# Until stopped.
exec sleep infinity
