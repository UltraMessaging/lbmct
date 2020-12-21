/* err.h - Error handling infrastructure.
 *
 * This code and its documentation is Copyright 2019, 2019 Steven Ford,
 * http://geeky-boy.com and licensed "public domain" style under Creative
 * Commons "CC0": http://creativecommons.org/publicdomain/zero/1.0/
 * To the extent possible under law, the contributors to this project have
 * waived all copyright and related or neighboring rights to this work.
 * In other words, you can use this code for any purpose without any
 * restrictions. This work is published from: United States. The project home
 * is https://github.com/fordsfords/err
 */

#ifndef ERR_H
#define ERR_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* For a Windows build of err.c, set the preprocessor symbol ERR_EXPORTS
 * if you want this subsystem to be available from a DLL.
 */
#if defined(_WIN32)
#  ifdef ERR_EXPORTS
#    define ERR_API __declspec(dllexport)
#  else
#    define ERR_API __declspec(dllimport)
#  endif
#else
#  define ERR_API
#endif

#define ERR_OK NULL

/* Simple macro to skip past the dir name of a full path (if any). */
#if defined(_WIN32)
#  define ERR_BASENAME(_p)\
    ((strrchr(_p, '\\') == NULL) ? (_p) : (strrchr(_p, '\\')+1))
#else
#  define ERR_BASENAME(_p)\
    ((strrchr(_p, '/') == NULL) ? (_p) : (strrchr(_p, '/')+1))
#endif

/* Applications that return an err_t should be declared with this macro. */
#define ERR_F __attribute__ ((__warn_unused_result__)) err_t *

/* Assertion with abort macro (prints programmer-friendly dump to _stream). */
#define ERR_ASSRT(_cond_expr, _err, _stream) do { if (!(_cond_expr)) { fprintf(_stream, "ERR_ASSRT failed at %s:%d (%s)\n", ERR_BASENAME(__FILE__), __LINE__, #_cond_expr); err_print(_err, _stream); fflush(_stream); abort(); } } while (0)

/* Shortcut assertion with abort macro: asserts no error. */
#define ERR_A(_err) ERR_ASSRT(_err == ERR_OK, _err, stderr)

#define ERR_THROW(_code, _mesg) do { return err_throw(__FILE__, __LINE__, _code, _mesg); } while (0)

#define ERR(_funct_call) do { err_t *_err; _err = (_funct_call); if (_err) return err_rethrow(__FILE__, __LINE__, _err, _err->code, NULL); } while (0)


struct err_s;  /* Forward def to allow self-reference. */

typedef struct err_s {
  int code;
  char *file;
  int line;
  char *mesg;  /* Separately malloced. */
  struct err_s *backtrace;  /* Linked list. */
} err_t;


ERR_API err_t *err_throw(char *file, int line, int code, char *msg);
ERR_API err_t *err_rethrow(char *file, int line, err_t *in_err, int code, char *msg);
ERR_API void err_print(err_t *err, FILE *stream);
ERR_API void err_dispose(err_t *err);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* ERR_H */
