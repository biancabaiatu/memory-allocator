# osmem - A Simple Memory Allocator for C Programs

osmem is a small memory allocator library for C programs. It provides a basic
implementation of malloc and free functions, along with additional functions
such as calloc and realloc. The library is designed to be easy to use and to
understand, and it can be easily integrated into existing C codebases.

## Implementation:

osmem is implemented using a simple linked list of memory blocks. Each block
contains a header that stores information about the block, such as its size and
its status (allocated or free). When a memory request is made, the allocator
traverses the linked list to find a block that is large enough to satisfy the
request. If a suitable block is found, it is marked as allocated and returned
to the caller. If no suitable block is found, the allocator requests additional
memory from the operating system using the sbrk or mmap system calls.

### os_malloc(size_t size): 
This function takes a size_t parameter size and allocates a block of memory of 
size size bytes using the sbrk() system call. The function returns a pointer 
to the allocated block if successful, or NULL if the allocation fails.

### calloc(size_t nmemb, size_t size): 
This function takes two size_t parameters nmemb and size, and allocates a block 
of memory for an array of nmemb elements, each of size bytes, using the sbrk() 
system call. The function initializes the allocated memory to zero and returns 
a pointer to the allocated block if successful, or NULL if the allocation fails.

### realloc(void* ptr, size_t size): 
This function takes a void pointer ptr to a previously allocated block of 
memory and a size_t parameter size specifying the new size of the block. 
The function reallocates the memory block to the new size using the sbrk() 
system call, and returns a pointer to the reallocated block if successful, or 
NULL if the allocation fails. If the ptr argument is NULL, the function 
behaves like os_malloc(size).

### free(void* ptr):
This function takes a void pointer ptr to a previously allocated block of 
memory and deallocates the block using the sbrk() system call. The function 
does nothing if the ptr argument is NULL and uses munmap syscall to retrieve 
blocks allocated with mmap.

## Resources

In order to implement these functions I used the following resources:

- ["Implementing malloc" slides by Michael Saelee](https://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf)
- [Malloc Tutorial](https://danluu.com/malloc-tutorial/)

## Copyright - Baiatu Bianca - 04.2023 