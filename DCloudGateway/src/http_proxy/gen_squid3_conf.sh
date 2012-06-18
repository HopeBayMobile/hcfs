cat > /etc/squid3/squid.conf << EOF
http_port 3128 transparent
acl localnet src 127.0.0.1/255.255.255.255
http_access allow all
http_access allow localnet
dns_nameservers 8.8.8.8  168.95.1.1

# cache_dir ufs <cache-dir> <cache size in MB> <# of L1 directories> <# of L2 directories>
cache_dir ufs $1 51200 64 128
EOF
