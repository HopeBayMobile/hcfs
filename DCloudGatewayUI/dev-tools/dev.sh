#!/bin/bash
gnome-terminal -e "../manage.py celeryd --loglevel=info" &
gnome-terminal -e "../manage.py runserver 0.0.0.0:80" &