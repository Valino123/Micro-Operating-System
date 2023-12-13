#include "mm.h"

#include "vm.h"
#include "stdio.h"


#define set_split(x) ((unsigned int)(x) | 0x80000000) // set highest 1
#define set_unsplit(x) ((unsigned int)(x) & 0x7fffffff) //set highest 0
#define get_size(x) set_unsplit(x)
#define check_split(x) ((unsigned int)(x) & 0x80000000)


uint64_t get_index(uint64_t pa) {

  uint64_t offset = (pa - buddy_system.base_addr) / PAGE_SIZE;
  int block_size = 1;
  while(offset % block_size == 0) {
    block_size <<= 1;
  }
  block_size = block_size >> 1;
  return ((MEMORY_SIZE / PAGE_SIZE) / block_size) + (offset / block_size);
}

uint64_t get_addr(int index) {

  int level = 0, index_copy = index;
  while(index_copy > 1) {
    index_copy >>= 1;
    level++;
  }

  int block_size = MEMORY_SIZE >> level;
  int offset = (index - (1 << level)) * block_size;
  return buddy_system.base_addr + offset;
}

uint32_t get_block_size(int index) {
  int level = 0, index_copy = index;
  while(index_copy > 1) {
    index_copy >>= 1;
    level++;
  }
  return MEMORY_SIZE >> level;
}


uint64_t alloc_page() {
  return alloc_pages(1);
}

int alloced_page_num() {
  // 返回已经分配的物理页面的数量
  // 在buddy system中，我们不再使用该函数
  return -1;
}

void init_buddy_system() {
  // TODO: 初始化buddy system
  // 1. 将buddy system的每个节点初始化为可分配的内存大小，单位为PAGE_SIZE
  // 注意我们用buddy_system.bitmap实现一个满二叉树，其下标变化规律如下：如果当前节点的下标是 X，那么左儿子就是 `X * 2` ，右儿子就是 `X * 2 + 1` ，X 从1开始。
  // 那么，下标为 X 的节点可分配的内存为多少呢？
  int prev_sum;
  int prev_last;
  for(int i=0; i<13; i++){
    if(i == 0){
      prev_sum = 1;
      prev_last = 1;
      buddy_system.bitmap[1] = 4096;
      buddy_system.bitmap[1] = set_unsplit(buddy_system.bitmap[1]);
    }else{
      for(int j=prev_last+1; j <= prev_last + prev_sum*2; j++){
        buddy_system.bitmap[j] = 4096/(prev_sum*2);
        buddy_system.bitmap[j] = set_unsplit(buddy_system.bitmap[j]);
      }
      prev_last += prev_sum*2;
      prev_sum *= 2;
    }
  }
  // 2. 将buddy system的base_addr设置为&_end的物理地址
  buddy_system.base_addr = PHYSICAL_ADDR((uint64_t)&_end);
  // 3. 将buddy system的initialized设置为true
  buddy_system.initialized = 1;

};

