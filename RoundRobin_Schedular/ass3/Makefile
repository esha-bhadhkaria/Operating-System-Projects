all:
	gcc -o SimpleShell SimpleShell.c -lrt -pthread
	gcc -o SimpleScheduler SimpleScheduler.c -lrt -pthread

clean:
	rm -f SimpleShell SimpleScheduler

jobs:
	gcc job1.c -o job1
	gcc job2.c -o job2
	gcc job3.c -o job3
	gcc job4.c -o job4
	gcc job5.c -o job5
