Usage Guide for benchmark_v1.py

=== Enviroment Setup ===
1. Install MySQL
	sudo apt-get install mysql-server

2. Install Python
	sudo apt-get install python python-mysqldb

3. Get into mysql:
	mysql -u root -p
	
4. Configure database, user and its previlige:
	 CREATE DATABASE Swift_Benchmark;
	 CREATE USER 'db_client' IDENTIFIED BY 'deltacloud';
	 GRANT ALL PRIVILEGES ON *.* TO db_client@localhost IDENTIFIED BY 'deltacloud' WITH GRANT OPTION;

=== Execute benchmark ===
1. python create_DB_tables.py
		note if database Swift_Benchmark in mysql is not empty, you better 
		DROP DATABASE Swift_Benchmark;
		if you are not sure whether old data is useful.
2. python benchmark_v1.py /dev/shm

=== Guide to Extract Result from MySQL ===
	mysql -u root -p
	USE Swift_Benchmark;
	SELECT file_size,AVG(file_size/elapsed_time/1024) FROM Operation_Log WHERE action='read file' AND success=1 GROUP BY file_size;
	SELECT file_size,AVG(file_size/elapsed_time/1024) FROM Operation_Log WHERE action='write file' AND success=1 GROUP BY file_size;

=== Change Log ===
ver 0.3:
	1. 1. Add a column in Operation_Log DB: 
	"error_msg," this message will record any errors in running linux file operations.
