// Bento includes
#include <bento_memory/system_allocator.h>
#include <bento_memory/common.h>

// Internal includes
#include "allocator_c_api.h"

GSAllocator gs_create_allocator()
{
	return (GSAllocator)bento::make_new<bento::SystemAllocator>(*bento::common_allocator());
}

void gs_destroy_allocator(GSAllocator allocator)
{
	bento::SystemAllocator* sys_alloc = (bento::SystemAllocator*)allocator;
	bento::make_delete<bento::SystemAllocator>(*bento::common_allocator(), sys_alloc);
}
