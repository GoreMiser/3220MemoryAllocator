William Newberry
Memory Allocator
Project #3

This project was to make a shim that would handle pages for malloc, calloc, and realloc calls.
I accomplished this using linked lists to manage things smaller than a page.

KNOWN PROBLEMS:
This runs slow. Like super slow. I have run through it several times trying to optimize the 
overall speed and runtime of this program, but I have met no avail (I made myself segfault 
for hours trying to get this to run faster).

DESIGN:
Outside of the functions we needed to implement for the project(malloc, calloc, realloc, free)
I ended up needing to implement other functions to handle things like the nodes and blocks.
These functions were usually used to help free up pages/nodes, but some were getters to make the actual functions
look much cleaner overall. 

The free list is used for sizes 1024 and smaller, allocating blocks of the size specified(powers of 2 as discussed in class)
If it was over 1024, it got a full page(originally I was going to just leave it giving everyone a page, 
but we had enough time I figured I needed to work harder)

Malloc does as you would expect of malloc: gives you enough memory to hld the amount you requested. 
If you were less than 1024, we'd look for some segments to give you from the free list.
If the memory call exceeded that, you just get a page(as per the write up)

Calloc manages itself by calling our own malloc to set the memory space,
then it sets the memory to 0.

Realloc also uses our malloc to allocate itself some memory before copying the memory over
and freeing the pointer to old memory

Free uses our helper functions(for nodes and pages) to free up the memory that we're passed
