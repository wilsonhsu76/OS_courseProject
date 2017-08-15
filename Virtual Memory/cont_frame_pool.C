/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

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
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool* ContFramePool::kernel_mem_pool = 0;
ContFramePool* ContFramePool::process_mem_pool = 0;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
    // Bitmap must fit in a single frame!
	if(_n_info_frames==0){
		assert(_n_frames <= FRAME_SIZE * 4);
	}
	else{
	    assert(_n_frames <= _n_info_frames * FRAME_SIZE * 4);
	}
    
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    nFreeFrames = _n_frames;
    info_frame_no = _info_frame_no;
    n_info_frames = _n_info_frames;
    
    // If _info_frame_no is zero then we keep management info in the first
    //frame, else we use the provided frame to keep management info
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
		kernel_mem_pool = this;
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
		process_mem_pool = this;
    }
    
    // Number of frames must be "fill" the bitmap!
    assert ((_n_frames % 4 ) == 0);
    
    
    // Everything ok. Proceed to mark all bits in the bitmap
    for(int i=0; i*4 < _n_frames; i++) {
        bitmap[i] = 0x00;   //01: head occupied, 10: contiguous occupied, 00: available
    }
    
    // Mark the first frame as being used if it is being used
    if(_info_frame_no == 0) {
        bitmap[0] = 0x40; //01: head occupied, 10: contiguous occupied, 00: available
        nFreeFrames--;
    }
    
    Console::puts("Frame Pool initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
	// Any frames left to allocate?
    //assert(nFreeFrames > 0);  //it will return 0
    
    // Find a frame that is not being used and return its frame index.
    // Mark that frame as being used in the bitmap.
    unsigned int frame_no = base_frame_no;
    unsigned int i = 0;
    bool find_flag = false;
	
	while(frame_no + _n_frames <= base_frame_no + nframes)
	{
		frame_no -= (frame_no%4); //for align 4x to do availble_header_search in each character of bitmap
		i = (frame_no-base_frame_no)/4;
		while (((bitmap[i]&0xC0) != 0) && ((bitmap[i]&0x30) != 0) && ((bitmap[i]&0x0C) != 0)&& ((bitmap[i]&0x03) != 0)) {
			i ++;
			frame_no +=4; 
		} 
		
		unsigned char mask = 0xC0;
		unsigned int availble_header_num = 0; //8(first frame, highest priority)+4(second frame)+2(third frame)+1(forth frame, lowest priority)
		unsigned int movement = 0; //know how far the possible range, then I can search from this position at next loop

		for(int x=4; x>0; x--)
		{
            if((mask & bitmap[i]) == 0) {
				availble_header_num += (1 << (x-1));
			} //possible initial head
			mask = mask >> 2;
		}
		 
		if((availble_header_num >= 8) && (!find_flag)){
			movement = 0;
			find_flag = judge_possible_head_with_enough_range(frame_no, i, _n_frames, &movement);
			availble_header_num -=8;
		}
		if((availble_header_num >= 4) && (!find_flag)){
			movement = 0;
			find_flag = judge_possible_head_with_enough_range(frame_no + 1, i, _n_frames, &movement);
			availble_header_num -=4;
			if(find_flag)
				frame_no+=1;
			else
				movement +=1;  //becasue at fail case, we start from the first frame in each element of bitmap[i].
		}
		if((availble_header_num >= 2) && (!find_flag)){
			movement = 0;
			find_flag = judge_possible_head_with_enough_range(frame_no + 2, i, _n_frames, &movement);
			availble_header_num -=2;
			if(find_flag)
				frame_no+=2;
			else
				movement +=2; //becasue at fail case, we start from the first frame in each element of bitmap[i].
		}
		if((availble_header_num >= 1) && (!find_flag)){
			movement = 0;
			find_flag = judge_possible_head_with_enough_range(frame_no + 3, i, _n_frames, &movement);
			availble_header_num -=1;
			if(find_flag)
				frame_no+=3;
			else
				movement+=3; //becasue at fail case, we start from the first frame in each element of bitmap[i].
		}
		if(find_flag==true)
			break;
		else{
			if(movement<=4)
				frame_no += 4;  //find next 4 frame
			else
				frame_no += movement; //can skip search range a lot
			continue;
		}
	}
	
	if(find_flag == true){
		nFreeFrames-=_n_frames;
		// Update bitmap
		unsigned int k = 0;
		unsigned char mask3 = 0x40; //head: 01
		mask3 = mask3 >> (((frame_no - base_frame_no) % 4)*2);
		for(k = 0; k < _n_frames; k++)
		{
			if(mask3 == 0){
				mask3 = 0x80;   //mask wrap around, contiuous:10
				i++;
			}
			bitmap[i] ^= mask3;
			if(k==0)
				mask3 = mask3 >> 1;  //let 0100 to 0010
			else
				mask3 = mask3 >> 2;  //let 1000 to 0010
		}
		return (frame_no);  //head no of allocated memory
	}
	else
	{
		return 0; //means no space for locating frames of request
	}
}

