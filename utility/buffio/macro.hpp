#ifndef BUFFIO_UTILITY_MACRO
#define BUFFIO_UTILITY_MACRO

#define BUFFIO_CLASS_PROTECT(name_instance)  \
 name_instance(name_instance const &) = delete; \
 name_instance(name_instance const &&) = delete; \
 name_instance &operator=(name_instance const &) = delete; \
 name_instance &operator=(name_instance const &&) = delete;

#define BUFFIO_STRINGFY_IMPL(...) #__VA_ARGS__
#define BUFFIO_ARGS_STRINGFY(...)  BUFFIO_STRINGFY_IMPL(__VA_ARGS__)

#endif
