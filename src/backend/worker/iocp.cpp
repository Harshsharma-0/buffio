#include "buffio/worker.hpp"

//TODO : add signal works
void buffio::Worker::signalLoop(buffio_fd fd,LoopStatusCode code){};

//TODO : add init code for IOCP
int buffio::Worker::init_poller(unsigned int order) { return 0;};

//TODO : add wait event loop code
int buffio::Worker::wait_event() { return 0;};
