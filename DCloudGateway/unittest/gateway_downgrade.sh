sudo apt-get -y remove s3ql
sudo dpkg -i s3ql_1.12.0~natty1_amd64.deb 
echo "Now the version of s3ql is down-graded to "
apt-show-versions s3ql
