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
LOG=/dev/null
CFGDIR=/etc/xrdp
LOGDIR=/var/log/xrdp
SPOOLDIR=/var/spool/xrdp


. /lib/lsb/init-functions


if ! test -x $SBINDIR/xrdp
then
  log_warning_msg "xrdp is not executable"
  exit 0
fi
if ! test -x $SBINDIR/xrdp-sesman
then
  log_warning_msg "xrdp-sesman is not executable"
  exit 0
fi
if ! test -x $SBINDIR/xrdp-logd
then
  log_warning_msg "xrdp-logd is not executable"
  exit 0
fi
if ! test -x $SBINDIR/xrdp-printerd
then
  log_warning_msg "xrdp-printerd is not executable"
  exit 0
fi
if ! test -x $CFGDIR/startwm.sh
then
  log_warning_msg "startwm.sh is not executable"
  exit 0
fi

xrdp_start()
{
  logoff all
  if ! test -d $SPOOLDIR; then
    mkdir -p $SPOOLDIR
  fi

  log_daemon_msg "Starting Xrdp rdp server"
  if start-stop-daemon --start --quiet --oknodo --pidfile /var/run/xrdp.pid --exec $SBINDIR/xrdp -- >$LOG; then
    log_end_msg 0
  else
    log_end_msg 1
  fi
  log_daemon_msg "Starting Xrdp session manager"
  if start-stop-daemon --start --quiet --oknodo --pidfile /var/run/xrdp-sesman.pid --exec $SBINDIR/xrdp-sesman -- >$LOG; then
    log_end_msg 0
  else
    log_end_msg 1
  fi
  log_daemon_msg "Starting Xrdp logging service"
  if start-stop-daemon --start --quiet --oknodo --pidfile /var/run/xrdp-logd.pid --exec $SBINDIR/xrdp-logd -- >$LOG; then
    log_end_msg 0
  else
    log_end_msg 1
  fi
  log_daemon_msg "Starting Xrdp printer service"
  if start-stop-daemon --start --quiet --oknodo --pidfile /var/run/xrdp-printerd.pid --exec $SBINDIR/xrdp-printerd -- >$LOG; then
    log_end_msg 0
  else
    log_end_msg 1
  fi
  return 0;
}

xrdp_stop()
{
  logoff all
  log_daemon_msg "Stopping Xrdp rdp server"
  if start-stop-daemon --stop --quiet --oknodo --pidfile /var/run/xrdp.pid --retry 30; then
    log_end_msg 0
  else
    log_end_msg 1
  fi
  log_daemon_msg "Stopping Xrdp session manager"
  if start-stop-daemon --stop --quiet --oknodo --pidfile /var/run/xrdp-sesman.pid --retry 30; then
    log_end_msg 0
  else
    log_end_msg 1
  fi
  log_daemon_msg "Stopping Xrdp logging service"
  if start-stop-daemon --stop --quiet --oknodo --pidfile /var/run/xrdp-logd.pid --retry 30; then
    log_end_msg 0
  else
    log_end_msg 1
  fi
  log_daemon_msg "Stopping Xrdp printing service"
  if start-stop-daemon --stop --quiet --oknodo --pidfile /var/run/xrdp-printerd.pid --retry 30; then
    log_end_msg 0
  else
    log_end_msg 1
  fi
  return 0;
}

is_xrdp_running()
{
  if [ -s /var/run/xrdp.pid ] && kill -0 $(cat /var/run/xrdp.pid) >/dev/null 2>&1
  then
    return 1;
  else
    return 0;
  fi
}

is_sesman_running()
{
  if [ -s /var/run/xrdp-sesman.pid ] && kill -0 $(cat /var/run/xrdp-sesman.pid) >/dev/null 2>&1
  then
    return 1;
  else
    return 0;
  fi
}

