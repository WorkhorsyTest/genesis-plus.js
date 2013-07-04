/*
    error.c --
    Error logging 
*/

import osd.d;

static FILE* error_log;

void error_init()
{
version(LOGERROR) {
  error_log = fopen("error.log","w");
}
}

void error_shutdown()
{
version(LOGERROR) {
  if(error_log) fclose(error_log);
}
}

void error(char* format, ...)
{
version(LOGERROR) {
  if (log_error)
  {
    va_list ap;
    va_start(ap, format);
    if(error_log) vfprintf(error_log, format, ap);
    va_end(ap);
  }
}
}
