// This provides a portable way of consuming shared libraries,
// staying as close as possible to dlopen semantics.

#ifndef SHARED_LIB_H_
#define SHARED_LIB_H_

#if !defined(_WIN32)
  #include <dlfcn.h>
#endif

#ifdef __APPLE__
#define SHARED_LIB_SUFFIX ".dylib"
#elif defined(_WIN32)
#define SHARED_LIB_SUFFIX ".dll"
#else
#define SHARED_LIB_SUFFIX ".so"
#endif

#ifdef _WIN32
  #include <windows.h>
  typedef HMODULE shared_lib_handle_t;
#else
  #include <ostream>
  typedef void* shared_lib_handle_t;
#endif /* _WIN32 */


shared_lib_handle_t shared_lib_open(const char *filename);
shared_lib_handle_t shared_lib_open(const char *filename, int flags);
// Similar to dlclose and unlike FreeLibrary, this returns 0
// on success.
int shared_lib_close(shared_lib_handle_t handle);
// Returns a string describing the most recent error.
// Uses dlerror on Posix compliant platforms and GetLastError
// on Windows.
char* shared_lib_last_err();
// As opposed to dlerror messages, those buffers will have to be
// deallocated on Windows.
void shared_lib_free_err_msg(char* msg);
void shared_lib_print_last_err(std::ostream *ss);
void* shared_lib_find_symbol(shared_lib_handle_t handle, const char* symbol);

#endif /* SHARED_LIB_H_ */
