Usage Guide for benchmark_v1.py

Enviroment Setup:
1. Install MySQL
	sudo apt-get install mysql-server

2. Install Python
	sudo apt-get install python python-mysqldb

3. Mount gateway's nfs export to a local path:
	mount -t nfs <nfs_server_ip>:<path> <local_path_for_mount>
	
4. Get into mysql:
	mysql -u root -p
	
5. Configure database, user and its previlige:
	 CREATE DATABASE Gateway_Benchmark;
	 CREATE USER 'db_client' IDENTIFIED BY 'deltacloud';
	 GRANT ALL PRIVILEGES ON *.* TO db_client@localhost IDENTIFIED BY 'deltacloud' WITH GRANT OPTION;

6. Modify create_DB_tables.py for correct path
	Find line 13, such as
	qstr2 = "/mnt/local_test/"
	change to 
	qstr2 = "your local mount path"
	
7.
