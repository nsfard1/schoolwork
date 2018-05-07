#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

#ifdef DEBUG_MALLOC
#define DEBUG_EN 1
#else
#define DEBUG_EN 0
#endif

#define MALLOC_BUFFER 65
#define CALLOC_BUFFER 77
#define FREE_BUFFER 26
#define REALLOC_BUFFER 78
#define ALIGN_SIZE 16
#define DATA_HEADER_SIZE (get_aligned_size(sizeof(Data_Header)))

void *global_head = NULL;

typedef struct Data_Header {
  size_t size;
  struct Data_Header *next;
  struct Data_Header *prev;
  int free;
} Data_Header;

size_t get_aligned_size(size_t size) {
  if (size % ALIGN_SIZE) {
    return size + (ALIGN_SIZE - (size % ALIGN_SIZE));
  }
  else {
    return size;
  }
}

void split_block(Data_Header *old_header, size_t size) {
  Data_Header *new_header = (Data_Header *) (((char *)old_header) +
   DATA_HEADER_SIZE + size);

  new_header->size = old_header->size - size - DATA_HEADER_SIZE;
  old_header->size = size;
  new_header->next = old_header->next;
  new_header->prev = old_header;
  old_header->next = new_header;
  new_header->free = 1;
}

void merge_free_blocks() {
  Data_Header *block = global_head;

  if (block) {
    block = block->next;
  }

  while (block) {
    if (block->prev->free && block->free) {
      block->prev->size += (DATA_HEADER_SIZE + block->size);
      block->prev->next = block->next;
      if (block->next) {
        block->next->prev = block->prev;
      }
    }

    block = block->next;
  }
}

void merge_adjacent_block(Data_Header *header) {
  header->size += header->next->size + DATA_HEADER_SIZE;
  if (header->next->next) {
    header->next->next->prev = header;
  }
  header->next = header->next->next;
}

Data_Header *find_free_block(size_t size) {
  Data_Header *current = global_head;

  while (current && !(current->free && current->size >= size)) {
    current = current->next;
  }

  return current;
}

Data_Header *request_space(size_t size) {
  Data_Header *new_block = (Data_Header *) sbrk(size + DATA_HEADER_SIZE);
  Data_Header *last = global_head;

  if (new_block == (void *) -1) {/* sbrk failed */
    errno = ENOMEM;
    return NULL;
  }

  new_block->size = size;
  new_block->free = 0;
  new_block->next = NULL;

  if (last) { /* not first call */
    while (last->next) {
      last = last->next;
    }

    new_block->prev = last;
    last->next = new_block;
  }
  else { /* first call */
    new_block->prev = NULL;
    global_head = new_block;
  }

  return new_block;
}

void *malloc(size_t size) {
  Data_Header *block = NULL;
  char buffer[MALLOC_BUFFER];

  if (size <= 0) {
    return NULL;
  }

  size = get_aligned_size(size);

  if (!global_head) { /* first call */
    block = request_space(size);

    if (!block) {
      return NULL;
    }

    global_head = block;
  }
  else {
    block = find_free_block(size);

    if (!block) { /* failed to find free block */
      block = request_space(size);

      if (!block) {
        return NULL;
      }
    }
    else {
      block->free = 0;
    }
  }

  if (size < block->size) {
    split_block(block, size);
  }

  if (DEBUG_EN) {
    snprintf(buffer, MALLOC_BUFFER,
     "MALLOC: malloc(%-10d) => (ptr=%-10p, size=%-10d)\n", (int) size,
     ((char *) block) + DATA_HEADER_SIZE, (int) block->size);
    write(STDERR_FILENO, buffer, MALLOC_BUFFER);
  }

  return (block + DATA_HEADER_SIZE);
}

void *calloc(size_t nmemb, size_t size) {
  size_t total = get_aligned_size(nmemb * size);
  char buffer[CALLOC_BUFFER];

  void *ptr = malloc(total);
  memset(ptr, 0, total);

  total = ((Data_Header *)(((char *) ptr) - DATA_HEADER_SIZE))->size;

  if (DEBUG_EN) {
    snprintf(buffer, CALLOC_BUFFER,
     "MALLOC: calloc(%-10d, %-10d) => (ptr=%-10p, size=%-10d)\n",
     (int) nmemb, (int) size, ptr, (int) total);
    write(STDERR_FILENO, buffer, CALLOC_BUFFER);
  }

  return ptr;
}

void free(void *ptr) {
  Data_Header *current = global_head;
  char buffer[FREE_BUFFER];

  if (!ptr) {
    return;
  }

  while (current) {
    /* if ptr == beginning of block */
    if ((char *)ptr == ((char *)current) + DATA_HEADER_SIZE) {
      break;
    }

    /* if ptr is in the middle of a block */
    if ((char *)ptr > (char *)current && (char *)ptr < (char *)current->next) {
      break;
    }

    current = current->next;
  }

  if (!current) {
    return;
  }

  assert(current->free == 0);

  current->free = 1;

  merge_free_blocks();

  if (DEBUG_EN) {
    snprintf(buffer, FREE_BUFFER, "MALLOC: free(%-10p)\n", ptr);
    write(STDERR_FILENO, buffer, FREE_BUFFER);
  }
}

void *realloc(void *ptr, size_t size) {
  void *new_ptr;
  char buffer[REALLOC_BUFFER];

  size = get_aligned_size(size);

  if (!ptr) {
    return malloc(size);
  }

  if (!size) {
    free(ptr);
    return NULL;
  }

  Data_Header *header_ptr = (Data_Header *) (((char *)ptr)-DATA_HEADER_SIZE);
  if (header_ptr->size == size) {
    return ptr;
  }

  if (header_ptr->size > size) {
    split_block(header_ptr, size);
    return ptr;
  }

  if (header_ptr->next && header_ptr->next->free &&
   header_ptr->next->size >= size - header_ptr->size) {
     merge_adjacent_block(header_ptr);
     split_block(header_ptr, size);
     return ptr;
   }

  new_ptr = malloc(size);
  if (!new_ptr) {
    return NULL;
  }

  memcpy(new_ptr, ptr, header_ptr->size);
  free(ptr);

  if (DEBUG_EN) {
    snprintf(buffer, REALLOC_BUFFER,
     "MALLOC: realloc(%-10p, %-10d) => (ptr=%-10p, size=%-10d)\n",
     ptr, (int) size, new_ptr, (int) header_ptr->size);
    write(STDERR_FILENO, buffer, REALLOC_BUFFER);
  }

  return new_ptr;
}
