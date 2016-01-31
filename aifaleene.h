#ifndef _AIFALEENE_H_
#define _AIFALEENE_H_

typedef unsigned long large_mask;

/*A myraid of types*/
typedef enum _token_type{
  VALUE_TOKEN        = 0x001,
  ID_TOKEN           = 0x002,
  OP_TOKEN           = 0x004,
  PREFIX_OP_TOKEN    = 0x008,
  OPEN_DELIM_TOKEN   = 0x010,
  CLOSE_DELIM_TOKEN  = 0x020,
  OPEN_QUOTE_TOKEN   = 0x040,
  CLOSE_QUOTE_TOKEN  = 0x080,
  ENDL_TOKEN         = 0x100,
  INVALID_TOKEN      = 0x200
}token_type;

typedef enum _value_type{
  UINTEGER_VALUE          = 1,
  INTEGER_VALUE           = 2,
  FLOAT_VALUE             = 3,
  BOOL_VALUE              = 4,
}value_type;
    
typedef enum _op_type{
  ADD_OP=0,
  SUB_OP,
  MUL_OP,
  IDIV_OP,
  DIV_OP,
  MOD_OP,
  POW_OP,
  EQ_OP,
  NOT_EQ_OP,
  GREAT_OP,
  LESS_OP,
  GR_EQ_OP,
  LS_EQ_OP,
  LAND_OP,
  LOR_OP,
  ASSIGN_OP,
  SEP_OP,
  INVALID_OP
}op_type;

typedef enum _prefix_op_type{
  NEG_PREF=0,
  LNOT_PREF,
  INVALID_PREF
}prefix_op_type;

typedef enum _open_delim_type{
  PARA_OPEN,
  SQ_OPEN,
  CURL_OPEN,
  INVALID_OPEN
}open_delim_type;

typedef enum _close_delim_type{
  LM_DEF_CLOSE,
  PARA_CLOSE,
  SQ_CLOSE,
  CURL_CLOSE,
  INVALID_CLOSE
}close_delim_type;

typedef unsigned long uint_t;
typedef long int_t;
typedef double flt_t;

typedef struct _value{
   value_type type;
   union
   {
     uint_t uinteger;
     int_t  integer;
     flt_t  floating;
   };
}value;

#define get_value(val)                              \
  (val.type == UINTEGER_VALUE ? val.uinteger		\
   : val.type == BOOL_VALUE ? val.uinteger          \
   : val.type == INTEGER_VALUE ? val.integer		\
   : val.floating)

#define get_valuep(val)                             \
  (val->type == UINTEGER_VALUE ? val->uinteger		\
   : val->type == BOOL_VALUE ? val->uinteger		\
   : val->type == INTEGER_VALUE ? val->integer		\
   : val->floating)

#define set_value(val, r)                                   \
  (val.type == UINTEGER_VALUE ? (val.uinteger=(uint_t)(r))	\
   : val.type == BOOL_VALUE ? (val.uinteger = 0 || (r))		\
   : val.type == INTEGER_VALUE ? (val.integer=(int_t)(r))	\
   : (val.floating=(flt_t)(r)))

#define set_valuep(val, r)                                      \
  (val->type == UINTEGER_VALUE ? (val->uinteger=(uint_t)(r))	\
   : val->type == BOOL_VALUE ? (val->uinteger = 0 || (r))       \
   : val->type == INTEGER_VALUE ? (val->integer=(int_t)(r))     \
   : (val->floating=(flt_t)(r)))
#define INLINETYPES
#include "buf.h"
#include "chstr.h"
#include "ibuf.h"
#include "list.h"
#include "hash.h"

#define ID_LEN 64
#define LONG_ID_LEN 256
typedef char idname[ID_LEN];
typedef char longidname[LONG_ID_LEN];

typedef struct _token{
  token_type type;
  int real_pos;
  union {
    value v;
    char_buf id;
    op_type op;
    prefix_op_type prefix_op;
    open_delim_type open;
    close_delim_type close;
  };
}token;

static inline
token_ch(token *in, token_type type) {
  if (in->type != type) {
    if (type == ID_TOKEN && char_buf_mk(&in->id))
      return MEMORY_ERROR;
    else if (in->type == ID_TOKEN)
      char_buf_free(&in->id);
  }
  in->type = type;  return 0;}
#define token_clean(in) token_ch(in, INVALID_TOKEN);
#define null_token ((token){.type=INVALID_TOKEN, .real_pos=-1})

