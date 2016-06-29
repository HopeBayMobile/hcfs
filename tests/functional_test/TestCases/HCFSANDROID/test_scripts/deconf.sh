#!/bin/bash

###notation
##TAG
deconf() {
adb push hcfsconf /data/
adb shell chmod 777 /data/hcfsconf
adb shell /data/hcfsconf dec /data/hcfs.conf /tmp/hcfs.conf.dec
adb pull /tmp/hcfs.conf.dec /tmp/hcfs.conf/dec
}

####main####
deconf

