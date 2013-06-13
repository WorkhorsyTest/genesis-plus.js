/*
    error.c --
    Error logging 
*/

#include "osd.h"

static FILE *error_log;

void error_init()
{
#ifdef LOGERROR
  error_log = fopen("error.log","w");
#endif
}

void error_shutdown()
{
#ifdef LOGERROR
  if(error_log) fclose(error_log);
#endif
}

void error(char *format, ...)
{
#ifdef LOGERROR
  if (log_error)
  {
    va_list ap;
    va_start(ap, format);
    if(error_log) vfprintf(error_log, format, ap);
    va_end(ap);
  }
#endif
}
