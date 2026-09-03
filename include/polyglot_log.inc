! include/polyglot_log.inc - Ergonomic source-location macros for Fortran 2008
! Include this file in Fortran routines compiled with preprocessor (-cpp / /fpp)
#ifndef POLYGLOT_LOG_INC
#define POLYGLOT_LOG_INC

#define F_LOG_TRACE(comp, msg) call f_log_loc(LOG_LVL_TRACE, comp, __FILE__, __LINE__, msg)
#define F_LOG_DEBUG(comp, msg) call f_log_loc(LOG_LVL_DEBUG, comp, __FILE__, __LINE__, msg)
#define F_LOG_INFO(comp, msg)  call f_log_loc(LOG_LVL_INFO,  comp, __FILE__, __LINE__, msg)
#define F_LOG_WARN(comp, msg)  call f_log_loc(LOG_LVL_WARN,  comp, __FILE__, __LINE__, msg)
#define F_LOG_ERROR(comp, msg) call f_log_loc(LOG_LVL_ERROR, comp, __FILE__, __LINE__, msg)
#define F_LOG_FATAL(comp, msg) call f_log_loc(LOG_LVL_FATAL, comp, __FILE__, __LINE__, msg)

#endif
