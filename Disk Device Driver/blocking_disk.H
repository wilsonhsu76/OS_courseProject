/*
     File        : blocking_disk.H

     Author      : 

     Date        : 
     Description : 

*/

#ifndef _BLOCKING_DISK_H_
#define _BLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */
#define Thread_N  4       //possible thread maximum number in this sysytem, assume 4  

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "scheduler.H"
#include "console.H"
#include "mirrored_disk.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class BlockingDisk : public SimpleDisk {
private:
    DISK_ID      disk_id;            /* This disk is either MASTER or SLAVE */
    unsigned int disk_size;          /* In Byte */
    void issue_operation(DISK_OPERATION _op, unsigned long _block_no);
    
public:
   BlockingDisk(DISK_ID _disk_id, unsigned int _size); 
   /* Creates a BlockingDisk device with the given size connected to the 
      MASTER or SLAVE slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness. 
      In a real system, we would infer this information from the 
      disk controller. */

   /* DISK OPERATIONS */

   virtual void read(unsigned long _block_no, unsigned char * _buf);
   /* Reads 512 Bytes from the given block of the disk and copies them 
      to the given buffer. No error check! */

   virtual void write(unsigned long _block_no, unsigned char * _buf);
   /* Writes 512 Bytes from the buffer to the given block on the disk. */

};

class FilterLock{
	private:
		volatile int level[Thread_N]; 
		volatile int victim[Thread_N-1];
	public:
		FilterLock();
	    void acquire();
		bool sameOrHigher(int i, int j);
		void release();	
};

#endif
