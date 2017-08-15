/* 
    File: file_system.H

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 10/04/05

    Description: Simple File System.
    

*/

#ifndef _FILE_SYSTEM_H_                   // include file only once
#define _FILE_SYSTEM_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/
#define BLOCK_BYTESIZE 512
#define SUPERBLOCK_VAR_BYTESIZE 16
#define INODE_FLAGS_COUNT ((BLOCK_BYTESIZE-SUPERBLOCK_VAR_BYTESIZE)*8)
#define FILE_SYSTEM_METABLOCKS 94
//0-93 for meta data, 0: superblock 1-93: inodes(1:file id, 2:file size, 3: start_block_index)  
/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

//#include "file.H"
#include "simple_disk.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */
class File;

/*--------------------------------------------------------------------------*/
/* F i l e S y s t e m  */
/*--------------------------------------------------------------------------*/

class FileSystem {

friend class File; /* -- not sure if we need this; feel free to delete */

private:
     /* -- DEFINE YOUR FILE SYSTEM DATA STRUCTURES HERE. */
     
    SimpleDisk * disk;
	
	//below saved in superblock(block_index:0)
    unsigned int size;
	unsigned int prep_freeblock_index;  //in this file system, the index of free block which is assigned next
	unsigned int file_count;            //the number of files in this file system
	int last_modified_inode;            //last modified inode: for deciding which filesystem part should update to disk. 
    bool inodeFlags_array[INODE_FLAGS_COUNT]; //if 1: used, 0:unused, totally: 3968
	//above saved in superblock
	
	int inodeFileID_array[128]; //one block saves (512/4) inodes. for file ID
	unsigned int inodeFileSize_array[128]; //for file size
	unsigned long inodeFileStartBlockIndex_array[128]; //for start_block_index for this file
	//inodes management by bitmap inodeFlags_array. data free block management by available link list

public:

    FileSystem();
    /* Just initializes local data structures. Does not connect to disk yet. */
    
    bool Mount(SimpleDisk * _disk);
    /* Associates this file system with a disk. Limit to at most one file system per disk.
     Returns true if operation successful (i.e. there is indeed a file system on the disk.) */
    
    static bool Format(SimpleDisk * _disk, unsigned int _size);
    /* Wipes any file system from the disk and installs an empty file system of given size. */
    
    File * LookupFile(int _file_id);
    /* Find file with given id in file system. If found, return the initialized
     file object. Otherwise, return null. */
    
    bool CreateFile(int _file_id);
    /* Create file with given id in the file system. If file exists already,
     abort and return false. Otherwise, return true. */
    
    bool DeleteFile(int _file_id);
    /* Delete file with given id in the file system; free any disk block occupied by the file. */
	
	void UpdateInode();
	//inode_index saved in last_modified_inode
	//update superblock(1 block), inode (3blocks) to disk after FileSystem(CreateFile/DeleteFile), File(write/rewrite)

    void LoadInode(int _inode_index);
	//load inodeFileID_array, inodeFileSize_array, inodeFileStartBlockIndex_array from disk
	
    int FindFreeInode();
	//return the index of free inodes in inode array.-1: means no free inode. when File create.
   
};
#endif