uint64_t alloc_buddy(int index, unsigned int num) {
  // TODO: 找到可分配的节点并完成分配，返回分配的物理页面的首地址（通过get_addr函数可以获取节点index的起始地址）
  uint64_t addr = 0;
  // 1. 如果当前节点大于num，则查看当前节点的左右儿子节点
  if(set_unsplit(buddy_system.bitmap[index]) > num){
    addr =  alloc_buddy(index*2, num);
    if(addr == 0){
        addr = alloc_buddy(index*2+1, num);
    }
    if(addr != 0){
        buddy_system.bitmap[index] = 
            set_unsplit(buddy_system.bitmap[index*2]) > set_unsplit(buddy_system.bitmap[index*2+1]) 
            ? set_unsplit(buddy_system.bitmap[index*2]) 
            : set_unsplit(buddy_system.bitmap[index*2+1]);
        buddy_system.bitmap[index] = set_split(buddy_system.bitmap[index]);
    }
  }
  // 提示：通过get_size函数可以获取节点index的可分配连续内存大小
  // 2. 如果当前节点的可分配连续内存等于num，且当前节点没有被拆分，则分配当前节点
  // 提示：通过check_split函数可以获取节点index的状态
  else if(set_unsplit(buddy_system.bitmap[index]) == num && check_split(buddy_system.bitmap[index]) == 0){
    addr =  get_addr(index);
    buddy_system.bitmap[index] = 0;
  }
  // 3. 如果当前节点的可分配连续内存等于num且已经被拆分，则查看当前节点的左右儿子节点
  else if(set_unsplit(buddy_system.bitmap[index]) == num && check_split(buddy_system.bitmap[index]) == 1){
    addr =  alloc_buddy(index*2, num);
    if(addr == 0){
        addr = alloc_buddy(index*2+1, num);
    }
    if(addr != 0){
      // buddy_system.bitmap[index] = 0;
      buddy_system.bitmap[index] = 
            set_unsplit(buddy_system.bitmap[index*2]) > set_unsplit(buddy_system.bitmap[index*2+1]) 
            ? set_unsplit(buddy_system.bitmap[index*2]) 
            : set_unsplit(buddy_system.bitmap[index*2+1]);
      buddy_system.bitmap[index] = set_split(buddy_system.bitmap[index]);
      // buddy_system.bitmap[index] = buddy_system.bitmap[index]-num;
    }
  }
  // 4. 如果当前节点的可分配连续内存小于num，则分配失败返回上层节点
  else{
    addr = (uint64_t)0;
  }
  return addr;
  // 如果完成分配，则要递归更新节点信息，将涉及到的节点的可分配连续内存更新，
  // 已拆分则是左右子节点的可分配的最大连续物理内存最大值，未拆分则是两者之和，并使用set_split更新节点的状态
}

uint64_t alloc_pages(unsigned int num) {
  // 分配num个页面，返回分配到的页面的首地址，如果没有足够的空闲页面，返回0
  if (!buddy_system.initialized) {
    init_buddy_system();
  }
  // TODO:
  // 1. 将num向上对齐到2的幂次
  int left = 1;
  int right = 2;
  while(1){
    if(left <= num && num <= right)
      break;
    left *= 2;
    right *= 2;
  }
  if(num == left)
    num = left;
  else
    num = right;
  // 2. 调用alloc_buddy函数完成分配
  return alloc_buddy(1, num);

}
// non-free: !0 && unsplit

void free_buddy(int index) {
  // TODO: 释放节点index的页面
  // 1. 首先判断节点index的状态，如果已经被拆分，则不能直接释放。异常状态，报错并进入死循环。
  if(check_split(buddy_system.bitmap[index]) == 1){
    printf("error\n");
    while(1);
    return;
  }
  // 2. 如果没有被拆分，则恢复节点index的状态为初始状态
  else{
    buddy_system.bitmap[index] = set_unsplit(get_block_size(index));
  }
  // 3. 如果该节点与其兄弟节点都没有被使用，则合并这两个节点，并递归处理父节点
  // 提示：使用check_split函数可以获取节点index的状态
  // 提示：使用set_unsplit函数可以将节点index的状态恢复为初始状态
  // 提示：使用get_block_size函数可以获取节点index的初始可分配内存大小
  int parent = index/2;
  int leftchild = parent*2;
  int rightchild = parent*2+1;
  while(parent >= 1){
    if(buddy_system.bitmap[leftchild] == get_block_size(leftchild)
      && buddy_system.bitmap[rightchild] == get_block_size(rightchild)){
        buddy_system.bitmap[parent] = set_unsplit(get_block_size(parent));
    }else{
      buddy_system.bitmap[parent] = set_unsplit(buddy_system.bitmap[leftchild]) > set_unsplit(buddy_system.bitmap[rightchild])
                                    ? set_unsplit(buddy_system.bitmap[leftchild])
                                    : set_unsplit(buddy_system.bitmap[rightchild]);
      buddy_system.bitmap[parent] = set_split(buddy_system.bitmap[parent]);
    }
    parent/=2;
    leftchild = parent*2;
    rightchild = parent*2+1;
  }
}

void free_pages(uint64_t pa) {
  // TODO: find the buddy system node according to pa, and free it.
  // 注意，如果该节点的状态为已经被拆分，则应该释放其左子节点
  // 提示：使用get_index函数可以获取pa对应的最上层节点的下标
  int index = get_index(pa);
  if(check_split(buddy_system.bitmap[index]) == 1){
    free_buddy(2*index);
    free_buddy(2*index+1);
  }else{
    if(buddy_system.bitmap[index] != 0){
      return;
    }else{
      free_buddy(index);
    }
  }
}