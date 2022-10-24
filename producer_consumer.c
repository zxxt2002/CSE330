#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/semaphore.h>

// SET MODULE PARAMS
int buffSize;
int prod;
int cons;
int uuid;
module_param(buffSize, int, S_IRUSR | S_IWUSR);
module_param(prod, int, S_IRUSR | S_IWUSR);
module_param(cons, int, S_IRUSR | S_IWUSR);
module_param(uuid, int, S_IRUSR | S_IWUSR);

//delcare struct
struct task_struct_list {
	struct task_struct* task;
	struct task_struct_list* next;
};

struct buffer {
	int capacity;
	struct task_struct_list* list;
};

// CONSTRUCT SEMAPHORES and varibles
struct buffer *buf = NULL;
u64 total_seconds;
int consumed = 0;
struct semaphore empty;	// Size: buffSize. All locks available when buffer is empty
struct semaphore full;	// Size: buffSize. All locks available when buffer is full
struct semaphore mutex;	// Size: 1. Lock is available when none are accessing buffer
sema_init(&empty, buffSize);
sema_init(&full, buffSize);
sema_init(&mutex, 1);


// Use up all locks on full
while (down_trylock(&full));

// PRODUCER FUNCTION

/*

producer takes a list of tasks and a buffer for those tasks,
and goes through each task in the list in order to add them to 
the buffer. However, it can only access the buffer so long as it
has the mutex lock. Additionally, once it has the mutex lock, it checks
to see if the buffer is available for insertion.

If the buffer is not available for task insertion, the producer
releases its mutex lock and repeats its attempt for the given task.

If the buffer is available for task insertion, the producer acquires
a lock for empty 
				(which has count 0 once there are no empty buffer cells, ie [task, task]),
inserts the task into the buffer,
and releases a lock for full 
				(which has count 0 once there are no used buffer cells, ie [null, null])

Return values:
	Successfully went through all tasks: 0
	Failure to go through all tasks: 1
*/
int producer(task_struct* tasks, task_struct* buffer) {
	for_each_process(tasks) {
		// Continue until thread stop point or item placed in buffer
		while (!kthread_should_stop()) {

			// Acquire buffer mutex lock (wait until acquired)
			if (down_interruptible(&mutex)) {

				// Acquire lock from empty if available
				if (down_trylock(&empty)) {
					buffer[buffSize - empty.count] = *tasks;

					up(&full);	// Release 1 lock for an item in the buffer
					up(&mutex);	// Release buffer mutex lock
					break;
				}

				up(&mutex);	// Release buffer mutex lock
			}
		}
		
		if (kthread_should_stop()) return 1;
	}

	return 0;
}

// CONSUMER AND PRODUCER FUNCTION
static int consumer(void *arg) {
	int consumedCount = 0;

	//Start infinite loop until stop
	while (!kthread_should_stop()) {
		if (down_interruptible(&full))
			break;
		if (down_interruptible(&mutex))
			break;


		//Check to skip critical section
		if (kthread_should_stop()) {
			return 0;
		}


		if (buf != NULL && buf->list != NULL && buf->capacity > 0) {
			//Critical section
			struct task_struct_list *temp = buf->list;
			buf->list = buf->list->next;
			u64 time_elapsed = ktime_get_ns() - temp->task->start_time;
			u64 elapsed_seconds = time_elapsed / 1000000000;
			int minutes = elapsed_seconds / 60;
			int hours = minutes / 60;
			int remaining_seconds = elapsed_seconds % 60;
			total_seconds = total_seconds + time_elapsed;
			//minutes = elapsed_seconds % 60;

			consumedCount++;
			consumed++;
			printk(KERN_INFO "[%s] Consumed Item#-%d on buffer index:0 PID:%d Elapsed Time- %d:%d:%d\n", current->comm, consumed, temp->task->pid, hours, minutes, remaining_seconds);
			buf->capacity--;
			up(&mutex);
			up(&empty);
		}
	}

	return 0;
}

