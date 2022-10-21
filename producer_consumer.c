#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/semaphore.h>

int buffSize;
int prod;
int cons;
int uuid;
module_param(buffSize, int, S_IRUSR | S_IWUSR);
module_param(prod, int, S_IRUSR | S_IWUSR);
module_param(cons, int, S_IRUSR | S_IWUSR);
module_param(uuid, int, S_IRUSR | S_IWUSR);
//int param_var;
//module_param(param_var,int, S_IRUSR | S_IWUSR);

void display(void){
	printk(KERN_ALERT "buffsize: param = %d", buffSize);
	printk(KERN_ALERT "prod: param = %d", prod);
	printk(KERN_ALERT "cons: param = %d", cons);
	printk(KERN_ALERT "uuid: param = %d", uuid);
	}
	
static int hello_init(void){
	printk(KERN_ALERT "TEST: hello");
	display();
	return 0;
	}
	
static void hello_exit(void){
    unsigned long long timeRun = 0;
    unsigned long timeRunS, timeRunM, timeRunH;

    for_each_process(ts1){					//For all processes in the task list
/*Still needs semphore creation to functionally destroy the semaphores*/
        up(ts1);						//Set flag to up
        timeRun += (ktime_get_ns() - ts1->start_time);		//Accumulate run time

        kthread_stop(ts1);					//Stop that thread
    }
    timeRunS = (unsigned long) (timeRun % 100000000);		//ns to s
    timeRunM = timeRunS % 60;					//s to min
    timeRunH = timeRunM % 60;					//min to hour
    
    timeRunS = timeRunS - (timeRunM*60);			//Find seconds between minutes running
    timeRunM = timeRunM - (timeRunH*60);			//Find minutes between hours running

    printk(KERN_INFO "The total elapsed time of all processes for UID %s is %d : %d : %d\n", uuid, timeRunH, timeRunM, timeRunS);
}
	

struct task_struct *kthread_run(int (*threadfn)(void *data), void *data, const char *namefmt)

static int kthread_func(void *arg){

}

ts1 = kthread_run(kthread_func, NULL, "thread-1");


module_init(hello_init);
module_exit(hello_exit);	
MODULE_LICENSE("GPL");
	
	
struct task_struct* p;
size_t process_counter = 0;
for_each_process(p){
	++process_counter;
	}	
	
struct semaphore empty;
static inline vid sema_init(struct semaphore *sem, int val);
void down_interruptible(struct semaphore *sem);
void up(struct semaphore *sem);

sema_init(&empty, 5);
while(!kthread_should_stop())
{
	if(down_interruptible(&empty))
	{
		break;
	}
	up(empty);
}
	
	
