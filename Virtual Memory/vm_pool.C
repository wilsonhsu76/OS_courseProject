/*
 File: vm_pool.C
 
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

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"
#include "page_table.H"

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
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) 
{
	base_address = (_base_address & 0xFFFFF000);  //pool head should align 4kB page
    size = _size/(PageTable::PAGE_SIZE);  //page size
    frame_pool = _frame_pool;
    page_table = _page_table;
    region_count = 0;
    maximum_region_count = (PageTable::PAGE_SIZE)/sizeof(allocated_region_node);  //upper bound for nodes count(512) in one page
	
	//get frame for saving nodes info
	allocated_nodes_list =  (allocated_region_node*)(_base_address);
    page_table->register_pool(this); //Let page table register this pool
    
	Console::puts("VMPool constructed\n");
}

unsigned long VMPool::allocate(unsigned long _size) 
{
	
	int required_pages = _size/(PageTable::PAGE_SIZE) + (_size % (PageTable::PAGE_SIZE) > 0 ? 1 : 0);
	int allocated_region_index = -1;  //allocate the region with this regions_index
	bool b_addMoreRegions = false;
	
	if((region_count + 1) > maximum_region_count){
        Console::puts("Can't add more regions info in the first page of one pool\n");
    }
	else{
		b_addMoreRegions = true;
	}
	
	//search is there any hole in these regions can beused?
	if(region_count!=0){
		for(int i =0; i < region_count; i++){
			int residual_pages = 0;
			if( allocated_nodes_list[i].size == 0 ){  //means it is not in used in the middle of nodes(not the end)
				if ((i+1)<= region_count)
					residual_pages = (allocated_nodes_list[i+1].base_addr - allocated_nodes_list[i].base_addr)/(PageTable::PAGE_SIZE);
				else
					residual_pages = size - (allocated_nodes_list[i].base_addr)/(PageTable::PAGE_SIZE);
			}
			if(residual_pages >= required_pages){
				allocated_region_index = i;
				break;
			}
		}
	}

	unsigned long start_addr = 0;        //allocated region's start_address (unit:Byte)
	if((allocated_region_index == -1) && (b_addMoreRegions==true)){    //need to allocate new region nodes
		if(region_count==0) //first nodes
			start_addr = base_address + (PageTable::PAGE_SIZE);  // first page in pool using for record list
		else  //follow the previous node
			start_addr = allocated_nodes_list[region_count-1].base_addr + (allocated_nodes_list[region_count-1].size)*(PageTable::PAGE_SIZE);
		
		//check for total size of usage
		unsigned long end_addr = (start_addr>>12) + required_pages;  //unit: pages
		unsigned long end_pool_addr = (base_address>>12) + size; //unit: pages
		if(end_addr > end_pool_addr){
			Console::puts("Can't allocate new space in this pool\n");
			return 0;
		}
		allocated_nodes_list[region_count].base_addr = start_addr;  //add one new node
		allocated_nodes_list[region_count].size = required_pages;	
		region_count++;	
	}
	else if((allocated_region_index != -1)){  //there is avaiable space in the middle nodes, use it.
		start_addr = allocated_nodes_list[allocated_region_index].base_addr;
		allocated_nodes_list[allocated_region_index].size = required_pages;
		
		if(b_addMoreRegions==true){  //if progrma can add more nodes, it can use this space more efficiently.
			unsigned long tmp_size = allocated_nodes_list[allocated_region_index+1].base_addr - (start_addr + required_pages*(PageTable::PAGE_SIZE));
			if( tmp_size >= (PageTable::PAGE_SIZE)){   //there is still avaiable space in the middle nodes, add a new node
				for(int i = region_count; i > (allocated_region_index + 1); i--){
					allocated_nodes_list[i].base_addr = allocated_nodes_list[i-1].base_addr;
					allocated_nodes_list[i].size = allocated_nodes_list[i-1].size;
				}
				allocated_nodes_list[allocated_region_index + 1].base_addr =  start_addr + required_pages*(PageTable::PAGE_SIZE);
				allocated_nodes_list[allocated_region_index + 1].size = 0;
				region_count++;
				
				//However, here should check merge again:
				//check next node of allocated_nodes_list[allocated_region_index + 1]:
				bool b_nextNodeSize0 = false;
				if(allocated_nodes_list[allocated_region_index + 2].size == 0)
					b_nextNodeSize0 = true;
				//merge allocated_nodes_list[allocated_region_index + 1] and allocated_nodes_list[allocated_region_index + 2]
				int index = (allocated_region_index + 1);
				if (b_nextNodeSize0 == true){
					for(int k = (index + 2); k < region_count; k++){
						allocated_nodes_list[k - 1].base_addr = allocated_nodes_list[k].base_addr;
						allocated_nodes_list[k - 1].size = allocated_nodes_list[k].size;
					}
					allocated_nodes_list[region_count - 1].base_addr = 0;
					allocated_nodes_list[region_count - 1].base_addr = 0;
					region_count = region_count - 1;	//merge 2 nodes into 1 node	
				}	
			}
		}
	}
	else{   //there is no middle avaiable space, and no space to locate new nodes...
		Console::puts("Can't allocate new space in this pool\n");
		return 0;
	}
	return start_addr;
}

void VMPool::release(unsigned long _start_address) 
{
	//release which region
	int index = -1;  //region with this index in list will be removed
	for(int i = 0; i < region_count; i++){
		if(allocated_nodes_list[i].base_addr == _start_address){
			index=i;
			break;
		}
	}
	
	if(index < 0){
		Console::puts("release illegal address\n");
		return;
	}

	//release pages in this region	
	for (int j = 0; j < allocated_nodes_list[index].size; j++)
	{
		page_table->free_page(_start_address);
		_start_address = _start_address + PageTable::PAGE_SIZE;
		page_table->load(); // flush the TLB after release one page
	}
	allocated_nodes_list[index].size = 0; //release this node.
	
	//reconstruct new allocated_nodes_list. Merge size = 0's nodes
	bool b_nextNodeSize0 = false;
	bool b_preNodeSize0 = false;
	//pre node:
	if((index - 1) >= 0){
		if(allocated_nodes_list[index - 1].size == 0)
		b_preNodeSize0 = true;	
	}
	//next node:
	if((index + 1) < region_count){
		if(allocated_nodes_list[index + 1].size == 0)
		b_nextNodeSize0 = true;	
	}
	
	if(b_preNodeSize0 && b_nextNodeSize0){
		for(int k = (index + 2); k < region_count; k++){
			allocated_nodes_list[k - 2].base_addr = allocated_nodes_list[k].base_addr;
			allocated_nodes_list[k - 2].size = allocated_nodes_list[k].size;
		}
		allocated_nodes_list[region_count - 1].base_addr = 0;
		allocated_nodes_list[region_count - 1].base_addr = 0;
		allocated_nodes_list[region_count - 2].base_addr = 0;
		allocated_nodes_list[region_count - 2].base_addr = 0;
		region_count = region_count - 2;  //merge 3 nodes into 1 node
	}
	else if (b_preNodeSize0 && (!b_nextNodeSize0)){
		for(int k = (index + 1); k < region_count; k++){
			allocated_nodes_list[k - 1].base_addr = allocated_nodes_list[k].base_addr;
			allocated_nodes_list[k - 1].size = allocated_nodes_list[k].size;
		}
		allocated_nodes_list[region_count - 1].base_addr = 0;
		allocated_nodes_list[region_count - 1].base_addr = 0;
		region_count = region_count - 1;	//merge 2 nodes into 1 node
	}
	else if ((!b_preNodeSize0) && b_nextNodeSize0){
		for(int k = (index + 2); k < region_count; k++){
			allocated_nodes_list[k - 1].base_addr = allocated_nodes_list[k].base_addr;
			allocated_nodes_list[k - 1].size = allocated_nodes_list[k].size;
		}
		allocated_nodes_list[region_count - 1].base_addr = 0;
		allocated_nodes_list[region_count - 1].base_addr = 0;
		region_count = region_count - 1;	//merge 2 nodes into 1 node	
	}
	
    //Console::puts("Released region of memory.\n");
}

bool VMPool::is_legitimate(unsigned long _address) 
{
     if((base_address <= _address) && (_address <= (base_address + size)))
     {
          return true;
     }
    return false;
    
}

ContFramePool* VMPool::get_frame_pool()
{
	return frame_pool;
}

