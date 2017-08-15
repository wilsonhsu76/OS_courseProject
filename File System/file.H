/*
     File        : file.H

     Author      : Riccardo Bettati
     Modified    : 2017/05/01

     Description : Simple File class with sequential read/write operations.
 
*/

#ifndef _FILE_H_
#define _FILE_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/
#include "file_system.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* class  F i l e   */
/*--------------------------------------------------------------------------*/

class File  {
    
private:
    /* -- your file data structures here ... */
	int file_id;
	unsigned int file_inode_index;
	unsigned int file_size;
	unsigned long file_start_block;
	unsigned int current_location;   //unit: bytes; range: 0~size
	unsigned long current_block;
    /* -- maybe it would be good to have a reference to the file system? */
	FileSystem* file_system;

public:
    File(int _file_id, unsigned int _file_size, unsigned long _file_start_block, unsigned int _file_inode_index, FileSystem* _file_system);
    /* Constructor for the file handle. Set the ’current
     position’ to be at the beginning of the file. */
    
    int Read(unsigned int _n, char * _buf);
    /* Read _n characters from the file starting at the current location and
     copy them in _buf.  Return the number of characters read. 
     Do not read beyond the end of the file. */
    
    void Write(unsigned int _n, const char * _buf);
    /* Write _n characters to the file starting at the current location, 
     if we run past the end of file, 
     we increase the size of the file as needed. */
    
    void Reset();
    /* Set the ’current position’ at the beginning of the file. */
    
    void Rewrite();
    /* Erase the content of the file. Return any freed blocks.
     Note: This function does not delete the file! It just erases its content. */
    
    bool EoF();
    /* Is the current location for the file at the end of the file? */

};

#endif
