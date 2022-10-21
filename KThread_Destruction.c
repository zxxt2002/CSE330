#include <linux/kernel.h>
#include <linux/module.h>

static void module_exit(void){
    struct task_struct* p;
    unsigned long long timeRun = 0;
    unsigned long timeRunS, timerunM, timeRunH;

    for_each_process(p){
        up(p->sema);
        timeRun += (ktime_get_ns() - p->start_time);

        kthread_stop(p);
    }
    timeRunS = (unsigned long) (timeRun % 100000000);
    timeRunM = timeRunS % 60;
    timeRunH = timeRunM % 60;

    printk(KERN_INFO "The total elapsed time of all processes for UID %s is %d : %d : %d\n", uuid, timeRunH, timeRunM, timeRunS);
}