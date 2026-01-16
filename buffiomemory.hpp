#ifndef __BUFFIO_MEMORY_HPP__
#define __BUFFIO_MEMORY_HPP__

/*
* Error codes range reserved for buffioqueue
*  [2500 - 3500]
*  [Note] Errorcodes are negative value in this range.
*  2500 <= errorcode <= 3500
*
*/

template <typename X>
class buffiomemory{

struct __memory{
 struct __memory *mem;
 X data;
};

public:
  buffiomemory(size_t count):mem(nullptr){init(count);}
  buffiomemory():mem(nullptr){}

  ~buffiomemory(){
    if(mem == nullptr) return;
    struct __memory *tmp = mem;
    for(; tmp != nullptr ;tmp = mem){
       mem = mem->mem;
       delete tmp; 
    }
  }

  // can be used to reserve memory
  int init(size_t count){
     if(count <= 0) return -1;
     struct __memory *tmp = nullptr;
      for(size_t i = 0; i < count; i++){
       tmp = new struct __memory;
       pushfrag(tmp);
     }
     return 0;
  };

  int reclaim(X* frag){
    if(frag == nullptr) return -1;
    struct __memory *brk = (struct __memory *)(((uintptr_t)frag) - sizeof(struct __memory*));
    pushfrag(brk);
    return 0;
  }

  X* getfrag(){ 
    struct __memory *tmp = popfrag();
    return &tmp->data;
  }

private:

 void pushfrag(struct __memory *frag){
    frag->mem = nullptr;
    if(mem == nullptr){ 
      mem = frag;
      frag->mem = nullptr;
      return;
    }
    struct __memory *brk = mem;
    mem = frag;
    frag->mem = brk; 
  }; // push the reclaimed frag to the list;

 struct __memory *popfrag(){
    if(mem == nullptr) init(5);
    struct __memory *brk = mem;
    mem = mem->mem;
    return brk;
  }; //return a existing frag of return new allocated frag;

 struct __memory *mem;
};

class buffiopage{
  struct page{
     char *buffer;
     size_t len;
     struct page *next;
  };
 public:
   buffiopage(){}
  ~buffiopage(){}

   buffiopage &operator=(const buffiopage&) = delete;
   buffiopage(const buffiopage &) = delete;
   void pagewrite(char *data,size_t len){} // get page length 
   char *getpage(size_t len){} // getpage lenght
   
private:
 buffiomemory <char>pagesmem;
 struct page *pages;
 struct page *next;
 size_t pagecount;
 size_t pagesize;

};

#endif
