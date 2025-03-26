#ifndef UTIL_H
#define UTIL_H


#define LINE_STR_HELPER(x) #x
#define LINE_STR(x) LINE_STR_HELPER(x)

#define PUSH_ERROR(msg) strcpy(__FILE__":"LINE_STR(__LINE__)": error:" msg, error_msg)
#define PANIC(msg) panic("\n"__FILE__":"LINE_STR(__LINE__)": panic: " msg)


#endif // UTIL_H