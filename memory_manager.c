#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/hrtimer.h>

//The process ID (stored in pid variable) is taken as the command-line input argument.
static int pid = 0;
module_param(pid, int, 0);

//Declaring variables

struct task_struct *task;
unsigned long address = 0;
struct vm_area_struct *vma;
pte_t *ptep_curr;

//Declaring variables to store the total number of pages that are present in the Resident Set Size (RSS), SWAP, Working Set Size (WSS) respectively. 

unsigned long RSS = 0;
unsigned long SWAP = 0;
unsigned long WSS = 0;

//This function walks through each valid process' page tables and checks if the page is present in the physical memory or in the swap. 

static void walk_page_tables(struct mm_struct *mm, unsigned long address){

	pgd_t *pgd;
	p4d_t *p4d;
	pte_t *ptep, pte;
	pud_t *pud;
	pmd_t *pmd;
		
	//Get pgd from mm and the page address

	pgd = pgd_offset(mm,address);
	if (pgd_none(*pgd) || pgd_bad(*pgd)){
		return;
	}
	
	//Get p4d from pgd and the page address
	
	p4d = p4d_offset(pgd,address);
	if (p4d_none(*p4d) || p4d_bad(*p4d)) {
		return;
	}
	//Get pud from p4d and the page address
	
	pud = pud_offset(p4d,address);
	if (pud_none(*pud) || pud_bad(*pud)) {
		return;
	}
	//Check if pmd is bad or does not exist
	
	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		return;
	}
	
	// Get pte from pmd and the page address
	
	ptep = pte_offset_map(pmd,address);
	if(!ptep) {
		return;
	}
		
	pte = *ptep;
	ptep_curr = ptep;
	
	//Check if the current page is valid and whether or not it is present in the memory.
	
	int pte_is_present = pte_present(pte);
	
	//If the page is valid but is not present in the memory, then it is in the SWAP. Increment the counter variable.
	
	if (pte_is_present == 0)
	{
		SWAP++;
	}
	
	//If the page is valid and present in the meory, then it is in the RSS. Increment the counter variable.
	
	else
	{
		RSS++;
	}
}

/* This is a timer function which helps by having a periodic timer (every 10 seconds) to help measure a given process' Resident Set Size (RSS), SWAP size, and
Working Set Size (WSS). For several minutes , these different memory usage changes are observed and the staistics related to that are printed.*/

unsigned long timer_interval_ns = 10e9; // The 10 second interval for timers.
static struct hrtimer timer; //Initializng the timer.

//The timer function.

enum hrtimer_restart no_restart_callback(struct hrtimer *timer){
	
	ktime_t currtime, interval;
	currtime = ktime_get();
	interval = ktime_set(0, timer_interval_ns);
	hrtimer_forward(timer, currtime, interval);
	
	//Calculate the Working Set Size (WSS) for the process.
	
	WSS = 0; //Reset the WSS for each new process.	
	
	vma = task->mm->mmap;
	while(vma != NULL){
		for (address = vma->vm_start; address < vma->vm_end; address += PAGE_SIZE){
		
			walk_page_tables(task->mm, address);
			if (ptep_test_and_clear_young(vma, address, ptep_curr) == 1)
				WSS++;	
		}
		vma = vma->vm_next;
	}
	
	// Convert all the sizes to KiloBytes.
	
	RSS = (RSS * PAGE_SIZE) / 1024;
	SWAP = (SWAP * PAGE_SIZE) / 1024;
	WSS = (WSS * PAGE_SIZE) / 1024;
	
	//Print out the statistics.
	
	printk(KERN_INFO "PID [%d]: RSS=%lu KB, SWAP=%lu KB, WSS=%lu KB\n", pid, RSS, SWAP, WSS);
	
	// Reset the counters to 0 for future processes that are going to be traversed.
	
	RSS = 0;
	SWAP = 0;
	
	return HRTIMER_RESTART;
}

//Initializing module that detects the process and starts the timer.
//static
int __init init_module(void){
	
	ktime_t currtime = ktime_add(ktime_get(), ktime_set(0,10e9));
	struct task_struct *currtask;
	
	for_each_process(currtask){
	
		if (currtask->pid == pid){
		
			task = currtask;
			hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
			timer.function = &no_restart_callback;
			hrtimer_start(&timer, currtime, HRTIMER_MODE_ABS);
			}
	}
	return 0;
}

//The exiting module, cancels the timer, and exits the program.

static void __exit exit_module(void){
	hrtimer_cancel(&timer);
}
//The initializing and exiting modules are alloted to functions.

module_init(init_module);
module_exit(exit_module);
MODULE_LICENSE("GPL");
