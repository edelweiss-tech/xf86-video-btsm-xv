#ifndef SMI_DBG_INC
#define SMI_DBG_INC

#if 1
#undef  SMI_DBG
#else
#define  SMI_DBG
#endif
extern int smi_indent;

#ifdef SMI_DBG
#define VERBLEV 1
#define ENTER() xf86ErrorFVerb(VERBLEV, "%*c %s \n", \
				smi_indent++, '>', __FUNCTION__)

#define LEAVE(...) \
	do{ \
		xf86ErrorFVerb(VERBLEV, "%*c %s \n", \
					--smi_indent, '<', __FUNCTION__); \
		return __VA_ARGS__; \
	}while(0)
#define DEBUG(...) xf86Msg(X_NOTICE,__VA_ARGS__)
#define ERROR(...) xf86Msg(X_ERROR,__VA_ARGS__)
#else
#define VERBLEV	4
#define ENTER()
#define LEAVE(...) return __VA_ARGS__
#define DEBUG(...)
#define ERROR(...) xf86Msg(X_ERROR,__VA_ARGS__)


#define pointer void *
#endif
#endif   /* ----- #ifndef SMI_DBG_INC  ----- */