bool ContFramePool::judge_possible_head_with_enough_range(unsigned int _frame_no, 
														  unsigned int _bitmap_index, 
														  unsigned int _n_frames,
														  unsigned int* _j_move_index)
{
		unsigned int frame_no = _frame_no;
		unsigned int j = 0;  //check available length
		unsigned int tmp_i = _bitmap_index;  //backup i
		unsigned char mask2 = 0xC0;
		mask2 = mask2 >> (((frame_no - base_frame_no) % 4)*2);
		if(frame_no + _n_frames <= base_frame_no + nframes)
		{
			for(j = 0; j < _n_frames; j++)
			{
				if(mask2 == 0){
					mask2 = 0xC0;   //mask wrap around
					tmp_i++;
				}

				if((bitmap[tmp_i] & mask2) != 0)
					break;
				else
					mask2 = mask2 >> 2;
			}
		}
		*_j_move_index = j;
		
		if(j == _n_frames){
			return true;
		}
		else{
			return false;
		}
}

//debug function :　　return_value: 0: available, 1: head, 2:continuous part.
unsigned char ContFramePool::read_info_in_each_frame(unsigned int _frame_no)
{
	unsigned int frame_no = _frame_no;
	unsigned int bitmap_index = ((frame_no - base_frame_no) / 4);
	unsigned char mask = 0xC0;
	unsigned int offset_frame = ((frame_no - base_frame_no) % 4);
	mask = mask >> (offset_frame*2);
	
	unsigned char return_value = (bitmap[bitmap_index] &  mask);
	return_value = return_value >> ((3-offset_frame)*2);
	return  return_value;
	
}


void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
	// Mark all frames in the range as being used.
	unsigned int frame_no = _base_frame_no;
    
	//check range
	assert ((frame_no >= base_frame_no) && (frame_no + _n_frames < base_frame_no + nframes));
    
    unsigned int bitmap_index = (frame_no - base_frame_no) / 4;
    unsigned char mask = 0xC0 >> (((frame_no - base_frame_no) % 4)*2);
	
	for(unsigned int j = 0; j < _n_frames; j++)
	{
		if(mask == 0){
			mask = 0xC0;   //mask wrap around
			bitmap_index++;
		}
		assert((bitmap[bitmap_index] & mask) == 0);   // Is the frame being used already?
	    mask = mask >> 2;
	}
       
	// Update bitmap
	bitmap_index = (frame_no - base_frame_no) / 4;
	unsigned int k = 0;
	unsigned char mask2 = 0x40; //head: 01
	mask2 = mask2 >> (((frame_no - base_frame_no) % 4)*2);
	for(k = 0; k < _n_frames; k++)
	{
		if(mask2 == 0){
			mask2 = 0x80;   //mask wrap around, contiuous:10
			bitmap_index++;
		}
		bitmap[bitmap_index] ^= mask2;
		if(k==0)
			mask2 = mask2 >> 1;  //let 0100 to 0010
		else
			mask2 = mask2 >> 2;  //let 1000 to 0010
	}
    nFreeFrames -= _n_frames;
}

bool ContFramePool::judge_release_pool(unsigned long _base_frame_no)
{
    if (base_frame_no <= _base_frame_no && _base_frame_no < base_frame_no + nframes)
        return true;
    return false;
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
	unsigned int frame_no = _first_frame_no;
	//check range
	ContFramePool* temp_fp;
    if (kernel_mem_pool->judge_release_pool(frame_no))
        temp_fp = kernel_mem_pool;
    else
        temp_fp = process_mem_pool;
	
	temp_fp->release_frames_imp(frame_no);

}

void ContFramePool:: release_frames_imp(unsigned long _first_frame_no)
{
	unsigned int frame_no = _first_frame_no;
	unsigned int bitmap_index = (frame_no - base_frame_no) / 4;
    unsigned char mask = 0xC0 >> (((frame_no - base_frame_no) % 4)*2);
	unsigned char head_mask = 0x40 >> (((frame_no - base_frame_no) % 4)*2);
	assert((bitmap[bitmap_index] & mask) == head_mask);
	
	// Update bitmap
	bitmap_index = (frame_no - base_frame_no) / 4;
	unsigned int k = 0;	
	bitmap[bitmap_index] -= head_mask; //release first frame in these allocated frames
	nFreeFrames++;
	
	mask = mask >> 2;  //prepare release next frame
	if(mask == 0){
		mask = 0xC0;   //mask wrap around
		bitmap_index++;
	}

	while((bitmap[bitmap_index]& mask) == (0xAA & mask))  //0xAA= b'10101010 //judge continous part
	{
        //01 means continue part of allocated frames(need release)
		bitmap[bitmap_index] -= (0xAA & mask);	 //remove contiuous part
		nFreeFrames++;	
		
		mask = mask >> 2;  //let 1000 to 0010
		if(mask == 0){
			mask = 0xC0;   //mask wrap around
			bitmap_index++;
		}
	}

}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
	unsigned long needed_n_frames = 0;
    needed_n_frames = _n_frames / 16384 + (_n_frames % 16384 > 0 ? 1 : 0);
	return needed_n_frames;
}