/*
* CODE EXAMPLES BELOW
*/

// Set module parameters
// int buffSize;
// int prod;
// int cons;
// int uuid;
// module_param(buffSize, int, S_IRUSR | S_IWUSR);
// module_param(prod, int, S_IRUSR | S_IWUSR);
// module_param(cons, int, S_IRUSR | S_IWUSR);
// module_param(uuid, int, S_IRUSR | S_IWUSR);

// //int param_var;
// //module_param(param_var,int, S_IRUSR | S_IWUSR);

// // Displays module parameter values
// void display(void){
// 	printk(KERN_ALERT "buffsize: param = %d", buffSize);
// 	printk(KERN_ALERT "prod: param = %d", prod);
// 	printk(KERN_ALERT "cons: param = %d", cons);
// 	printk(KERN_ALERT "uuid: param = %d", uuid);
// }
	
// Initializer: Says hello and displays 'module_param's
static int hello_init(void){
	// printk(KERN_ALERT "TEST: hello");	// Test initializer
	display();
	return 0;
}

// Destructor: Runs a list of tasks and accumulates the time for the list to run
static void hello_exit(void){
    unsigned long timeRun = 0;
    unsigned long timeRunS, timeRunM, timeRunH;
	
    up(&full);                                                  //All semaphores set to up
    up(&empty);
    up(&mutex);

    sem_destroy(&full);						//Destroy all sempahores
    sem_destroy(&empty);
    sem_destroy(&mutex);
	
    for_each_process(ts1){					//For all processes in the task list
        timeRun += (ktime_get_ns() - ts1->start_time);		//Accumulate run time
        kthread_stop(ts1);					//Stop that thread
    }

	
    timeRunS = timeRun % 100000000;				//ns to s
    timeRunM = timeRunS % 60;					//s to min
    timeRunH = timeRunM % 60;					//min to hr
    
    timeRunS = timeRunS - (timeRunM*60);			//Find seconds between minutes running
    timeRunM = timeRunM - (timeRunH*60);			//Find minutes between hours running

    printk(KERN_INFO "The total elapsed time of all processes for UID %s is\t%d:%d:%d\n", uuid, timeRunH, timeRunM, timeRunS);
}
	
/* 
threadfn is the function to run in the thread; 
data is the data pointer for threadfn; 
namefmt is the name for the thread. 

On Success: Returns pointer to thread's task_struct
On Failure: Returns ERR_PTR(-ENOMEM)
*/
// struct task_struct* kthread_run(int (*threadfn)(void* data), void* data, const char* namefmt);

// Example code to create kernel thread:

static int kthread_func(void *arg){
	// Some code
}

// Create and run "thread-1"
ts1 = kthread_run(kthread_func, NULL, "thread-1");


module_init(hello_init);	// Initialize function: hello_init
module_exit(hello_exit);	// Exit function: hello_exit
MODULE_LICENSE("GPL");
	
	
struct task_struct* p;			// List of tasks
size_t process_counter = 0;		// Holds number of tasks

// Count number of processes in task list
for_each_process(p){
	++process_counter;
}	

// Semaphores
// struct semaphore empty;	// Size: bufSize. All locks available when buffer is empty
// struct semaphore full;	// Size: bufSize. All locks available when buffer is full
// struct semaphore mutex;	// Size: 1. Lock is available when none are accessing buffer
// sema_init(&mutex, 1);

/*
* 
* List of semaphore functions

static inline vid sema_init(struct semaphore *sem, int val);	// Initializes semaphore using a value val
void down_interruptible(struct semaphore *sem);					// Locks a semaphore
void up(struct semaphore *sem);									// Releases a semaphore lock
*/

sema_init(&empty, 5);	// Initialize 'empty' to 5
while(!kthread_should_stop())
{
	/*
		Acquires a lock from empty, and waits to acquire if lock is unavailable
	*/
	if(down_interruptible(&empty))
	{
		break;
	}

	// Some code...

	up(empty);	// Release lock from empty
}
	
	
