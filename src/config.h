#if defined(MSDOS) || defined(__OS2__) || defined(__NT__) || defined(_WIN32)
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#include "cmake_config.h"
