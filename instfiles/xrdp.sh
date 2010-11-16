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

SBINDIR=/usr/sbin
CFGDIR=/etc/xrdp
LOGDIR=/var/log/xrdp
SPOOLDIR=/var/spool/xrdp
PIDDIR=/var/run
LOGDIR=/dev/null

BINARIES="xrdp-logd xrdp-printerd xrdp xrdp-sesman"

. /lib/lsb/init-functions

is_exec()
{
    if ! [ -x "$1" ]; then
      log_warning_msg "$1 is not executable"
      exit 1
    fi
}

is_daemon_running()
{
  DAEMON=$1
  PID=$PIDDIR/$DAEMON.pid
  if [ -f $PID ]; then
    if kill -0 $(cat $PID) 2>$LOGDIR; then
      return 0;
    else
      rm $PID
      return 1
    fi
  else
    return 1;
  fi
}

for exe in $BINARIES; do
    is_exec $SBINDIR/$exe
done
is_exec $CFGDIR/startwm.sh

xrdp_start()
{
  logoff all
  FAILED=0
  for exe in $BINARIES; do
    if is_daemon_running $exe; then
      FAILED=1
      log_warning_msg "$exe is already loaded"
    fi
  done
  if [ $FAILED -eq 1 ]; then
    log_failure_msg "Do not try to launch another daemon !"
    exit 1
  fi
  for exe in $BINARIES; do
      log_daemon_msg "Starting $exe"
      if start-stop-daemon --start --quiet --oknodo --pidfile $PIDDIR/$exe.pid \
                           --exec $SBINDIR/$exe -- >$LOGDIR
      then
        log_end_msg 0
      else
        log_end_msg 1
      fi
  done
}

xrdp_stop()
{
  logoff all
  for exe in $(echo $BINARIES | tac -s ' '); do
      log_begin_msg "Stopping $exe"
      if ! is_daemon_running $exe; then
          log_progress_msg " (not loaded)"
          log_end_msg 1
          continue
      fi
      if start-stop-daemon --stop --quiet --oknodo \
                           --pidfile $PIDDIR/$exe.pid --retry 30
      then
        log_end_msg 0
      else
        log_end_msg 1
      fi
  done;
}


case "$1" in

  start)
    xrdp_start
  ;;

  stop)
    xrdp_stop
  ;;

  force-reload|restart)
    log_success_msg "Restarting xrdp..."
    xrdp_stop
    xrdp_start
  ;;

  status)
    for exe in $BINARIES; do
        status_of_proc -p "$PIDDIR/$exe.pid" "$exe" $exe || exit $?
    done
  ;;

  *)
    log_success_msg "Usage: /etc/init.d/xrdp {start|stop|restart|force-reload|status}"
    exit 1
  ;;

esac

exit 0
