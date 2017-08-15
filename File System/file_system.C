/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2017/05/01

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define BLOCK_BYTESIZE 512
#define SUPERBLOCK_VAR_BYTESIZE 16
#define INODE_FLAGS_COUNT ((BLOCK_BYTESIZE-SUPERBLOCK_VAR_BYTESIZE)*8)
#define FILE_SYSTEM_METABLOCKS 94

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file_system.H"
#include "file.H"

static unsigned char tmp_FS_buf[512];  //static allocation avoid stack overflow

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() 
{
    Console::puts("In file system constructor.\n");
	disk = NULL;
	size = 0;
	prep_freeblock_index = 0;
	file_count = 0;
	last_modified_inode = -1;
	memset(inodeFlags_array, 0, BLOCK_BYTESIZE - SUPERBLOCK_VAR_BYTESIZE);
	//File::file_system = this;  //ptr in file
}

/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/

bool FileSystem::Mount(SimpleDisk * _disk) 
{
    Console::puts("mounting file system form disk\n");
    disk = _disk;
    disk->read(0, tmp_FS_buf);
    memcpy(&size, tmp_FS_buf, 4);
    memcpy(&prep_freeblock_index, tmp_FS_buf+4, 4);
    memcpy(&file_count, tmp_FS_buf+8, 4);
	memcpy(&last_modified_inode, tmp_FS_buf+12, 4); //set -1 originally
    memcpy(inodeFlags_array, tmp_FS_buf+16, BLOCK_BYTESIZE - SUPERBLOCK_VAR_BYTESIZE);
    return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size)
{
    Console::puts("formatting disk\n");
	memset(tmp_FS_buf, 0, BLOCK_BYTESIZE);
	
	//1-93 indoes, fill all 0.
	unsigned long block_no = 1;
	for(block_no = 1; block_no < FILE_SYSTEM_METABLOCKS; block_no++){
        _disk->write(block_no, tmp_FS_buf);
    }
	
	memset(tmp_FS_buf, -1, BLOCK_BYTESIZE);  //set -1 as default file_id(formal id should >=0)
	for(block_no = 1; block_no < FILE_SYSTEM_METABLOCKS; block_no = block_no + 3){
        _disk->write(block_no, tmp_FS_buf);
    }
	
	memset(tmp_FS_buf, 0, BLOCK_BYTESIZE);
	//initialize data blocks
	for(block_no = FILE_SYSTEM_METABLOCKS; block_no < (_size / BLOCK_BYTESIZE - 1); block_no++){
			unsigned long next_block_no = block_no + 1;
			memcpy(tmp_FS_buf + 508, &next_block_no, 4);
			_disk->write(block_no, tmp_FS_buf);
	}
	int last_block_end = -1;
	memcpy(tmp_FS_buf + 508, &last_block_end, 4);  //-1 in the end of last data blocks
	_disk->write(block_no, tmp_FS_buf);
	
	unsigned int tmp = _size;
	int tmp2 = -1;
	memcpy(tmp_FS_buf, &tmp, 4);         //size
	tmp = FILE_SYSTEM_METABLOCKS; //index:94, is the first free data block.
	memcpy(tmp_FS_buf+4, &tmp, 4);       //prep_freeblock_index
	tmp = 0;
	memcpy(tmp_FS_buf+8, &tmp, 4);       //file_count
	memcpy(tmp_FS_buf+12, &tmp2, 4);      //last_modified_inode(-1)
	memset(tmp_FS_buf+16, 0, BLOCK_BYTESIZE - SUPERBLOCK_VAR_BYTESIZE); //inodes bitmap
	_disk->write(0, tmp_FS_buf);
	return true;
}

