#ifndef __CSPRINTF_H__
#define __CSPRINTF_H__

// Returns a constant string. *Don't* save the result, it's meant for inline use only.
// Output is limited to 1000 characters. You can use up to 5 calls in an expression.
// Yes, it's hacky and limited, not thread-safe, yadda yadda yadda.
char *csprintf(char *format, ...) __attribute__ ((format (printf, 1, 2)));

#endif//__CSPRINTF_H__
