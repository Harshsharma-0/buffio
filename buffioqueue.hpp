#ifndef __BUFFIO_QUEUE_HPP__
#define __BUFFIO_QUEUE_HPP__

#include <unordered_map>


template<typename X, typename Y>
  class waitingmap{
   private:
   std::unordered_map<X,Y> waitingmap;
   public:
   void push(X id , Y thing){waitingmap[id] = thing;};
   Y pop(X id){
     auto res = waitingmap.find(id);
     if(res != waitingmap.end()) return res->second; 
     return {0};
   };
};

// introducing more compressed datatype design
// a^b^b = a; property of xor.

template <typename T> class buffioqueue {
  struct buffiochain{
    T data;
    struct buffiochain *next;
   };

public:
  buffioqueue(){};
  void setdefault(T def){};
  ~buffioqueue() {};

  
  

private:  
 T *data;
};


// xor property:
// a^b^b = a

template<typename X>
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
       tmp->mem = nullptr;
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
    if(mem == nullptr) return nullptr;
    struct __memory *brk = mem;
    mem = mem->mem;
    return brk;
  }; //return a existing frag of return new allocated frag;

 struct __memory *mem;
};

#endif
