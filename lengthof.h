//-------=====================<<<< COPYRIGHT >>>>========================-------
//   Copyright (c) 2004-2005 David Caldwell,  No Rights Reserved. Feel Free.
//-------================================================================-------

#ifndef __LENGTHOF_H__
#define __LENGTHOF_H__

#define __compile_assert(e) sizeof(char[1-2*!(e)]) // expression version of compile_assert()
#define lengthof(x) (sizeof(x)/sizeof(*(x)) + \
                     0*__compile_assert(!__builtin_types_compatible_p(typeof(x),typeof(&(x)[0]))))

#endif /* __LENGTHOF_H__ */
