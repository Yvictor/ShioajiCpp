#ifndef __GETOPT_H__
#define __GETOPT_H__

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(SOLCLIENT_AIX_BUILD) && !defined(__VMS) && !defined(SOLCLIENT_HPUX_BUILD)
#ifndef __VMS
extern int opterr;		/* if error message should be printed */
extern int optind;		/* index into parent argv vector */
#endif
extern int optopt;		/* character checked for validity */
extern char *optarg;		/* argument associated with option */
#endif
#ifdef __VMS
extern charPtr __optarg;
#define optarg __optarg
#endif

extern int optreset;		/* reset getopt */

struct option
{
  const char *name;
  int has_arg;
  int *flag;
  int val;
};

#define no_argument       0
#define required_argument 1
#define optional_argument 2

#if !defined(__VMS) && !defined(SOLCLIENT_AIX_BUILD) && !defined(SOLCLIENT_ZTPF_BUILD) && !defined(SOLCLIENT_HPUX_BUILD)
int getopt(int, char**, char*);
#endif
int getopt_long(int, char**, char*, struct option*, int*);

#ifdef __cplusplus
}
#endif

#endif /* __GETOPT_H__ */
