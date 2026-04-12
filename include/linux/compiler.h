#ifndef _LINUX_COMPILER_H
#define _LINUX_COMPILER_H

#include <stddef.h>

#ifndef __always_inline
#define __always_inline inline
#endif

#ifndef __inline
#define __inline inline
#endif

#ifndef __inline__
#define __inline__ inline
#endif

#endif /* _LINUX_COMPILER_H */
