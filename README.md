# Operating-Systems
xv6 OS for learning about and experimenting with operating systems concepts


Arianne Butler
alb153, 11068223
Kristof Mercier
kmm732, 11182729

Files changed for further functionality:
	
	
	syscall.c
	-	Define user space syscall functions:
		o	sys_nice(void)
		o	sys_getpriority(void)
		o	sys_setpriority(void)
	-	put the new syscalls in the syscall array
	
	syscall.h
	-	Set the syscall numbers:
		o	#define SYS_nice   22
		o	#define SYS_getpriority 23
		o	#define SYS_setpriority 24
	
	sysproc.c
	-	Write 3 functions to get arguments for nice, getpriority, and setpriority 
		from userspace and pass them to kernel space
		o	int sys_nice(void)
		o	int sys_getpriority(void)
		o	int sys_setpriority(void)
	
	user.h 
	-	Define kernel space syscall functions
		o	int nice (int incr);
		o	int getpriority(int pid);
		o	int setpriority(int pid, int new_priority);
	
	proc.h
	-	add priority and inQ attributes to process struct (inQ is a boolean value that 
		is 1 if the process is already in a queue, and 0 if it is not)

	proc.c
	-	Define structs for list, list node, free nodes array, and the priority queues
		o	List needs head, tail, and count
		o	List node needs a pointer to a process and a pointer to the next node
		o	The free nodes struct needs two arrays, one containing the actual available 
			nodes, and one contains pointers to those nodes. We will use these arrays to
			fill the array contains pointers to available nodes. Contains a cursor and 
			max is defined by NPROC
			This will require a function to initialize the array of node pointers to 
			an array of actual nodes (nodeInit( ) )
	-	Write an enqueue function that takes the pid of the process to add to a priority
		queue. The function must find the given process in the ptable using the pid, 
		create a new list node, set the list node's process to the process found in the 
		ptable. It must then place the newly created node in one of five queues, 
		depending on the processes priority. The function will need to check whether or
		not the queue is empty before adding it. 
	-	allocproc() will need to set the inQ and priority variables
	-	Each time the state of a process is switched to runnable, the enqueue function 
		must be called
	-	In fork(), the priority of the child processes must be set to the priority of its 
		parent 
	-	The scheduler function will search through the ptable looking for runnable 
		processes. When it finds one, it enqueues the process onto one of the priority 
		queues. It will then require a for loop to search through the priority queues, 
		one at a time, beginning at the queue with the highest priority. 
	-	Kernel space functions getpriority, setpriority, and nice need to be written. 
		o	getpriority will return the priority of the process with the pid that matches
			its parameter
		o	setpriority will change the priority of the process with the pid that matches
			its parameter
		o	nice will force the priority of the process with the pid that matches its 
			parameter to change to a lower priority process 
			
	- Added later:
		dequeue function to handle removing processes from the priority queues

