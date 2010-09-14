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

if ! test -x $SBINDIR/xrdp
then
  echo "xrdp is not executable"
  exit 0
fi
if ! test -x $SBINDIR/xrdp-sesman
then
  echo "xrdp-sesman is not executable"
  exit 0
fi
if ! test -x $SBINDIR/xrdp-logd
then
  echo "xrdp-sesman is not executable"
  exit 0
fi
if ! test -x $CFGDIR/startwm.sh
then
  echo "startwm.sh is not executable"
  exit 0
fi

xrdp_start()
{
  echo -n "Starting: logd, xrdp and sesman . . "
  logoff all
  $SBINDIR/xrdp-logd 1>>$LOG 2>&1
  $SBINDIR/xrdp 1>>$LOG 2>&1
  $SBINDIR/xrdp-sesman 1>>$LOG 2>&1
  echo "."
  sleep 1
  return 0;
}

xrdp_stop()
{
  echo -n "Stopping: logd, xrdp and sesman . . "
  logoff all
  $SBINDIR/xrdp-sesman --kill 1>>$LOG 
  $SBINDIR/xrdp --kill 1>>$LOG
  $SBINDIR/xrdp-logd --kill 1>>$LOG
  echo "."
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
  return 0;
}

case "$1" in
  start)
    check_up
    is_xrdp_running
    if ! test $? -eq 0
    then
      echo "xrdp is already loaded"
      exit 1
    fi
    is_sesman_running
    if ! test $? -eq 0
    then
      echo "sesman is already loaded"
      exit 1
    fi
    is_logd_running
    if ! test $? -eq 0
    then
      echo "sesman is already loaded"
      exit 1
    fi
    xrdp_start
    ;;
  stop)
    check_up
    is_xrdp_running
    if test $? -eq 0
    then
      echo "xrdp is not loaded."
    fi
    is_sesman_running
    if test $? -eq 0
    then
      echo "sesman is not loaded."
    fi
    is_logd_running
    if test $? -eq 0
    then
      echo "sesman is not loaded."
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
  *)
    echo "Usage: $0 {start|stop|restart|force-reload}"
    exit 1
esac

exit 0