is_logd_running()
{
  if [ -s /var/run/xrdp-logd.pid ] && kill -0 $(cat /var/run/xrdp-logd.pid) >/dev/null 2>&1
  then
    return 1;
  else
    return 0;
  fi
}

is_printerd_running()
{
  if [ -s /var/run/xrdp-printerd.pid ] && kill -0 $(cat /var/run/xrdp-printerd.pid) >/dev/null 2>&1
  then
    return 1;
  else
    return 0;
  fi
}


check_up()
{
  # Cleanup : If sesman isn't running, but the pid exists, erase it.
  is_sesman_running
  if test $? -eq 0
  then
    if test -e /var/run/xrdp-sesman.pid
    then
      rm /var/run/xrdp-sesman.pid
    fi
  fi
  # Cleanup : If xrdp isn't running, but the pid exists, erase it.
  is_xrdp_running
  if test $? -eq 0
  then
    if test -e /var/run/xrdp.pid
    then
      rm /var/run/xrdp.pid
    fi
  fi
  # Cleanup : If xrdp-logd isn't running, but the pid exists, erase it.
  is_logd_running
  if test $? -eq 0
  then
    if test -e /var/run/xrdp-logd.pid
    then
      rm /var/run/xrdp-logd.pid
    fi
  fi
  # Cleanup : If printer isn't running, but the pid exists, erase it.
  is_printerd_running
  if test $? -eq 0
  then
    if test -e /var/run/xrdp-printerd.pid
    then
      rm /var/run/xrdp-printerd.pid
    fi
  fi
  return 0;
}

case "$1" in
  start)
    check_up
    FAILED=0
    is_xrdp_running
    if ! test $? -eq 0
    then
      FAILED=1
      log_begin_msg "xrdp is already loaded"
      log_end_msg 1
    fi
    is_sesman_running
    if ! test $? -eq 0
    then
      FAILED=1
      log_begin_msg "sesman is already loaded"
      log_end_msg 1
    fi
    is_logd_running
    if ! test $? -eq 0
    then
      FAILED=1
      log_begin_msg "logd is already loaded"
      log_end_msg 1
      exit 1
    fi
    is_printerd_running
    if ! test $? -eq 0
    then
      FAILED=1
      log_begin_msg "printerd is already loaded"
      log_end_msg 1
      exit 1
    fi
    if test $FAILED -eq 1
    then
      exit 1
    fi
    xrdp_start
    ;;
  stop)
    check_up
    is_xrdp_running
    if test $? -eq 0
    then
      log_begin_msg "xrdp is not loaded"
      log_end_msg 1
    fi
    is_sesman_running
    if test $? -eq 0
    then
      log_begin_msg "sesman is not loaded"
      log_end_msg 1
    fi
    is_logd_running
    if test $? -eq 0
    then
      log_begin_msg "logd is not loaded"
      log_end_msg 1
    fi
    is_printerd_running
    if test $? -eq 0
    then
      log_begin_msg "printerd is not loaded"
      log_end_msg 1
    fi
    xrdp_stop
    ;;
  force-reload|restart)
    check_up
    echo "Restarting xrdp ..."
    xrdp_stop
    is_xrdp_running
    while ! test $? -eq 0
    do
      check_up
      sleep 1
      is_xrdp_running
    done
    xrdp_start
    ;;
  status)
    status_of_proc -p "/var/run/xrdp.pid" "xrdp" xrdp || exit $?
    status_of_proc -p "/var/run/xrdp-sesman.pid" "xrdp-sesman" xrdp-sesman || exit $?
    status_of_proc -p "/var/run/xrdp-logd.pid" "xrdp-logd" xrdp-logd || exit $?
    status_of_proc -p "/var/run/xrdp-printerd.pid" "xrdp-printerd" xrdp-printerd || exit $?
    ;;
  *)
    log_success_msg "Usage: /etc/init.d/xrdp {start|stop|restart|force-reload|status}"
    exit 1
  ;;
esac

exit 0
