rm sqlite3.db
./manage.py syncdb << EOF
no
EOF
./manage.py runserver 0.0.0.0:8765
