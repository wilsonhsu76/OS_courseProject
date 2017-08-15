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

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "mirrored_disk.H"
extern Scheduler* SYSTEM_SCHEDULER;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

MirroredDisk::MirroredDisk(DISK_ID _disk_id, unsigned int _size) 
  : SimpleDisk(_disk_id, _size) {
	disk_id = _disk_id;
	disk_size = _size;
}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void MirroredDisk::issue_operation(DISK_OPERATION _op, unsigned long _block_no) {
  Machine::outportb(0x171, 0x00); /* send NULL to port 0x1F1         */
  Machine::outportb(0x172, 0x01); /* send sector count to port 0X1F2 */
  Machine::outportb(0x173, (unsigned char)_block_no);
                         /* send low 8 bits of block number */
  Machine::outportb(0x174, (unsigned char)(_block_no >> 8));
                         /* send next 8 bits of block number */
  Machine::outportb(0x175, (unsigned char)(_block_no >> 16));
                         /* send next 8 bits of block number */
  Machine::outportb(0x176, ((unsigned char)(_block_no >> 24)&0x0F) | 0xE0 | (disk_id << 4));
                         /* send drive indicator, some bits, 
                            highest 4 bits of block no */

  Machine::outportb(0x177, (_op == READ) ? 0x20 : 0x30);

}

bool MirroredDisk::is_mirror_ready() {
	return ((Machine::inportb(0x177) & 0x08) != 0);
}

void MirroredDisk::read(unsigned long _block_no, unsigned char * _buf) {
	issue_operation(READ, _block_no);  //just issue read command
}

void MirroredDisk::read_action(unsigned char * _buf) {
    /* read data from port */
	int i;
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = Machine::inportw(0x170);
		_buf[i*2]   = (unsigned char)tmpw;
		_buf[i*2+1] = (unsigned char)(tmpw >> 8);
	}
}

void MirroredDisk::write(unsigned long _block_no, unsigned char * _buf) {
	issue_operation(WRITE, _block_no);
}

void MirroredDisk::write_action(unsigned char * _buf) {
	/* write data to port */
	int i; 
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
		Machine::outportw(0x170, tmpw);
	}
}

bool MirroredDisk::check_disk_ready(){
	return is_mirror_ready();
}
