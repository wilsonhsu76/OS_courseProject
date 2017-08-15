/*
     File        : blocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */
#define SYSTEM_DISK_SIZE (10 MB)

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "blocking_disk.H"
extern Scheduler* SYSTEM_SCHEDULER;
MirroredDisk * SYSTEM_MIRROR_DISK;
FilterLock  *  SYSTEM_DISK_LOCK;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

BlockingDisk::BlockingDisk(DISK_ID _disk_id, unsigned int _size) 
  : SimpleDisk(_disk_id, _size) {
	disk_id = _disk_id;
	disk_size = _size;
	SYSTEM_MIRROR_DISK = new MirroredDisk(_disk_id, _size);  //user can't see
	SYSTEM_DISK_LOCK = new FilterLock(); 
}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void BlockingDisk::issue_operation(DISK_OPERATION _op, unsigned long _block_no) {
  Machine::outportb(0x1F1, 0x00); /* send NULL to port 0x1F1         */
  Machine::outportb(0x1F2, 0x01); /* send sector count to port 0X1F2 */
  Machine::outportb(0x1F3, (unsigned char)_block_no);
                         /* send low 8 bits of block number */
  Machine::outportb(0x1F4, (unsigned char)(_block_no >> 8));
                         /* send next 8 bits of block number */
  Machine::outportb(0x1F5, (unsigned char)(_block_no >> 16));
                         /* send next 8 bits of block number */
  Machine::outportb(0x1F6, ((unsigned char)(_block_no >> 24)&0x0F) | 0xE0 | (disk_id << 4));
                         /* send drive indicator, some bits, 
                            highest 4 bits of block no */

  Machine::outportb(0x1F7, (_op == READ) ? 0x20 : 0x30);

}

void BlockingDisk::read(unsigned long _block_no, unsigned char * _buf) {
	SYSTEM_DISK_LOCK->acquire();
	issue_operation(READ, _block_no);  //issue read command to main disk
	SYSTEM_MIRROR_DISK->read(_block_no, _buf);  //issue read command to mirror disk
	
	while ((!is_ready()) && (!SYSTEM_MIRROR_DISK->check_disk_ready())) {
		SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
		SYSTEM_SCHEDULER->yield();
	}
	/* read data from port */
	if(is_ready()){   //read from main
		int i;
		unsigned short tmpw;
		for (i = 0; i < 256; i++) {
			tmpw = Machine::inportw(0x1F0);
			_buf[i*2]   = (unsigned char)tmpw;
			_buf[i*2+1] = (unsigned char)(tmpw >> 8);
		}
	}
	else{    //read from mirror
		SYSTEM_MIRROR_DISK->read_action(_buf);
	}
	SYSTEM_DISK_LOCK->release();
}


void BlockingDisk::write(unsigned long _block_no, unsigned char * _buf) {
	SYSTEM_DISK_LOCK->acquire();
	bool main_write_done = false;
	bool mirror_write_done = false;
	issue_operation(WRITE, _block_no);  //send write issue to main disk
	SYSTEM_MIRROR_DISK->write(_block_no, _buf);  //send write issue to mirrored disk
	
	while ((!main_write_done) || ((!mirror_write_done))) {
		if(!main_write_done){
			if(is_ready()){
				/* write data to port */
				int i; 
				unsigned short tmpw;
				for (i = 0; i < 256; i++) {
					tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
					Machine::outportw(0x1F0, tmpw);
				}
				main_write_done = true;
			}
		}
		if(!mirror_write_done){
			if(SYSTEM_MIRROR_DISK->check_disk_ready()){
				SYSTEM_MIRROR_DISK->write_action(_buf);
				mirror_write_done = true;
			}
		}

		if ((!main_write_done) || ((!mirror_write_done))){
	        SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
			SYSTEM_SCHEDULER->yield();
		}
	}
	SYSTEM_DISK_LOCK->release();
}

FilterLock::FilterLock(){
	for(int i = 0; i < Thread_N - 1; i++){
		level[i] = 0;
		victim[i] = 0;
	}
	level[Thread_N - 1] = 0;
}

void FilterLock::acquire() 
{
	for (int j = 1; j < Thread_N; j++) {
		level [Thread::CurrentThread()->ThreadId()] = j;
		victim [j] = Thread::CurrentThread()->ThreadId();
		// wait while conflicts exist
		while (sameOrHigher(Thread::CurrentThread()->ThreadId(),j) &&
		victim[j] == Thread::CurrentThread()->ThreadId()){
			SYSTEM_SCHEDULER->resume(Thread::CurrentThread());  //don't busy wait, move to next ready thread
			SYSTEM_SCHEDULER->yield();
		}
	}
}

bool FilterLock::sameOrHigher(int i, int j) 
{
	for(int k = 0; k < Thread_N; k++){
		if (k != i && level[k] >= j) return true;
	}
	return false;
}

void FilterLock::release() {
	level[Thread::CurrentThread()->ThreadId()] = 0;
}	