File * FileSystem::LookupFile(int _file_id) 
{
    Console::puts("looking up file\n");
	if(_file_id < 0)
		return NULL;  //check this case...

	/* Check if it exists */
	int found_inode_index = -1;
	int first_index_3blocks = -1;
	int second_index_inBlock = -1;
	for(unsigned long i = 1; i < FILE_SYSTEM_METABLOCKS; i = i + 3){
		this->disk->read(i, tmp_FS_buf);
		memcpy(inodeFileID_array, tmp_FS_buf, 512);
		for(int j = 0; j < (BLOCK_BYTESIZE>>2); j++){
			if(inodeFileID_array[j] == _file_id) {
				found_inode_index = (i/3)*(BLOCK_BYTESIZE>>2) + j;
				second_index_inBlock = j;
				break;
			}
		}
		if(found_inode_index >= 0){
			first_index_3blocks = (int)i;
			break;
		}
	}
	
	if(found_inode_index >= 0){
		int parameter_file_id = 0;
		unsigned int parameter_file_size = 0;
		unsigned long parameter_start_block_no = 0;
		
		if(inodeFlags_array[found_inode_index] == true){
			this->LoadInode(found_inode_index);	
			parameter_file_id = inodeFileID_array[second_index_inBlock];
			parameter_file_size = inodeFileSize_array[second_index_inBlock];
			parameter_start_block_no = inodeFileStartBlockIndex_array[second_index_inBlock];

			File* pfileObj = NULL; 
			pfileObj = new File(parameter_file_id, parameter_file_size, parameter_start_block_no, found_inode_index, this);
			return pfileObj;
		}
		else{
			return NULL;
		}
	}
	else{
        return NULL;
	}	
}
void FileSystem::UpdateInode()
{
	int inode_index = last_modified_inode;
	if(last_modified_inode < 0){
		return;
	}
	int first_index_3blocks = (inode_index >>7)*3 + 1;
	int second_index_inBlock = (inode_index % 128);
	
	//update superblock in disk
	last_modified_inode = -1;
	memcpy(tmp_FS_buf, &size, 4);         //size
	memcpy(tmp_FS_buf+4, &prep_freeblock_index, 4);       //prep_freeblock_index
	memcpy(tmp_FS_buf+8, &file_count, 4);       //file_count
	memcpy(tmp_FS_buf+12, &last_modified_inode, 4);      //last_modified_inode
	memcpy(tmp_FS_buf+16, inodeFlags_array, BLOCK_BYTESIZE - SUPERBLOCK_VAR_BYTESIZE); //inodes bitmap
	disk->write(0, tmp_FS_buf);
	
	//update inode_file_id in disk:
	memcpy(tmp_FS_buf, inodeFileID_array, 512);
	this->disk->write(first_index_3blocks, tmp_FS_buf);
	//update inode_file_size in disk:
	memcpy(tmp_FS_buf, inodeFileSize_array, 512);
	this->disk->write(first_index_3blocks+1, tmp_FS_buf);
	//update inode_file_startblock in disk:
	memcpy(tmp_FS_buf, inodeFileStartBlockIndex_array, 512);
	this->disk->write(first_index_3blocks+2, tmp_FS_buf);
}

void FileSystem::LoadInode(int _inode_index)
{
	int inode_index = _inode_index;
	if(inode_index < 0){
		return;
	}
	int first_index_3blocks = (inode_index >>7)*3 + 1;
	
	//read inode_file_id from disk:
	this->disk->read(first_index_3blocks, tmp_FS_buf);
	memcpy(inodeFileID_array, tmp_FS_buf, 512);
	//read inode_file_size from disk:
	this->disk->read(first_index_3blocks+1, tmp_FS_buf);
	memcpy(inodeFileSize_array, tmp_FS_buf, 512);
	//read inode_file_startblock from disk:
	this->disk->read(first_index_3blocks+2, tmp_FS_buf);
	memcpy(inodeFileStartBlockIndex_array, tmp_FS_buf, 512);
}

int FileSystem::FindFreeInode()
{
	int inodes_count = INODE_FLAGS_COUNT;
	int free_inodes_index = -1;
	for(int i = 0; i < inodes_count; i++ ){
		if(inodeFlags_array[i]==false){
			free_inodes_index = i;
			break;
		}
	}
	return free_inodes_index;
}

