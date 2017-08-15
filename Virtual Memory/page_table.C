#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;

void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
    kernel_mem_pool  = _kernel_mem_pool;
    process_mem_pool = _process_mem_pool;
    shared_size      = _shared_size;
    Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
	//assign the first PDE and its corespoinding PTEs(total 4MB, one page is 4KB)
    page_directory = (unsigned long *)(process_mem_pool->get_frames(1)* PAGE_SIZE);
	unsigned long * page_table = (unsigned long *)(process_mem_pool->get_frames(1)* PAGE_SIZE);
	unsigned long tmp_addr = 0;
	
	//map 0~4MB to page directly in kernel space, 4kB for each PTE
	for(int i = 0; i < ENTRIES_PER_PAGE; i++){
		page_table[i] = ((unsigned long)tmp_addr) | 3;  //011 (kernel mode + RW + valid)
		tmp_addr += PAGE_SIZE;
	}
	
	page_directory[0] = ((unsigned long)page_table)| 3;  //011 (kernel mode + RW + valid)	
	for(int i = 1; i < (ENTRIES_PER_PAGE -1) ; i++){
		page_directory[i] = 2; //010 (kernel mode + RW + non-valid) and without address of Page Table pages 
		//(total 1024 PDEs enough use for 32bit system, no need to assign new page directories when page fault)
	}	
	page_directory[1023] = ((unsigned long)page_directory)| 3;  //last PDE record the address of this directory
	
	for (int i=0; i < VM_ARRAY_SIZE ; i++)
        vm_pool_array[i] = NULL;    //initial vm_pool_array
	
    Console::puts("Constructed Page Table object\n");
}


void PageTable::load()
{
   current_page_table = this;
   write_cr3((unsigned long) (current_page_table->page_directory));   
   //Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
   paging_enabled = 1;
   write_cr0(read_cr0() | 0x80000000);  
   Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
	//only access miss page fault
	if ((_r->err_code & 1) == 0){
		unsigned long pf_addr = read_cr2(); //pf: page fault, pf_addr: missing page address.[31:0]
		
		VMPool** vm_array = current_page_table->vm_pool_array;
		ContFramePool* current_mem_pool;
		int vm_index = -1;
		for(int i = 0;i < VM_ARRAY_SIZE; i++)
            if(vm_array[i]!= NULL){
                if (vm_array[i]->is_legitimate(pf_addr)){
                    vm_index = i;
					current_mem_pool = current_page_table->vm_pool_array[i]->get_frame_pool();
                    break;
                }
            }
		if (vm_index < 0)
		{
           //Console::puts("[Can't Access this Page Fault] INVALID ADDRESS in VM_POOL\n");
		   return;
		}
		
		unsigned long pf_page_dir_index = pf_addr >> 22; //[31:22]
		unsigned long pf_page_table_index = (pf_addr & (0x03FF<<12)) >> 12; //[21:12]
		
		unsigned long * pf_page_dir = (unsigned long *) 0xFFFFF000; 
		//index1:1023,index2:1023, offset=0 (addr of the page directory)
		unsigned long * pf_page_table = (unsigned long *) (0xFFC00000 | (pf_page_dir_index << 12)); 
		//index1:1023,index2:PDE index, offset=0((addr of the page table))
		unsigned long * tmp_addr = 0;

		if( (pf_page_dir[pf_page_dir_index]& 1) == 0){   
			//if no any entry in this PDE. (bit0=0, non-valid), need to locate mem for this PDE(with 1024 PTEs)
			tmp_addr = (unsigned long *) (current_mem_pool->get_frames(1)* PAGE_SIZE);  //assign mem for this PDE's page table
			pf_page_dir[pf_page_dir_index] = ((unsigned long)tmp_addr)| 3;
			for(int i = 0; i< ENTRIES_PER_PAGE; i++){
				pf_page_table[i] = 2 ;  //init 1024 PTEs for this PDE as 010 (kernel mode + RW + non-valid)
			}
		}
		
		//map memory from process frame pool to miss page
		tmp_addr = (unsigned long *) (current_mem_pool->get_frames(1)* PAGE_SIZE);
		pf_page_table[pf_page_table_index] = ((unsigned long)tmp_addr) | 3 ;  //011 (kernel mode + RW + valid)	
	}
	
	//Console::puts("handled page fault\n");
}

void PageTable::register_pool(VMPool * _vm_pool)
{
	int index = -1;
	for (int i = 0; i < VM_ARRAY_SIZE; i++) //find available position for vmpool
		if (vm_pool_array[i]==NULL)
			index = i;

	if (index >= 0){
		vm_pool_array[index]= _vm_pool;   //register pool
		Console::puts("register pool\n");
	}
	else{
		Console::puts("register VMPool failed\n"); //report error becasue array is full
	}
}

void PageTable::free_page(unsigned long _page_no)
{
	unsigned long pf_page_dir_index = _page_no >> 22; //[31:22]
	unsigned long pf_page_table_index = (_page_no & (0x03FF<<12)) >> 12; //[21:12]
	unsigned long* page_table = (unsigned long*)(0xFFC00000 | (pf_page_dir_index << 12));
	if((page_table[pf_page_table_index]&0x1) == 0x0){
		return; //valid bit = 0 means this page allocate VA, but not allocate real PA.(this page is not have been used)
	}
    unsigned long frame_number= (page_table[pf_page_table_index]&0xFFFFF000)>>12 ;
	ContFramePool::release_frames(frame_number);
	page_table[pf_page_table_index] &=(0xFFFFFFFE);  //clear valid bit[0]
	//Console::puts("free page\n");
}



