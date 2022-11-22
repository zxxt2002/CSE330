#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/hrtimer.h>

//The process ID (stored in pid variable) is taken as the command-line input argument.
static int pid = -1;
module_param(pid, int, 0); //Pass the input argument to this kernel module


/*Declaring variables to store the total number of pages that are present in the Resident Set Size (RSS), SWAP, Working Set Size (WSS) respectively
and later used to calculate their size from it.*/

unsigned long rss_count = 0;
unsigned long swap_count = 0;
unsigned long wss_count = 0;

//Variables that will help with walking through the page tables.

unsigned long page_address = 0; //Used to store the page address.
pte_t *curr_pte; //The pointer to currently traversed page entry.

//This function walks through each valid process' page tables and checks if the page is present in the physical memory or in the swap. 

static void walk_page_tables(struct mm_struct *mm, unsigned long address){

	//Pointers to help with going through the five levels of page tables.
	
	pgd_t *pgd;
	p4d_t *p4d;
	pte_t *ptep, pte;
	pud_t *pud;
	pmd_t *pmd;
	
	//Five levels (PGD, P4D, PTE, PUD, PMD) of page levels are walked through.
		
	//Get pgd from mm and the page address

	pgd = pgd_offset(mm,page_address);
	if (pgd_none(*pgd) || pgd_bad(*pgd)){
		return; //Check if pgd is bad or does not exist
	}
	
	//Get p4d from pgd and the page address
	
	p4d = p4d_offset(pgd,page_address);
	if (p4d_none(*p4d) || p4d_bad(*p4d)) {
		return; //Check if p4d is bad or does not exist
	}
	//Get pud from p4d and the page address
	
	pud = pud_offset(p4d,page_address);
	if (pud_none(*pud) || pud_bad(*pud)) {
		return; //Check if pud is bad or does not exist
	}
	//Check if pmd is bad or does not exist
	
	pmd = pmd_offset(pud, page_address);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		return; //Check if pmd is bad or does not exist
	}
	
	// Get pte from pmd and the page address
	
	ptep = pte_offset_map(pmd,page_address);
	if(!ptep) {
		return; //Check if pte does not exist
	}
	
	//Update the pointers since we have reached the page table entry.
		
	pte = *ptep;
	curr_pte = ptep;
	
	//Check If the page is valid and present in the memory, then it is part of the process's RSS. Increment the RSS counter variable.
	
	if ((pte_present(pte)) == 1)
	{
		rss_count++;
	}
	
	//If the page is valid and not present in the memory, then it is in the SWAP. Increment the SWAP counter variable.
	
	else
	{
		swap_count++;
	}
}

/*Declaring some essential pointers to help in traversing the memory regions to help with measurments and calculations.*/

struct task_struct *task; //Used to get the details of all the processes
struct vm_area_struct *vma; //Used to traverse the memory regions.

//Declarations for the 10-second periodic timer that will be defined.

unsigned long timer_interval_ns = 10e9; // The 10 second interval for timers.
static struct hrtimer timer; //Initializng the timer.

// This function checks if the page

int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep){
	
	int ret = 0;
	if (pte_young(*ptep))
		ret = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *) &ptep->pte);
	return ret;
}


//The 10 second interval timer function where the page tables are walked through and the calculations are carried out.

enum hrtimer_restart no_restart_callback(struct hrtimer *timer){
	
	ktime_t currtime, interval;
	currtime = ktime_get();
	interval = ktime_set(0, timer_interval_ns);
	hrtimer_forward(timer, currtime, interval); // Advances the timer's expiration time by 10 seconds from the current time.
	
	/*Calculate the Working Set Size (WSS) for the process. To measure the WSS, during the 10 second interval, the number of pages accessed during that period will be counted by checking the bit of every table entry.*/
	
	vma = task->mm->mmap; //Get the memory region for the current process.

	while(vma != NULL){
		//Loop through the memory region, checking each page.
			
		for (page_address = vma->vm_start; page_address < vma->vm_end; page_address += PAGE_SIZE){
		
			/*Walk through each process's page tables and find out if it is present in the physical memory or in the swap.*/
			
			walk_page_tables(task->mm, page_address);
			
			/*It is checked if the current page table entry has been accessed or not using the "ptep_test_and_clear_young" function. */
			
			if (ptep_test_and_clear_young(vma, page_address, curr_pte) == 1)
			/*If the given pte was proven to be accessed, the acessed bit has already been cleared, and the count of pages in WSS can be increased. */
			{
				wss_count++;	
			}
		}
		
		//Move on to the next memory region.
		
		vma = vma->vm_next;
	}
	
	// Calculate the total size of the pages in for RSS, SWAP, and WSS and convert them to KiloBytes.
	
	unsigned rss_size = (rss_count * PAGE_SIZE) / 1024;
	unsigned swap_size = (swap_count * PAGE_SIZE) / 1024;
	unsigned wss_size = (wss_count * PAGE_SIZE) / 1024;
	
	//Print out the required statistics.
	
	printk(KERN_INFO "PID [%d]: RSS=%lu KB, SWAP=%lu KB, WSS=%lu KB\n", pid, rss_size, swap_size, wss_size);
	
	// Reset the counters to 0 for future processes that are going to be traversed.
	
	rss_count = 0;
	swap_count = 0;
	wss_count = 0;
	
	return HRTIMER_RESTART; //Invoke the HRT periodically.
}

//The Initializing module that detects the process and starts the timer for walking through the pages.

static int __init initialize_program(void){

	//Pointer for traversing through the processes.
	
	struct task_struct *currtask;
	for_each_process(currtask){
		if (currtask->pid == pid && currtask != NULL){
			task = currtask;
			
			/*Initialize the timer, it is monotonic to prevent the elapsed time to ever be negative and it is in HRTIMER_ABS mode to make sure the timer is using an absolute value and not relative to the current time.*/
			
			hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
			
			//Restart call back for the timer, the timer's function will be the one defined before.
			
			timer.function = &no_restart_callback;
			
			//Start the timer with a 10 second interval.
			
			hrtimer_start(&timer, (ktime_add(ktime_get(), ktime_set(0,10e9))), HRTIMER_MODE_ABS);
			}
	}
	return 0;
}

//The exiting module, cancels the timer, and exits the program.

static void __exit end_program(void){
	hrtimer_cancel(&timer); //Cancels the timer.
}
//The initializing and exiting modules are alloted to functions.

module_init(initialize_program);
module_exit(end_program);
MODULE_LICENSE("GPL");
