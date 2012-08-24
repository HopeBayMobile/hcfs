import  os, time

def generateFile():
	zero = "".zfill(1024)
	count=0
	start = time.time()
	for i in range(10):
		l1_dirname = "L1_dir"+str(i)
		os.system("mkdir "+l1_dirname)
		for j in range(10):
			l2_dirname=l1_dirname+"/L2_dir"+str(j)
			os.system("mkdir "+l2_dirname)
			for k in range(10):
				l3_dirname=l2_dirname+"/L3_dir"+str(k)
				os.system("mkdir "+l3_dirname)
				for l in range(10):
					l4_dirname=l3_dirname+"/L4_dir"+str(l)
					os.system("mkdir "+l4_dirname)
					ts = time.time()
					for m in range(1000):
						filename = l4_dirname+"/test"+str(m)
						fh = open(filename, "w")
						fh.write(zero)
						fh.close()
					elapsed = time.time() - ts
                                	count=count+1
					print "Create the %dth dir in %d seconds"%(count,elapsed)
	end = time.time()
	elapsed = end - start
	print "Completed in %d seconds"%elapsed
		

if __name__ == "__main__":
	generateFile()
