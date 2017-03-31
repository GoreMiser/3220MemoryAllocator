#define _GNU_SOURCE
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <fcntl.h>
 
#define PAGE_SIZE 4096
#define PAGE_NUM 9

//linked list for blocks
typedef struct node_t
{
        void* next;
} node_t;
 
typedef struct header_t
{
        uint64_t len;
        uint64_t element_size;
        uint64_t free;
} header_t;

//to handle sizes smaller than a page
size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, PAGE_SIZE};

//free blocks
node_t *seg_free_lists[PAGE_NUM];

int fd = -1;

//make sure /dev/zero only opens once
void init()
{
        if (fd == -1)
        {
                fd = open("/dev/zero", O_RDWR);
                int i = 0;
                for (i = 0; i < (PAGE_NUM + 1); i++)
                {
                        seg_free_lists[i] = NULL;
                }
        }
}


header_t* getParentPage(node_t* node)
{
        return (header_t*)((uintptr_t)node & ~((uintptr_t)0xfff));
}
 
header_t* getPage(int how_many, int size)
{
        header_t* page = NULL;
        void* memory = NULL;
 
        if ((memory = mmap(NULL, how_many * PAGE_SIZE, PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE, fd, 0)) != MAP_FAILED)
        {
                page = (header_t*) memory;
                page->len = how_many;
                page->element_size = sizes[size];
                page->free = (int)((PAGE_SIZE - sizeof(header_t))/ sizes[size]);
 
                if (size < PAGE_NUM - 1)
                {
                        int x = 0;
                        for (x = 0; x < page->free - 1; x++)
                        {
                                node_t* node = (node_t*)((char*)memory + sizeof(header_t) +
                                        x * sizes[size]);
                                if (node != NULL)
                                {
                                        node->next = ((char*)memory) + sizeof(header_t)
                                                + (x + 1) * sizes[size];
                                }
                        }
                        ((node_t*)(((char*)memory) + sizeof(header_t)))->next = NULL;
                        seg_free_lists[size] = (node_t*)(((char*)memory) +
                                        sizeof(header_t));
                }
                else
                {
                        ((node_t*)(((char*)memory) + sizeof(header_t)))->next = NULL;
 
                        seg_free_lists[size] = (node_t*)(((char*)memory) +
                                sizeof(header_t));
                }
        }
        return page;
}
 
node_t* getNode(int i)
{
        node_t* node = NULL;
	
	//is it coming out of the free list?
        if (i < PAGE_NUM - 1)
        {
                //get a section from free list
                node = seg_free_lists[i];

                //update the freelist
                seg_free_lists[i] = (node_t*)(node->next);
 
                //DON'T MESS UP THE FREE LIST
                node->next = NULL;
 
                header_t* pops = getParentPage(node);
 
                pops->free--;
        }
        else
        {
                //otherwise it gets its own block
                node = seg_free_lists[i];
                seg_free_lists[i] = (node_t*)(node->next);
                node->next = NULL;
        }
 
        return node;
}
 
//help free up pages
void restock_page(header_t* head, int i)
{
        if (i < PAGE_NUM)
        {
                //take the nodes off the free list
                node_t* prev, * curr, *end;
                prev = NULL;
                curr = seg_free_lists[i];
                while(((uintptr_t)head) + sizeof(header_t) != (uintptr_t)curr)
                {
                        prev = curr;
                        curr = (node_t*)(curr->next);
                }
                end = (node_t*)(((char*)curr) + (head->free)-1);
 
                if (prev == NULL)
                {
                        seg_free_lists[i] = end;
                }
                else
                {
                        prev->next = (void*)end;
                }
 
                //Unmap
                munmap(head, PAGE_SIZE);
        }
        else
        {
                munmap(head, PAGE_SIZE * head->len);
        }
}
 
//help to free up a node
void restock_node(node_t* node, int i)
{
        node_t* prev = NULL, * curr = seg_free_lists[i];
 
        while(curr != NULL && (uintptr_t)node > (uintptr_t)curr)
        {
                prev = curr;
                curr = (node_t*)(curr->next);
        }
 
        if ((uintptr_t)node == (uintptr_t)curr) return;
 
        if (prev == NULL)
        {
                seg_free_lists[i] = node;
        }
        else
        {
                prev->next = (void*)node;
        }
 
        node->next = (void*)curr;
 
        header_t* pops = getParentPage(node);
 
        pops->free++;
 
}
 
uint32_t Log2(const uint32_t x)
{
        uint32_t y;
        asm("bsr %1, %0\n"
                : "=r"(y)
                : "r"(x));
 
        return y;
}
 
void* malloc(size_t size)
{
        void* ret = NULL;
 
        if (fd == -1)
        {
                init();
        }
 
        if (size > 0)
        {
                if (size <= 1024)
                {
                        int x = Log2(size - 1) - 2;
 
                        if (x < 0)
                        {
                                x = 0;
                        }
 
                        if (x > PAGE_NUM - 1)
                        {
                                x = PAGE_NUM - 1;
                        }
 
                        if (seg_free_lists[x] == NULL)
                        {
                                int pages = (int)(size / PAGE_SIZE) + 1;
                                header_t* some_pages = getPage(pages, x);
                                
				if (some_pages != NULL)
                                {
                                        node_t* node = getNode(x);
                                        ret = (void*)node;
                                }
                        }
                        else
                        {
                                ret = (void*)getNode(x);
                        }
                }
                else
                {
			//just give it a page at this point
                        ret = (void*)getPage((size / PAGE_SIZE) + 1, Log2(size - 1) - 2);
                }
        }
 
        return ret;
 
}
 
void* calloc(size_t count, size_t size)
{
        if (fd == -1)
                init();
 
        void* ret = NULL;
 
        size_t new_size = count * size;
        
	if (new_size > 0)
        {
                ret = malloc(new_size);
                if (ret != NULL) memset(ret, 0x00, size);
        }
        return ret;
}
 
void* realloc(void* old_ptr, size_t new_size)
{
        if (fd == -1)
                init();
 
        void *ret = old_ptr;
 
        if (old_ptr == NULL) ret = malloc(new_size);
        else
        {
                if (ret != NULL)
                {
                        header_t* pops = (header_t*)((uintptr_t)old_ptr & ~((uintptr_t)0xfff));
                        int bytes = sizes[pops->element_size]*pops->len;
 
                        if (new_size > bytes)
                        {
                                ret = malloc(new_size);
                                if (ret != NULL)
                                {
                                        memcpy(ret, old_ptr, new_size);
                                        free(old_ptr);
                                }
                        }
                        else
                        {
                                ret = old_ptr;
                        }
                }
        }
        return ret;
}
 
void free(void* ptr)
{
        if (fd == -1)
                init();
 
        if (ptr != NULL)
        {
                header_t* pops = (header_t*)((uintptr_t)ptr & ~((uintptr_t)0xfff));
                
		int i = pops->element_size;
                if (i < PAGE_NUM - 1)
                {
                        restock_node((node_t*)ptr, i);
                       
                        if(pops->free == (int)((PAGE_SIZE - sizeof(header_t)) / sizes[i]))
                        {
                                restock_page(pops, i);
 
                        }
                }
        }
}
