#ifndef _CRE_H
#define _CRE_H

struct cre_re;
typedef struct cre_re cre_re_t;

cre_re_t * cre_compile(const char * regex);

void cre_free(cre_re_t * re);

void cre_print(cre_re_t * re);

#endif // _CRE_H