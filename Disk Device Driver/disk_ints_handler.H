/* 
    File: disk_ints_handler.H

    This is a dummy function of a disk interrupt handler.
    The function is implemented in 'handle_interrupt'.

*/

#ifndef _DISK_INTS_HANDLER_H_
#define _DISK_INTS_HANDLER_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "interrupts.H"

/*--------------------------------------------------------------------------*/
/* S I M P L E   T I M E R  */
/*--------------------------------------------------------------------------*/

class DiskIntsHandler : public InterruptHandler {

private:
  /* none */

public :
  DiskIntsHandler();
  /* Initialize the DiskIntsHandler*/

  virtual void handle_interrupt(REGS *_r);
  /* This must be installed as the interrupt handler for the disk irq 14 and irq 15 
     when the system gets initialized. (e.g. in "kernel.C")  
  */

};

#endif
