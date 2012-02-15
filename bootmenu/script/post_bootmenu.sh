#!/system/bootmenu/binary/busybox ash

######## BootMenu Script
######## Execute Post BootMenu

source /system/bootmenu/script/_config.sh

export PATH=/system/xbin:/system/bin:/sbin

######## Main Script

# there is a problem, this script is executed if we 
# exit from recovery...

echo 0 > /sys/class/leds/blue/brightness

## Run Init Script

######## Don't Delete.... ########################
mount -o remount,rw rootfs /
mount -o remount,rw $PART_SYSTEM /system
##################################################

chmod 777 /dev/graphics
chmod 666 /dev/graphics/fb0
chmod 666 /dev/video*

chmod +w /sys/class/leds/spotlight/*
chmod +w /sys/class/leds/torch-flash/*

if [ -d /system/bootmenu/init.d ]; then
    chmod 755 /system/bootmenu/init.d/*
    run-parts /system/bootmenu/init.d
fi

# not made automatically in ics ? to check in vendor/cm
chmod 755 /system/etc/init.d/*
run-parts /system/etc/init.d

# Clean market cache
rm -f /data/data/com.android.providers.downloads/cache/*

# normal cleanup here (need fix in recovery first)
# ...


# fast button warning (to check when script is really used)
if [ -f /sbin/adbd.root ]; then

echo 1 > /sys/class/leds/button-backlight/brightness
usleep 50000
echo 0 > /sys/class/leds/button-backlight/brightness
usleep 50000
echo 1 > /sys/class/leds/button-backlight/brightness
usleep 50000
echo 0 > /sys/class/leds/button-backlight/brightness
usleep 50000
echo 1 > /sys/class/leds/button-backlight/brightness
usleep 50000
echo 0 > /sys/class/leds/button-backlight/brightness

exit 1

fi

######## Don't Delete.... ########################
mount -o remount,ro rootfs /
mount -o remount,ro $PART_SYSTEM /system
##################################################

#/system/bootmenu/script/media_fixup.sh &

exit 0
