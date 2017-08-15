/*
     File        : blocking_disk.H

     Author      : 

     Date        : 
     Description : 

*/

#ifndef _MIRRORED_DISK_H_
#define _MIRRORED_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "scheduler.H"
#include "console.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class MirroredDisk : public SimpleDisk {
private:
	DISK_ID      disk_id;            /* This disk is either MASTER or SLAVE */
	unsigned int disk_size;          /* In Byte */
    void issue_operation(DISK_OPERATION _op, unsigned long _block_no);
    bool is_mirror_ready();          //check if mirror_disk at ata1 ready

public:
   MirroredDisk(DISK_ID _disk_id, unsigned int _size); 
   /* Creates a BlockingDisk device with the given size connected to the 
      MASTER or SLAVE slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness. 
      In a real system, we would infer this information from the 
      disk controller. */

   /* DISK OPERATIONS */

   virtual void read(unsigned long _block_no, unsigned char * _buf);
   /* Reads 512 Bytes from the given block of the disk and copies them 
      to the given buffer. No error check! */
   //just issue now

   virtual void write(unsigned long _block_no, unsigned char * _buf);
   /* Writes 512 Bytes from the buffer to the given block on the disk. */
   
   bool check_disk_ready();   //for outer function to check chennel status
   
   void read_action(unsigned char * _buf);  //do read action
   void write_action(unsigned char * _buf);  //do write action
   
};

#endif