bool FileSystem::CreateFile(int _file_id) 
{
    Console::puts("creating file\n");
	/* Check whether this file exists */
	int found_inode_index = -1;
	int first_index_3blocks = -1;
	int second_index_inBlock = -1;
	for(unsigned long i = 1; i < FILE_SYSTEM_METABLOCKS; i = i + 3){
		this->disk->read(i, tmp_FS_buf);
		memcpy(inodeFileID_array, tmp_FS_buf, 512);
		for(int j = 0; j < (BLOCK_BYTESIZE>>2); j++){
			if(inodeFileID_array[j] == _file_id) {
				found_inode_index = (i/3)*(BLOCK_BYTESIZE>>2) + j;
				second_index_inBlock = j;
				break;
			}
		}
		if(found_inode_index >= 0){
			first_index_3blocks = (int)i;
			break;
		}
	}
	
	if(found_inode_index >= 0){
		return false;
	}
	else{
		int available_inode_index =  this->FindFreeInode();
		if(available_inode_index < 0){
			return false;
		}
		else{
			inodeFlags_array[available_inode_index] = true;
			this->LoadInode(available_inode_index);
			second_index_inBlock = (available_inode_index % 128) ;
			inodeFileID_array[second_index_inBlock] = _file_id;
			inodeFileSize_array[second_index_inBlock] = 0;
			inodeFileStartBlockIndex_array[second_index_inBlock] = 0; //0 means not allocate any
			last_modified_inode = available_inode_index;
			file_count++;
			this->UpdateInode();  //write metadata to disk
			return true; 
		}
	}
}

bool FileSystem::DeleteFile(int _file_id) 
{
    Console::puts("deleting file\n");
	/* Check whether this file exists */
	int found_inode_index = -1;
	int first_index_3blocks = -1;
	int second_index_inBlock = -1;
	for(unsigned long i = 1; i < FILE_SYSTEM_METABLOCKS; i = i + 3){
		this->disk->read(i, tmp_FS_buf);
		memcpy(inodeFileID_array, tmp_FS_buf, 512);
		for(int j = 0; j < (BLOCK_BYTESIZE>>2); j++){
			if(inodeFileID_array[j] == _file_id) {
				found_inode_index = (i/3)*(BLOCK_BYTESIZE>>2) + j;
				second_index_inBlock = j;
				break;
			}
		}
		if(found_inode_index >= 0){
			first_index_3blocks = (int)i;
			break;
		}
	}
	
	if(found_inode_index < 0){
		return false;
	}
	else if(inodeFlags_array[found_inode_index] == false){
		return false;
	}
	else{
		this->LoadInode(found_inode_index);
		unsigned int block_no = inodeFileStartBlockIndex_array[second_index_inBlock];
        unsigned int next_block_no = 0;
		int test_file_size = (int)inodeFileSize_array[second_index_inBlock];
		
		 while(1) {
			if(test_file_size <=0){
					break;  //delete until EOF
			}
			disk->read(block_no, tmp_FS_buf);  //get next block ptr of the current block_no
			memcpy(&next_block_no, tmp_FS_buf+508, 4);
			memcpy(tmp_FS_buf+508, &prep_freeblock_index, 4);  //set the current block_no's next ptr to free block.
			disk->write(block_no, tmp_FS_buf);
			prep_freeblock_index = block_no;  //set this current block is free block.
			block_no = next_block_no;  //handle next_block_no in the next loop
			test_file_size -=508;
        }
		
		this->LoadInode(found_inode_index); //read metadata from disk
		inodeFileID_array[second_index_inBlock] = -1;
		inodeFileSize_array[second_index_inBlock] = 0;
		inodeFileStartBlockIndex_array[second_index_inBlock] = 0; //0 means not allocate any
		last_modified_inode = found_inode_index;
		file_count--;
		inodeFlags_array[found_inode_index] = false;
		this->UpdateInode();  //write metadata to disk
		return true;
	}
	
}
