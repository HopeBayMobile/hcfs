cat > /etc/squid3/squid.conf << EOF
acl manager proto cache_object
acl localhost src 127.0.0.1/32 ::1
acl to_localhost dst 127.0.0.0/8 0.0.0.0/32 ::1
#acl all src all

acl SSL_ports port 443
acl Safe_ports port 80		# http
acl Safe_ports port 21		# ftp
acl Safe_ports port 443		# https
acl Safe_ports port 70		# gopher
acl Safe_ports port 210		# wais
acl Safe_ports port 1025-65535	# unregistered ports
acl Safe_ports port 280		# http-mgmt
acl Safe_ports port 488		# gss-http
acl Safe_ports port 591		# filemaker
acl Safe_ports port 777		# multiling http
acl CONNECT method CONNECT

http_access allow manager localhost
http_access allow all
http_access allow localhost

http_port 3128

hierarchy_stoplist cgi-bin ?

cache_mem 2048 MB

# cache_dir ufs <cache-dir> <cache size in MB> <# of L1 directories> <# of L2 directories>
cache_dir ufs $1 51200 64 256

minimum_object_size 0 KB

maximum_object_size 20 GB

coredump_dir /var/spool/squid3

# Add any of your own refresh_pattern entries above these.
refresh_pattern ^ftp:		1440	20%	10080
refresh_pattern ^gopher:	1440	0%	1440
refresh_pattern -i (/cgi-bin/|\?) 0	0%	0
refresh_pattern .		0	20%	4320

EOF
