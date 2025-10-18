#ifndef __BUFFIO_LOG_HPP__
#define __BUFFIO_LOG_HPP__

enum LOG_LEVEL{
  ERROR,
  DEBUG,
  TRACE,
  WARN,
  FATEL,
  INFO,
  LOG
};

#if !defined(BUFFIO_DEBUG)
#define BUFFIO_LOG(part ,...)
#endif


#if defined(BUFFIO_DEBUG)
 
#include <string_view>

namespace Color {
    // Reset
    inline constexpr std::string_view Reset       = "\033[0m";

    // Basic colors
    inline constexpr std::string_view black       = "\033[0;30m";
    inline constexpr std::string_view red         = "\033[1;31m";
    inline constexpr std::string_view green       = "\033[1;32m";
    inline constexpr std::string_view yellow      = "\033[1;33m";
    inline constexpr std::string_view blue        = "\033[1;34m";
    inline constexpr std::string_view magenta     = "\033[1;35m";
    inline constexpr std::string_view cyan        = "\033[1;36m";
    inline constexpr std::string_view white       = "\033[1;37m";
    inline constexpr std::string_view gray        = "\033[0;37m";

    // Bright variants (using 256-color codes for more vivid tone)
    inline constexpr std::string_view brightBlue  = "\033[1;94m";
    inline constexpr std::string_view brightGreen = "\033[1;92m";
    inline constexpr std::string_view brightRed   = "\033[1;91m";
    inline constexpr std::string_view brightCyan  = "\033[1;96m";
    inline constexpr std::string_view brightYellow= "\033[1;93m";
    inline constexpr std::string_view brightMagenta= "\033[1;95m";
    inline constexpr std::string_view orange      = "\033[38;5;208m";
    inline constexpr std::string_view pink        = "\033[38;5;205m";
    inline constexpr std::string_view lightGray   = "\033[38;5;250m";
    inline constexpr std::string_view darkGray    = "\033[38;5;240m";

    // Background colors (optional)
    inline constexpr std::string_view bgRed       = "\033[41m";
    inline constexpr std::string_view bgGreen     = "\033[42m";
    inline constexpr std::string_view bgYellow    = "\033[43m";
    inline constexpr std::string_view bgBlue      = "\033[44m";
    inline constexpr std::string_view bgMagenta   = "\033[45m";
    inline constexpr std::string_view bgCyan      = "\033[46m";
    inline constexpr std::string_view bgWhite     = "\033[47m";
}


 template<typename... Args>
  void log_msg(enum LOG_LEVEL type,Args&&...args){
    std::string_view color;
    std::string_view tag;

    switch(type){   
      case LOG_LEVEL::ERROR: color = Color::brightRed;    tag = "[ERROR]";  break;
      case LOG_LEVEL::DEBUG: color = Color::cyan;         tag = "[DEBUG]";  break;
      case LOG_LEVEL::TRACE: color = Color::orange;       tag = "[TRANCE]"; break;
      case LOG_LEVEL::WARN:  color = Color::brightYellow; tag = "[WARN]";   break;
      case LOG_LEVEL::FATEL: color = Color::red;          tag = "[FATEL]";  break;
      case LOG_LEVEL::INFO:  color = Color::brightGreen;  tag = "[INFO]";   break;
      case LOG_LEVEL::LOG:   color = Color::white;        tag = "[LOG]";    break;
    }
    std::cout<<color<<tag<<Color::lightGray;
    (std::cout<<...<<args)<<Color::Reset<<std::endl;
  }

  #define BUFFIO_LOG(part ,...) log_msg(part,__VA_ARGS__)       

#endif

#endif
