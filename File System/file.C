/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2017/05/01

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file.H"

static unsigned char tmp_F_buf[512];  //static allocation avoid stack overflow

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(int _file_id, unsigned int _file_size, unsigned long _file_start_block, unsigned int _file_inode_index, FileSystem* _file_system)
{
    /* We will need some arguments for the constructor, maybe pointer to disk
     block with file management and allocation data. */
    Console::puts("In file constructor.\n");
	file_id = _file_id;
	file_size = _file_size;
	file_start_block = _file_start_block;
	current_block = _file_start_block;
	current_location = 0;  //unit: bytes; range: 0~size-1,(current_location == size): EOF
	file_inode_index = _file_inode_index;
	file_system = _file_system;
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char * _buf)
{
    //Console::puts("reading from file\n");
	if(file_size == 0 || EoF())
		return 0; //can't read anything
   
	unsigned int char_read = 0;  //how many char(Byte) has it read.
	
	while(_n) {
		file_system->disk->read(current_block, tmp_F_buf);  //disk==>tmp_F_buf
		unsigned int offset = current_location % 508;
		unsigned int char_read_at_this_loop = 0; //how many char(Byte) will read in this loop
		if (_n > 508 - offset)
			char_read_at_this_loop = 508 - offset;
		else
			char_read_at_this_loop = _n ;

		if(char_read_at_this_loop > file_size - current_location)  //check total size
				char_read_at_this_loop = file_size - current_location;
		
		memcpy(_buf+char_read, tmp_F_buf+offset, char_read_at_this_loop);  //tmp_F_buf==>_buf(target)
		char_read += char_read_at_this_loop;
		_n -= char_read_at_this_loop;
		current_location += char_read_at_this_loop;
		if((!EoF()) && (current_location % 508 == 0)) {    //move to next block
			memcpy(&current_block, tmp_F_buf+508, 4);
		}
    }
	return char_read;
}


void File::Write(unsigned int _n, const char * _buf)
{
    //Console::puts("writing to file\n");
	if(_n == 0)
		return; //can't write anything

	unsigned int char_write = 0;  //how many char(Byte) has it written.

	while(_n) {
		// assign new block?
		if(EoF()&& (current_location % 508 == 0)){  //if there is "EOF" && "in the end of block" for this writting
			if(file_system->prep_freeblock_index!= -1){
				if(current_block!=0){    //already has its own data block
					file_system->disk->read(current_block, tmp_F_buf);
					memcpy(tmp_F_buf+508, &(file_system->prep_freeblock_index), 4);
					file_system->disk->write(current_block, tmp_F_buf);
				}
				current_block = file_system->prep_freeblock_index;  //assign the cuurent prep_freeblock_index
			}
			else{
				Console::puts("no any free block for this file writing\n");
				file_system->LoadInode(file_inode_index);  //update_metablocks
				int second_index_inblock = file_inode_index % 128;
				file_system->inodeFileSize_array[second_index_inblock] = file_size;  //update file size
				file_system->inodeFileStartBlockIndex_array[second_index_inblock] = file_start_block; //update start_block
				file_system->last_modified_inode = file_inode_index;
				file_system->UpdateInode();
				return;
			}

			if(file_start_block == 0){
				file_start_block = file_system->prep_freeblock_index;
			}
			file_system->disk->read(current_block, tmp_F_buf);  //update prep_freeblock_index
			memcpy(&(file_system->prep_freeblock_index), tmp_F_buf+508, 4);
		}
		
		//write task
		unsigned int offset = current_location % 508;
		file_system->disk->read(current_block, tmp_F_buf);  //disk==>tmp_F_buf
		
		unsigned int char_write_at_this_loop = 0;  //how many char(Byte) will write in this loop
		if (_n > 508 - offset)
			char_write_at_this_loop = 508 - offset;
		else
			char_write_at_this_loop = _n ;
		
		memcpy(tmp_F_buf + offset, _buf + char_write, char_write_at_this_loop);  //_buf==>tmp_F_buf
		file_system->disk->write(current_block, tmp_F_buf);
	
		if(char_write_at_this_loop + current_location > file_size)  //update file_size
			file_size = current_location + char_write_at_this_loop;

		char_write += char_write_at_this_loop;
		_n -= char_write_at_this_loop;
		current_location += char_write_at_this_loop;
	}
	
	//update meta block
	file_system->LoadInode(file_inode_index);
	int second_index_inblock = file_inode_index % 128;
	file_system->inodeFileSize_array[second_index_inblock] = file_size;  //update file size
	file_system->inodeFileStartBlockIndex_array[second_index_inblock] = file_start_block; //update start_block
	file_system->last_modified_inode = file_inode_index;
	file_system->UpdateInode();

	return;
}

void File::Reset()
{
    //Console::puts("reset current position in file\n");
    current_block = file_start_block;
	current_location = 0;
}

void File::Rewrite()
{
    //Console::puts("erase content of file\n");
	unsigned int block_no = file_start_block;
	unsigned int next_block_no = 0;
	int test_file_size = (int)file_size;

	//release all data block in this file //like file_system::delete
	 while(1) {
		if(test_file_size <=0){
				break;  //delete until EOF
		}
		file_system->disk->read(block_no, tmp_F_buf);  //get next block ptr of the current block_no
		memcpy(&next_block_no, tmp_F_buf+508, 4);
		memcpy(tmp_F_buf+508, &(file_system->prep_freeblock_index), 4);  //set the current block_no's next ptr to free block.
		file_system->disk->write(block_no, tmp_F_buf);
		file_system->prep_freeblock_index = block_no;  //set this current block is free block.
		block_no = next_block_no;  //handle next_block_no in the next loop
		test_file_size -=508;
	}
	file_start_block = 0;
	file_size = 0;
	current_location = 0;
	current_block = 0;   //0 means not any allocate to this file
	
	//set info in inode and update metablocks to DISK
	file_system->LoadInode(file_inode_index);
	int second_index_inblock = file_inode_index % 128;
	file_system->inodeFileSize_array[second_index_inblock] = file_start_block;
	file_system->inodeFileStartBlockIndex_array[second_index_inblock] = file_size; //0 means not allocate any
	file_system->last_modified_inode = file_inode_index;
	file_system->UpdateInode();  //write metadata to disk
}


bool File::EoF() {
    //Console::puts("testing end-of-file condition\n");
    return (current_location == file_size);
}
