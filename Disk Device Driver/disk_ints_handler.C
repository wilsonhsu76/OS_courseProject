/* 
    File:disk_ints_handler.C

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "interrupts.H"
#include "disk_ints_handler.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

DiskIntsHandler::DiskIntsHandler() {
}

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S i m p l e T i m e r */
/*--------------------------------------------------------------------------*/


void DiskIntsHandler::handle_interrupt(REGS *_r) {
	//do nothing now...just register this dummy function for interrupts.C later sends EOI.
}