#define get_token(tok)                              \
  (tok.type == VALUE_TOKEN        ? (tok.v)         \
   : tok.type == OP_TOKEN         ? (tok.op)		\
   : tok.type == PREFIX_OP_TOKEN  ? (tok.prefix_op)	\
   : tok.type == OPEN_DELIM_TOKEN ? (tok.open)		\
   : tok.type == CLOSE_DELIM_TOKEN? (tok.close)     \
   : (_str(tok.id)))

#define get_tokenp(tok)                               \
  (tok->type == VALUE_TOKEN        ? (tok->v)         \
   : tok->type == OP_TOKEN         ? (tok->op)        \
   : tok->type == PREFIX_OP_TOKEN  ? (tok->prefix_op) \
   : tok->type == OPEN_DELIM_TOKEN ? (tok->open)	  \
   : tok->type == CLOSE_DELIM_TOKEN ? (tok->close)    \
   : (_str(tok->id)))

#define set_token(tok, r)                                   \
  (tok.type == VALUE_TOKEN        ? (tok.v = (r))           \
   : tok.type == OP_TOKEN         ? (tok.op = (r))          \
   : tok.type == PREFIX_OP_TOKEN  ? (tok.prefix_op=(r))     \
   : tok.type == OPEN_DELIM_TOKEN ? (tok.open = (r))		\
   : tok.type == CLOSE_DELIM_TOKEN? (tok.close = (r))		\
   : ( char_buf_cpy(&tok.id, (r)) ))
   
#define set_tokenp(tok, r)                                  \
  (tok->type == VALUE_TOKEN        ? (tok->v = (r))         \
   : tok->type == OP_TOKEN         ? (tok->op = (r))        \
   : tok->type == PREFIX_OP_TOKEN  ? (tok->prefix_op =(r))  \
   : tok->type == OPEN_DELIM_TOKEN ? (tok->open = (r))      \
   : tok.type == CLOSE_DELIM_TOKEN? (tok->close = (r))      \
   : ( char_buf_cpy(&tok->id, (r)) ))

buf_dec(token);
ibuf_dec(token);
#define null_token_ibuf ((token_ibuf){                  \
      .buf=(token_buf){.sz=0.data=0},.magic=-1,.pos=-1})
#define null_stack = null_token_ibuf;

inline void
token_buf_clean(token_buf*);

static inline void
token_ibuf_clean(token_ibuf* in){
  token_buf_clean(&in->buf);
}
static inline
token_ibuf_pushmove(token_ibuf* to,
                    token_ibuf* from,
                    size_t i){
  if (token_geti(*from,i).type == ID_TOKEN);
}

static inline void
token_ibuf_nullify(token_ibuf* in){
  token_ibuf_clean(&in);
  token_ibuf_free(&in);
  *in = null_token_ibuf;
}


/* quotes */
typedef struct _quote{
  char_buf scope;
  char_buf scope;
  token_buf tokens;
} quote;

static inline
quote_mk(quote *in) {
  return (char_buf_mk(&in->scope) ||
          char_buf_mk(&in->params) ||
          token_buf_mk(&in->tokens))?
    MEMORY_ERROR : 0 ;
}
static inline void
quote_free(quote *in) {
  char_buf_free(&in->scope);
  char_buf_free(&in->params);
  token_buf_free_deep(&in->tokens);
}


/* var */
typedef enum {
  VALUE_VAR = 0,
  QUOTE_VAR,
  INVALID_VAR
} var_type;

typedef struct _var {
  var_type type;
  char_buf name;
  union {
    value v;
    quote q;
  };
}var;
static inline
var_mk(var* in) {
  return char_buf_mk(&in->name) ? MEMORY_ERROR;
  : ((in->type = INVALID_VAR), 0);
}
static inline
var_ch(var* in, var_type type) {
  if (type != in->type) {
    if (type == QUOTE_VAR && quote_mk(&in->q)) 
      return MEMORY_ERROR;
    else if (in->type == QUOTE_VAR)
      quote_free(&in->q);
    in->type = type;
  }
  return 0;
}
#define var_clean(in) var_ch(in, INVALID_VAR);

list_dec(var);
hash_dec(var);
#undef INLINETYPES

/* astate */
typedef struct _astate{
  var_hash_table table;
  large_mask flags;
  idname_list *scope;
}astate;

typedef enum {
  VERBOSE_AFLAG=0x001,
  INVALID_AFLAG
}astate_flag;

#define verbose(state) ((state).flags & VERBOSE_AFLAG)
#define verbosep(state) verbose(*state)

/*error codes*/
typedef enum {
  INVALID_ERROR   =  -6,
  NOTFOUND_ERROR,
  DIVBYZERO_ERROR,
  SYNTAX_ERROR,
  MEMORY_ERROR,
  GENERAL_ERROR
}errorcode;

#endif /*_AIFALEENE_H_*/
