#!/bin/sh
### BEGIN INIT INFO
# Provides:          xrdp
# Required-Start:    $remote_fs $syslog $local_fs
# Required-Stop:     $remote_fs $syslog $local_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start xrdp daemon
# Description:       Provide rdp support.
### END INIT INFO

# xrdp control script
# Written : 1-13-2006 - Mark Balliet - posicat@pobox.com
# maintaned by Jay Sorg
# chkconfig: 2345 11 89
# description: starts xrdp

#TODO: improve logs - start/stop daemon in right order

SBINDIR=/usr/sbin
CFGDIR=/etc/xrdp
LOGDIR=/var/log/xrdp
SPOOLDIR=/var/spool/xrdp
LOGDIR=/dev/null

BINARIES="xrdp-logd xrdp-printerd xrdp xrdp-sesman"

. /lib/lsb/init-functions

for exe in $BINARIES; do
    if ! [ -x "$SBINDIR/$exe" ]; then
      log_warning_msg "$exe is not executable"
      exit 0
    fi
done
if ! [ -x $CFGDIR/startwm.sh ]; then
  log_warning_msg "startwm.sh is not executable"
  exit 0
fi

is_daemon_running()
{
  DAEMON=$1
  PID=/var/run/$DAEMON.pid
  if [ -s $PID ] && kill -0 $(cat $PID) >/dev/null 2>&1; then
    return 0;
  else
    return 1;
  fi
}

xrdp_start()
{
  logoff all
  [ -d $SPOOLDIR ] && mkdir -p $SPOOLDIR
  for exe in $BINARIES; do
      log_daemon_msg "Starting $exe"
      if start-stop-daemon --start --quiet --oknodo --pidfile /var/run/$exe.pid\
                           --exec $SBINDIR/$exe -- >$LOGDIR
      then
        log_end_msg 0
      else
        log_end_msg 1
      fi
  done
  return 0;
}

xrdp_stop()
{
  logoff all
  for exe in $BINARIES; do
      log_begin_msg "Stopping $exe"
      if ! is_daemon_running $exe; then
          log_progress_msg " (not loaded)"
          log_end_msg 1
          continue
      fi
      if start-stop-daemon --stop --quiet --oknodo \
                           --pidfile /var/run/$exe.pid --retry 30
      then
        log_end_msg 0
      else
        log_end_msg 1
      fi
  done;
  return 0;
}


case "$1" in

  start)
    FAILED=0
    for exe in $BINARIES; do
        if is_daemon_running $exe; then
          FAILED=1
          log_begin_msg "$exe is already loaded"
          log_end_msg 1
        fi
    done
    [ $FAILED -eq 1 ] && exit 1
    xrdp_start
    ;;

  stop)
    xrdp_stop
    ;;

  force-reload|restart)
    echo "Restarting xrdp..."
    xrdp_stop
    while ! is_daemon_running xrdp; do
      sleep 1
    done
    xrdp_start
    ;;

  status)
    for exe in $BINARIES; do
        status_of_proc -p "/var/run/$exe.pid" "$exe" $exe || exit $?
    done
    ;;

  *)
    log_success_msg "Usage: /etc/init.d/xrdp {start|stop|restart|force-reload|status}"
    exit 1
  ;;

esac

exit 0
