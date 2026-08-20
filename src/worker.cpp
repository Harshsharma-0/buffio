

int buffio::worker::init(int numWorker,int queueOrder){
  if(io_uring_queue_init((1 << BUFFIO_WORKER_QUEUE_ORDER),&workerInfo.ring,0) != 0)
     return -1;
  
  return 0;
}

int buffio::worker::notify(){

  while(0 < workerInfo.subCount.load(std::memory_order_acquire)){
     
  };

  return 0;
};

void buffio::worker::pushWork(BGLOBOPVEC vec){
 while(!workerInfo.submissionQueue.enqueue(vec)){
   notify();
 };
 workerInfo.subCount.fetch_add(1,std::memory_order_acq_rel);
};

buffio::worker::worker(){};
buffio::worker::~worker(){};

int buffio::worker::consume(){return 0;};

