/**********************************************************************

  regex.h -

  $Author: akr $

  Copyright (C) 1993-2007 Yukihiro Matsumoto

**********************************************************************/

#ifndef ONIGURUMA_REGEX_H
#define ONIGURUMA_REGEX_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "oniguruma.h"

#ifndef ONIG_RUBY_M17N

ONIG_EXTERN OnigEncoding    OnigEncDefaultCharEncoding;

#define mbclen(p,e,enc)  mrb_enc_mbclen((p),(e),(enc))

#endif /* ifndef ONIG_RUBY_M17N */

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif /* ONIGURUMA_REGEX_H */
