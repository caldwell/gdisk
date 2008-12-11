#ifndef __CAT_H__
#define __CAT_H__

#define CAT2(a,b) a##b
#define CAT(a,b) CAT2(a,b)
#define Unique(symbol) CAT(symbol, __LINE__)

#endif//__CAT_H__
