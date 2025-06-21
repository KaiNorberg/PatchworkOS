#ifndef _LOCALE_H
#define _LOCALE_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/config.h"

struct lconv
{
    char* decimal_point; /* decimal point character                     */     /* LC_NUMERIC */
    char* thousands_sep; /* character for separating groups of digits   */     /* LC_NUMERIC */
    char* grouping; /* string indicating the size of digit groups  */          /* LC_NUMERIC */
    char* mon_decimal_point; /* decimal point for monetary quantities       */ /* LC_MONETARY */
    char* mon_thousands_sep; /* thousands_sep for monetary quantities       */ /* LC_MONETARY */
    char* mon_grouping; /* grouping for monetary quantities            */      /* LC_MONETARY */
    char* positive_sign; /* string indicating nonnegative mty. qty.     */     /* LC_MONETARY */
    char* negative_sign; /* string indicating negative mty. qty.        */     /* LC_MONETARY */
    char* currency_symbol; /* local currency symbol (e.g. '$')            */   /* LC_MONETARY */
    char* int_curr_symbol; /* international currency symbol (e.g. "USD"   */   /* LC_MONETARY */
    char frac_digits; /* fractional digits in local monetary qty.    */        /* LC_MONETARY */
    char p_cs_precedes; /* if currency_symbol precedes positive qty.   */      /* LC_MONETARY */
    char n_cs_precedes; /* if currency_symbol precedes negative qty.   */      /* LC_MONETARY */
    char p_sep_by_space; /* if it is separated by space from pos. qty.  */     /* LC_MONETARY */
    char n_sep_by_space; /* if it is separated by space from neg. qty.  */     /* LC_MONETARY */
    char p_sign_posn; /* positioning of positive_sign for mon. qty.  */        /* LC_MONETARY */
    char n_sign_posn; /* positioning of negative_sign for mon. qty.  */        /* LC_MONETARY */
    char int_frac_digits; /* Same as above, for international format     */    /* LC_MONETARY */
    char int_p_cs_precedes; /* Same as above, for international format     */  /* LC_MONETARY */
    char int_n_cs_precedes; /* Same as above, for international format     */  /* LC_MONETARY */
    char int_p_sep_by_space; /* Same as above, for international format     */ /* LC_MONETARY */
    char int_n_sep_by_space; /* Same as above, for international format     */ /* LC_MONETARY */
    char int_p_sign_posn; /* Same as above, for international format     */    /* LC_MONETARY */
    char int_n_sign_posn; /* Same as above, for international format     */    /* LC_MONETARY */
};

#if defined(__cplusplus)
}
#endif

#endif
