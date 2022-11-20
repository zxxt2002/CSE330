/**********************************************************************
 * Authors: Parker Ottaway, Derek Allen, Auston Hein
 *
 * Description: This is a kernel module that takes in a
 * 				task, walks the page table and outputs
 * 				the starting and ending addresses of
 * 				100 contiguous pages of different kinds
 * 				of memory, including invalid pages,
 * 				pages swapped to the disk, and pages
 * 				present in memory.
 *
 * Purpose: To understand how data is stored and
 * 			accessed in a computer.
 *
 **********************************************************************/

/*

Code modified from authors above by:

Aaron Huggins,
!!! Make sure to add your name here and to the MODULE_AUTHOR call. We can Remove the other names if needed !!!
*/



#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/sched/signal.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>

 /* Define the number of similar pages in a row. */
#define CONTIG_PAGES 100

/* Stops the kernel from being reported as "tainted." */
MODULE_LICENSE("GPL");
/* Name the module's authors. */
MODULE_AUTHOR("Parker Ottaway, Derek Allen, and Auston Hein. Modified by Aaron Huggins, ");


int pid;							// Process ID
module_param(pid, int, S_IRUSR);	// Gather process ID as parameter

#pragma region VariableDeclaration

struct task_struct* task;			// The current task (or "process")
struct vm_area_struct* vma;			// Memory Region

int ii;
int	physicalMemCount = 0;
int swapCount = 0;

int contiguousPhysical = 0;
int contiguousSwap = 0;
int contiguousInvalid = 0;

unsigned long beginAddressPhysical = 0;
unsigned long endAddressPhysical = 0;
unsigned long beginAddressSwap = 0;
unsigned long endAddressSwap = 0;
unsigned long beginAddressInvalid = 0;
unsigned long endAddressInvalid = 0;

pgd_t* pgd;
p4d_t* p4d;
pud_t* pud;
pmd_t* pmd;
pte_t* ptep;

#pragma endregion

// Finds the appropriate proccess's task struct.
int findTask() {
	for_each_process(task) {
		if (task->pid == pid) return 1;
	}

	return 0;
}

// Loads a page's table information
void loadDirs() {
	pgd = pgd_offset(task->mm, ii);	// Get Global dir
	p4d = p4d_offset(pgd, ii);		// Get 4th dir
	pud = pud_offset(p4d, ii);		// Get Upper dir
	pmd = pmd_offset(pud, ii);		// Get middle dir
	ptep = pte_offset_map(pmd, ii);	// Get page table entry
}

// Main function. Initializes necessary values and runs the primary purpose of this program.
int memMod_init(void) {
	// Find correct task
	int taskFound = findTask();
	if (!taskFound) return -1;

	vma = task->mm->mmap;	// Access vm_area of task
	while (vma) {			// Continues until vm_area is out of bounds
		// Parse every page in vma
		for (ii = vma->vm_start; ii <= (vma->vm_end - PAGE_SIZE); ii += PAGE_SIZE) {
			loadDirs();						// Get page's table information
			down_read(&task->mm->mmap_sem);	// Lock the page

			// Check that the page table entry is valid (entry exists)
			if (!pte_none(*ptep)) {	// Valid

				/* Check if in memory or on disk. */
				if (pte_present(*ptep)) { /* Page is in memory. */
					physicalMemCount++;

					// Within bounds of process pages
					if (contiguousPhysical != CONTIG_PAGES + 1) contiguousPhysical++;
					if (contiguousPhysical == 1) beginAddressPhysical = ii;

					/* If others are non-zero, set them to zero. */
					if (contiguousSwap != CONTIG_PAGES + 1) {
						contiguousSwap = 0;
						beginAddressSwap = 0;
					}
					if (contiguousInvalid != CONTIG_PAGES + 1) {
						contiguousInvalid = 0;
						beginAddressInvalid = 0;
					}

					/* Save ending address. */
					if (contiguousPhysical == CONTIG_PAGES) {
						endAddressPhysical = ii;
						printk("Starting Physical Address: %lu\tEnding Physical Address: %lu", beginAddressPhysical, endAddressPhysical);
						/* Set contiguous counter to 101 so that it does not keep printing. */
						contiguousPhysical++;
					}
				}
				else { /* Page is swapped. */
					 /* Increment counters. */
					swapCount++;
					/* If the swap memory chunk has not been found yet. */
					if (contiguousSwap != CONTIG_PAGES + 1) {
						contiguousSwap++;
					}

					/* Track first address. */
					if (contiguousSwap == 1) {
						beginAddressSwap = ii;
					}

					/* If others are non-zero, set them to zero. */
					if (contiguousPhysical != CONTIG_PAGES + 1) {
						contiguousPhysical = 0;
						beginAddressPhysical = 0;
					}
					if (contiguousInvalid != CONTIG_PAGES + 1) {
						contiguousInvalid = 0;
						beginAddressInvalid = 0;
					}

					/* Save ending address. */
					if (contiguousSwap == CONTIG_PAGES) {
						endAddressSwap = ii;
						printk("Starting Swap Address: %lu\tEnding Swap Address: %lu", beginAddressSwap, endAddressSwap);
						/* Set contiguous counter to 101 so that it does not keep printing. */
						contiguousSwap++;
					}

				}
			}
			else { /* Page is invalid. */
				 /* If the invalid memory chunk has not been found yet. */
				if (contiguousInvalid != CONTIG_PAGES + 1) {
					contiguousInvalid++;
				}

				/* Track first page. */
				if (contiguousInvalid == 1) {
					beginAddressInvalid = ii;
				}

				/* If others are non-zero, set them to zero. */
				if (contiguousPhysical != CONTIG_PAGES + 1) {
					contiguousPhysical = 0;
					beginAddressPhysical = 0;
				}
				if (contiguousSwap != CONTIG_PAGES + 1) {
					contiguousSwap = 0;
					beginAddressSwap = 0;
				}

				/* Save ending address. */
				if (contiguousInvalid == CONTIG_PAGES) {
					endAddressInvalid = ii;
					printk("Starting Invalid Address: %lu\tEnding Invalid Address: %lu", beginAddressInvalid, endAddressInvalid);
					/* Set contiguous counter to 101 so that it does not keep printing. */
					contiguousInvalid++;
				}
			}

			/* Unlock the page from read lock. */
			up_read(&task->mm->mmap_sem);
		}
		/* Get the next virtual address space belonging to the
		   task. */
		vma = vma->vm_next;
	}

	/* Print the PID, number of pages in memory, and number of pages swapped. */
	int workingSetSize = 0;	// WSS for part 4. Not implemented yet.
	printk("PID %d: RSS=%d KB,\tSWAP=%d KB, WSS=%d KB", task->pid, physicalMemCount, swapCount, workingSetSize);
	printk("GOT HERE.");
	return 0;
}

/* Module function that is run when the module is removed from the
   kernel. This will mainly do cleanup. */
void memMod_exit(void) {

}

/* Define the init and exit functions for this kernel module. */
module_init(memMod_init);
module_exit(memMod_exit);