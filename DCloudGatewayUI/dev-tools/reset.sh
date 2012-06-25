#!/bin/bash
../manage.py flush --noinput
/home/nii/reset_gateway
/etc/init.d/celeryd restart
