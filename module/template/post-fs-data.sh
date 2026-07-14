#!/system/bin/sh
# shellcheck disable=SC2034
MODDIR=${0%/*}

MODULE_MIN_MAGISK_VERSION=27005

MODULE_MIN_ZYGISKSU_VERSION=497
MODULE_MIN_REZYGISK_VERSION=361
# NeoZygisk (JingMatrix) reuses the `zygisksu` module id but is a separate
# project with its own version scheme (verCode = git commit count), so the
# ZygiskNext threshold above must not be applied to it. Floor is conservative
# (2.x line; validated on 275).
MODULE_MIN_NEOZYGISK_VERSION=200

HAS_ZYGISKSU=false
HAS_NEOZYGISK=false
HAS_REZYGISK=false

rm -f /data/adb/nohello/no_clr_ptracemsg

if [ -d "/data/adb/modules/zygisksu" ]; then
  if [ ! -f "/data/adb/modules/zygisksu/disable" ]; then
    ZYGISKSU_VERSION=$(grep versionCode /data/adb/modules/zygisksu/module.prop | sed 's/versionCode=//g')
    ZYGISKSU_NAME=$(grep '^name=' /data/adb/modules/zygisksu/module.prop | sed 's/name=//g')
    ZYGISKSU_AUTHOR=$(grep '^author=' /data/adb/modules/zygisksu/module.prop | sed 's/author=//g')
    if [ "$ZYGISKSU_AUTHOR" = "JingMatrix" ] || echo "$ZYGISKSU_NAME" | grep -qi neozygisk; then
      HAS_NEOZYGISK=true
      if [ -z "$ZYGISKSU_VERSION" ]; then
        touch "$MODDIR/disable"
      elif [ "$ZYGISKSU_VERSION" -lt "$MODULE_MIN_NEOZYGISK_VERSION" ]; then
        touch "$MODDIR/disable"
      else
        # NeoZygisk is itself a ptrace-based injector that manages the zygote,
        # so skip NoHello's own ptrace_message-clearing dance to avoid a ptrace
        # conflict (mirrors the ZygiskNext>=521 / ReZygisk>=362 behaviour).
        touch /data/adb/nohello/no_clr_ptracemsg
      fi
    else
      HAS_ZYGISKSU=true
      if [ -z "$ZYGISKSU_VERSION" ]; then
        touch "$MODDIR/disable"
      elif [ "$ZYGISKSU_VERSION" -lt "$MODULE_MIN_ZYGISKSU_VERSION" ]; then
        touch "$MODDIR/disable"
      elif [ "$ZYGISKSU_VERSION" -ge 521 ]; then
        touch /data/adb/nohello/no_clr_ptracemsg
        touch /data/adb/nohello/no_dirtyro_ar
      fi
    fi
  fi
fi

if [ -d "/data/adb/modules/rezygisk" ]; then
  if [ ! -f "/data/adb/modules/rezygisk/disable" ]; then
    HAS_REZYGISK=true
    REZYGISK_VERSION=$(grep versionCode /data/adb/modules/rezygisk/module.prop | sed 's/versionCode=//g')
    if [ -z "$REZYGISK_VERSION" ]; then
      touch "$MODDIR/disable"
    elif [ "$REZYGISK_VERSION" -lt "$MODULE_MIN_REZYGISK_VERSION" ]; then
      touch "$MODDIR/disable"
    elif [ "$REZYGISK_VERSION" -ge 362 ]; then
      touch /data/adb/nohello/no_clr_ptracemsg
    fi
  fi
fi

if { [ "$HAS_ZYGISKSU" = true ] || [ "$HAS_NEOZYGISK" = true ]; } && [ "$HAS_REZYGISK" = true ]; then
  touch "$MODDIR/disable"
fi

if [ "$HAS_ZYGISKSU" = false ] && [ "$HAS_NEOZYGISK" = false ] && [ "$HAS_REZYGISK" = false ]; then
  MAGISK_VERSION="$(magisk -V)"
  if [ -z "$MAGISK_VERSION" ]; then
    touch "$MODDIR/disable"
  elif [ "$MAGISK_VERSION" -lt "$MODULE_MIN_MAGISK_VERSION" ]; then
    touch "$MODDIR/disable"
  fi
fi

if [ ! -f /data/adb/post-fs-data.d/.nohello_cleanup.sh ]; then
  mkdir -p /data/adb/post-fs-data.d
  cat "$MODDIR/cleanup.sh" > /data/adb/post-fs-data.d/.nohello_cleanup.sh
  chmod +x /data/adb/post-fs-data.d/.nohello_cleanup.sh
fi
