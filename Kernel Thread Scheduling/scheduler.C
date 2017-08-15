/*
 File: scheduler.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {
  queue_size=0;
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
	if (queue_size != 0){
		queue_size--;
		Thread* p_nextThread= ready_queue.dequeue();
		Thread::dispatch_to(p_nextThread);;
	}
}

void Scheduler::resume(Thread * _thread) {
	ready_queue.enqueue(_thread);
	queue_size++;
}

void Scheduler::add(Thread * _thread) {
	ready_queue.enqueue(_thread);
	queue_size++;
}

void Scheduler::terminate(Thread * _thread) {
	bool b_Find = false;
	for (int i = 0; i < queue_size; i++){
		Thread* p_tmpThread = ready_queue.dequeue();
		if ( p_tmpThread->ThreadId() == _thread->ThreadId() )
			b_Find = true;
		else 
			ready_queue.enqueue(p_tmpThread);
	}
	if (b_Find)
		queue_size--; //A--B--C--D, delete D; A--B--C
}
