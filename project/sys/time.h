/*
 * sys/time.h — POSIX 兼容 shim，供 TI NDK 头文件使用
 * TI C6000 编译器工具链不包含此 POSIX 头，在此提供最小实现
 */
#ifndef _SYS_TIME_H_
#define _SYS_TIME_H_

#include <stdint.h>

#ifndef _STRUCT_TIMEVAL
#define _STRUCT_TIMEVAL
struct timeval {
    long tv_sec;    /* 秒 */
    long tv_usec;   /* 微秒 */
};
#endif /* _STRUCT_TIMEVAL */

#ifndef _STRUCT_TIMEZONE
#define _STRUCT_TIMEZONE
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif /* _STRUCT_TIMEZONE */

#endif /* _SYS_TIME_H_ */
