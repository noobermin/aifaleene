#include "aifaleene.h"
#include "misc.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>

#ifdef __STDC_NO_VLA__
#error "Compile me on a platform/compiler that has VLAs!"
#endif /*__STDC_NO_VLA__*/

/*****************************************************
 * low level exception handling and memory management
 */
/* Exception handling.
 * 
 * These types currently only have context in this file.
 */
typedef struct _mem_guard
{
  int cur;
  large_mask alloc;
  /* These are done purposefully so that
   * this can be passed by value to a calling function
   */
  int *lineno;
  char *errstr;
  int *ret;
}mem_guard;


/*freer*/
#define _f_g(_g,call,n)	{ (call); _g.alloc&= ~(1<<n);}
#define _f(call,n) _f_g(_g,call,n)

/*exception jumper*/
#define gotoerr_gl(_g,LABEL,pos, err, vl...)        \
  { sprintf(_g.errstr, vl);                         \
    *_g.lineno=pos;                                 \
    *_g.ret=err;                                    \
    goto LABEL; }                          
#define gotoerr_g(_g, pos, err, vl...) gotoerr_gl(_g,end,pos,err,vl)
#define gotoerr(pos, err, vl...) gotoerr_g(_g,pos,err,vl)
/*allocation exception wrapper*/
#define _a_gls(_g,LABEL,errstr,call) {          \
    if (call)                                   \
      gotoerr_gl(_g,LABEL, 0,                   \
                 MEMORY_ERROR, errstr);         \
    _g.alloc|=1<<(_g.cur++);                    \  
  }
#define _a_gl(_g,LABEL,call)                    \
  _a_gls(_g,LABEL, "Allocation Error",call)
#define _a_g(_g,call) _a_gl(_g,end,call)
#define _a(call) _a_g(_g,call)

const char *error_names[] = {
  "General Error",
  "Memory Error",
  "Syntax Error",
  "Division By Zero Error",
  "(Not) Found Error",
  "Invalid Error...?"
};

#define print_err(code)				\
  printf(">>> %s:\n",error_names[-(code)-1])

static
line_error(const char *line, int pos,
           const char* fmt,...) {
  va_list vl;
  va_start(vl,fmt);
  vprintf(fmt,vl);
  va_end(vl);
  printf("\n%s\n",line);
  if (pos < 0) return 0;
  
  for(int i=0; i<pos; ++i) putchar(' ');
  puts("^");
  return 0;
}

/*****************************************************
 * syntax
 */

const char *pref_op_strs[] = {"-","!",""};

const char *op_strs[] = {
/*              v--deliberate hack to keep this working with my scanner*/
  "+","-","*","//","/","%","^",
  "==","!=",">","<",">=","<=",
  "&&","||",
  "=",",",""
};

const char *open_quote_str = "{";
const char *close_quote_str = "}";

const int op_prec[] = {
/*+  -  *  // /  %  ^  == !=  >  <  >=  <=*/
  3, 3, 4, 5, 5, 5, 5, 2, 2,  2, 2, 2,  2,
/*&& || */
  1, 1,
/* =   ,  invalid*/
  -1, -2, 0x10000000 };

static
precedence(token tok) {
  return tok.type == OP_TOKEN ? op_prec[tok.op]
    : tok.type == OPEN_DELIM_TOKEN ? -2
    : tok.type == PREFIX_OP_TOKEN  ? 100
    : 100000;}

const char *open_strs[] = {"(","["};

const char *close_strs[] = {")=","]",")"};

const char *token_strs[] = {
  "value",
  "identifier",
  "operator",
  "prefix operator",
  "opening parentheical",
  "closing parentheical",
  "open quote",
  "close quote",
  "end of line",
  "invalid" };

static
isopendelim(int in)
{ return in == '(' || in == ']'; }

static
isclosedelim(int in)
{ return in == ')' || in == ']'; }

static
isop(int in) { 
  return in == '+' || in == '-' || in == '/'
    || in == '*'|| in == '%' || in == '^'
    || in == '=' || in == '>'|| in == '<'  
    || in == '!' || in == '&' || in == '|';
}

static
ispref(int in) { return in=='-' || in=='!';}

/*static
isident(int in)
{ return isalphanum(in) || in == '_'; }*/

static
isendline(int in) { return in == '\0'; }

static token_type
identify_token(char in, large_mask expected) {
  if (isalpha(in) || in=='_')
    return ID_TOKEN;
  else if (isdigit(in))
    return VALUE_TOKEN;
  else if (isop(in))
    return !(expected & OP_TOKEN) && ispref(in) ? PREFIX_OP_TOKEN : OP_TOKEN;
  else if (isopendelim(in))
    return OPEN_DELIM_TOKEN;
  else if (isclosedelim(in))
    return CLOSE_DELIM_TOKEN;
  else if (isendline(in))
    return ENDL_TOKEN;
  else if (in == open_quote_str[0])
    return OPEN_QUOTE_TOKEN;
  else if (in == close_quote_str[1])
    return CLOSE_QUOTE_TOKEN;
  else
    return INVALID_TOKEN;
}
/*****************************************************
 * output helpers
 */

print_value(value v) {
  char *fmt;
  switch(v.type) {
  case UINTEGER_VALUE:
  case INTEGER_VALUE:
    fmt = "%i";
    break;
  case FLOAT_VALUE:
    fmt = fabs(v.floating) > 1.0e10 ? "%0.4e" : "%0.4f";
    break;
  case BOOL_VALUE:
    fmt = v.uinteger ? "true" : "false";
    break;
  }  
  return printf(fmt, get_value(v));
}

dump_stack_opt(const token_ibuf* in, const char* sep, int normal) {
  int i = (in->pos)+1;
  while( --i >= 0) {
    token cur = ibuf_geti(*in, !normal ? i : in->pos-i );
    switch(cur.type) {
    case VALUE_TOKEN:
      print_value(cur.v);
      break;
    case ID_TOKEN:
      printf("%s",cur.id);
      break;
    case OP_TOKEN:
      printf("%s",op_strs[cur.op]);
      break;
    case PREFIX_OP_TOKEN:
      printf("%s",pref_op_strs[cur.prefix_op]);
      break;
    case OPEN_DELIM_TOKEN:
      printf("(");
      break;
    case CLOSE_DELIM_TOKEN:
      printf(")");
      break;

    default: break;
    }
    fputs(sep,stdout);
  }
  return puts("");
}
#define dump_stack(st,sep) dump_stack_opt(st,sep,-1)

/*****************************************************
 * evaluation
 */

prefix_operate(value *r, prefix_op_type pre) {
  switch(pre)
    {
    case NEG_PREF:
      set_valuep(r, -get_valuep(r));
      break;
    case LNOT_PREF:
      set_valuep(r, !get_valuep(r));
      r->type=BOOL_VALUE;
      break;
    default:
      return -1;
    }
  return 0;
}

#define zero(v)						\
  (v.type == UINTEGER_VALUE || v.type == INTEGER_VALUE  ? 0 : 0.0)

operate(value l, value r, op_type op, value *ret) {
  /*handle promotion*/
  ret->type = max(l.type,r.type);
  switch(op) {
  case ADD_OP:
    set_valuep(ret, get_value(l) + get_value(r));
    break;
  case SUB_OP:
    set_valuep(ret, get_value(l) - get_value(r));
    break;
  case MUL_OP:
    set_valuep(ret, get_value(l) * get_value(r));
    break;
  case POW_OP:
    set_valuep(ret, pow(get_value(l), get_value(r)));
    break;
  case IDIV_OP:
    /*casting first*/
    l.uinteger = (unsigned long) get_value(l);
    r.uinteger = (unsigned long) get_value(r);
    ret->type = l.type = r.type = UINTEGER_VALUE; /*intentional fall through*/
  case DIV_OP:
    if (get_value(r) == zero(r))
      return DIVBYZERO_ERROR;
    set_valuep(ret, get_value(l) / get_value(r));
    break;
  case MOD_OP:
    l.uinteger = (unsigned long) get_value(l);
    r.uinteger = (unsigned long) get_value(r);
    ret->type = l.type = r.type = UINTEGER_VALUE;
    if (get_value(r) == zero(r))
      return DIVBYZERO_ERROR;
    set_valuep(ret, l.uinteger % r.uinteger);
    break;
  case EQ_OP:
    ret->type = BOOL_VALUE;
    set_valuep(ret, get_value(l) == get_value(r));
    break;
  case NOT_EQ_OP:
    ret->type = BOOL_VALUE;
    set_valuep(ret, get_value(l) != get_value(r));
    break;
  case GREAT_OP:
    ret->type = BOOL_VALUE;
    set_valuep(ret, get_value(l) > get_value(r));
    break;
  case LESS_OP:
    ret->type = BOOL_VALUE;
    set_valuep(ret, get_value(l) < get_value(r));
    break;
  case GR_EQ_OP:
    ret->type = BOOL_VALUE;
    set_valuep(ret, get_value(l) >= get_value(r));
    break;
  case LS_EQ_OP:
    ret->type = BOOL_VALUE;
    set_valuep(ret, get_value(l) <= get_value(r));
    break;
  case LAND_OP:
    ret->type = BOOL_VALUE;
    set_valuep(ret, get_value(l) && get_value(r));
    break;
  case LOR_OP:
    ret->type = BOOL_VALUE;
    set_valuep(ret, get_value(l) || get_value(r));
    break;
  default: break;
  }
  return 0;
}
#undef zero



value_from_id(const char *id,
	      var_hash_table table,
	      token *ret) {
  var* idp;
  int st;
  if((st = var_get(id,table,&idp))) return st;
  if(idp->type != VALUE_ID)
    return NOTFOUND_ERROR;
  ret->v    = idp->v;
  ret->type = VALUE_TOKEN;
  return 0;
}

/*used in evaluate and aifaleene.*/
#define tok_to_val(tok)							\
    if (tok.type == ID_TOKEN						\
	&& value_from_id(tok.id, state->table, &tok) == NOTFOUND_ERROR)	\
      {gotoerr(tok.real_pos, NOTFOUND_ERROR,"token \"%s\" not found.",tok.id);}

evaluate(astate* state, const token_ibuf* expr, int len,
	 token_ibuf* aux_stack,
	 mem_guard _g) {

  for(int i=0; i<=len; ++i) {
    token cur = ibuf_geti(*expr,i);
    if (cur.type == VALUE_TOKEN || cur.type == ID_TOKEN) {
	  token_ibuf_push(aux_stack,cur);
	}  else if (cur.type == OP_TOKEN) {  
	  token l,r; int st;
	  value v;
	  if (token_ibuf_popv(aux_stack,2,&r,&l))
	    gotoerr(cur.real_pos, SYNTAX_ERROR, "incorrect number of arguments.");
	  if (cur.op == ASSIGN_OP) {
        var id;
        if (l.type != ID_TOKEN)
          gotoerr(l.real_pos, SYNTAX_ERROR, "cannot assign to non-var");
        tok_to_val(r);
        id.v = v = r.v; id.type = VALUE_ID; strncpy(id.name, l.id, ID_LEN);
        if ((st=var_setdefault(&id, state->table)))
          gotoerr(cur.real_pos, st, "failed in assignment.");
      }
	  else { /*normal operator*/
        tok_to_val(l);
        tok_to_val(r);
        if ((st=operate(l.v,r.v,cur.op,&v)))
          gotoerr(cur.real_pos, st, "failed in operation %s", op_strs[cur.op]);
      }
	  token_ibuf_push(aux_stack,
                      (token){.type = VALUE_TOKEN,
                          .real_pos = cur.real_pos,
                          .v        = v});
	} else if (cur.type == PREFIX_OP_TOKEN) {
	  /*int st =*/prefix_operate(&(ibuf_get(*aux_stack).v),cur.prefix_op);
	  /*should be no errors right now.*/
	}
  } /*for each out element*/
 end:
  return (*_g.ret);
}
/* for parsers */
#define _mtok(tok) {                         \
    if (call)                                \
      goto_err(tok.n, MEMORY_ERROR,          \
               "Memory Error in Parsing");
#define push_stackp(p, stack)
  _mtok(token_ibuf_push(stack,p))
#define push_stack(p, stack)
  _mtok(token_ibuf_push(&stack,p))

/*parsing*/
tokenize(char *line, token_ibuf* out, mem_guard _g){
#define push_out(p) push_stack(p, out)
  const large_mask normal_exp =
    VALUE_TOKEN |
    ID_TOKEN |
    OPEN_DELIM_TOKEN |
    PREFIX_OP_TOKEN |
    ENDL_TOKEN; 
  large_mask expected = normal_exp;
  int pdepth=0, qdepth=0, n=0, i;
  enum {
    close_delim_unexpected,
    val_then_delim,
    close_delim_expected
  }close_state = close_delim_unexpected;
  for(;;){
    token_type type;
    token tok = null_token;
    /*skip whitespace*/
    n+=to_1st(line+n);
    tok.real_pos=n;
    type = identify_token(*(line+n), expected);
    /*sorry this has to be here,
     *basically, if there is a missing open quote,
     *we just insert one and expect the end of line to
     *end all quotes*/
    if (!(expected & type) &&
        (expected & OPEN_QUOTE_TOKEN)) {
      push_token_stack((token){
          .type=OPEN_QUOTE_TOKEN,.real_pos=n});
      ++qdepth;
      expected &= OPEN_QUOTE_TOKEN;
      expected |= CLOSE_QUOTE_TOKEN;
      continue;
    }
    if (!(expected & type)) {
      gotoerr(n, SYNTAX_ERROR,
              "unexpected %s token.",token_strs[binplace(type)]);
    }
    _a(token_ch(&tok, type));
    /* begin validating tokens */
    if (type == VALUE_TOKEN) {
#define FLT 37 /*see strol you */
      char base=FLT;
      char *end;
      if ( (*(line+n)) == '0') {
        char next = *(line+n+1);
        if (next == 'b') base=2, n+=2;
        else if (next == 'x') base=0x10, n+=2;
        else if (next == 'd') base=10, n+=2;
        else if (next>='1'&& next<='9') base=010, ++n;
      }
      if (base==FLT) {
        tok.v.type = FLOAT_VALUE;
        tok.v.floating = strtod(line+n, &end);
      } else if (base==10) {
        tok.v.type = INTEGER_VALUE;
        tok.v.integer = strtol(line+n, &end,10);
      } else {
        tok.v.type = UINTEGER_VALUE;
        tok.v.uinteger = strtoul(line+n, &end,base);
      }
#undef FLT
      n=end-line;
      expected |= ~(VALUE_TOKEN | INVALID_TOKEN | CLOSE_DELIM_TOKEN);
      expected &= ~(VALUE_TOKEN);
      if (close_state == val_then_delim)
        close_state = close_delim_expected;
      if (close_state == close_delim_expected)
        expected |= CLOSE_DELIM_TOKEN;
    } else if (type == ID_TOKEN) {
      /*peak at the top, add a multiplication
       *if the last token was a value.*/
      if (!ibuf_empty(tok_stack)
          && ibuf_get(tok_stack).type == VALUE_TOKEN)
        push_tok_stack((token){
            .type=OP_TOKEN, .real_pos=n, .op = MUL_OP});
      /*for now, no arguments*/
      /*first argument is good*/
      int m = n+1, cplen=0;
      for(;m < len;++m)
        if (!(isalpha(line[m]) || isdigit(line[m])|| line[m]=='_'))
          break;
      cplen = m-n;
      _mtok(char_buf_resize(&tok.id, m-n+1));
      char_buf_memcpy(&tok.id, line+n, m-n);
      char_buf_memcpy_at(&tok.id, m-n, "\0", 1);
      n=m;
      expected |= ~(VALUE_TOKEN | ID_TOKEN | INVALID_TOKEN | CLOSE_DELIM_TOKEN);
      expected &= ~(VALUE_TOKEN | ID_TOKEN );
      if (close_state == val_then_delim)
        close_state = close_delim_expected;
      if (close_state == close_delim_expected)
        expected |= CLOSE_DELIM_TOKEN;
    } /*end of ID_TOKEN...we start parsing things
       *that can be looked up in the tables*/
      
      /*scanning a lookup table, this
       *is a hack in disguise: at the end of string,
       *we are assured of no overruns because '/0' is not a
       *valid character for anything, so the second char of the string
       *at worst is '\0', no overrun, lol.
       *
       *this will obviously break once if I ever add operators with more
       *than two characters.
       */ 
#define scan_table(table, TYPE)                                 \
    for (i=0; i != (int)INVALID_##TYPE; ++i)                    \
      /*we can do this because operators tend to be short ;)*/	\
      if (*(line+n) == table[i][0])                             \
        {                                                       \
          /*if op is only one char long, instant match*/        \
          if (table[i][1] == '\0')                              \
            { break; }                                          \
          else if (*(line+n+1) == table[i][1])                  \
            { ++n; break;}                                      \
        }                                                       \
    ++n;                                                        \
    if (i==(int)INVALID_##TYPE)                                 \
      { /* the prescan should have weeded out things*/          \
        printf("How in the hell did this happen?\n");           \
        return malloc(0xDEADBEEF*3.14);                         \
      }							
      
    else if (type == OP_TOKEN) {
    scan_table(op_strs, OP);
    tok.op = (op_type)i;
    expected = VALUE_TOKEN | ID_TOKEN
      | PREFIX_OP_TOKEN | OPEN_DELIM_TOKEN ;
  } else if (type == PREFIX_OP_TOKEN) {
    scan_table(pref_op_strs, PREF);
    tok.prefix_op = (prefix_op_type)i;
    expected = VALUE_TOKEN | ID_TOKEN
      | PREFIX_OP_TOKEN | OPEN_DELIM_TOKEN ;
  } else if (type == OPEN_DELIM_TOKEN) {
    scan_table(open_strs,OPEN);
    tok.open = (open_delim_type)i;
    /*peek top*/
    if (!ibuf_empty(tok_stack)
      && ibuf_get(tok_stack).type == VALUE_TOKEN)
      push_token_stack((token){
          .type=OP_TOKEN,.real_pos=n,.op = MUL_OP}); 
    close_state = val_then_delim;
    expected = VALUE_TOKEN |
      ID_TOKEN | PREFIX_OP_TOKEN | OPEN_DELIM_TOKEN ;
    ++pdepth;
  } else if (type == CLOSE_DELIM_TOKEN) {
    if (!pdepth)
      gotoerr(n, SYNTAX_ERROR, "unexpected closing delimiter");
    scan_table(close_strs,CLOSE);
    tok.close = (close_delim_type)i;
    if (i == LM_DEF_CLOSE) {
      /*the open para was really a lambda argument list.*/
      /*scan down the out stack until we find the enclosing parentheses*/
      expected |= OPEN_QUOTE_TOKEN;
    } else {
      expected = OP_TOKEN | ENDL_TOKEN;
      if (--pdepth)
        expected |= CLOSE_DELIM_TOKEN,
          close_state = close_delim_expected;
      else
        expected &=~CLOSE_DELIM_TOKEN,
          close_state = close_delim_unexpected;
    }
  } else if ( type == OPEN_QUOTE_TOKEN ) {
    ++qdepth;
    expected |= VALUE_TOKEN |
      ID_TOKEN | OPEN_DELIM_TOKEN | PREFIX_OP_TOKEN |
      ENDL_TOKEN | CLOSE_QUOTE_TOKEN;
  } else if ( type == CLOSE_QUOTE_TOKEN ) {
    if (--qdepth)
      expected &= ~CLOSE_QUOTE_TOKEN;
  } else if ( type == ENDL_TOKEN ) {
    if (pdepth)
      gotoerr(n, SYNTAX_ERROR,"unclosed parentheses");
    while(qdepth-->0) push_token_stack((token){
    .type=OPEN_QUOTE_TOKEN,.real_pos=n});
    break;
  }
    push_token_stack(tok);
#undef scan_table
  }/*end parsing for-loop*/
 end:
  return (*_g.ret)
#undef push_out
}


aifaleene(char *line, astate* state)
{
  /*error handling*/
  mem_guard _g  =
    (mem_guard){
    .cur=0, .alloc=0,
    .lineno=&n, .errstr=errstr,.ret=&ret};
  /*general stuff*/
  int i, len=strlen(line),ret=0, st;
  /* these are the stacks*/
  token_ibuf tok_stack, aux_stack, out_stack;
  tok_stack = aux_stack = out_stack = null_stack;
#define push_aux_stack(p) push_stack(aux_stack, p)
#define push_out_stack(p) push_stack(out_stack, p)
  token res; var tmp;
  char errstr[max(len+1,64)];

  _a(token_ibuf_mk(&tok_stack));
  
  if (! tokenize(line, &out_stack, len,  _g) ) goto end;
  
  verbose(*state) && dump_stack(&tok_stack, "");
  
  _a(token_ibuf_mk_sz(&aux_stack,tok_stack.sz));
  _a(token_ibuf_mk_sz(&out_stack,tok_stack.sz));
  /*to rpn*/
  for(i=0; i<=tok_stack.pos; ++i) {
    token tok = ibuf_geti(tok_stack,i);
    switch(tok.type){
    case ID_TOKEN:
    case VALUE_TOKEN:
      token_ibuf_push(&out_stack, tok); break;
    case PREFIX_OP_TOKEN:
    case OP_TOKEN:
      /*peek top of stack*/
      if (!ibuf_empty(aux_stack) &&
          precedence(ibuf_get(aux_stack)) >= precedence(tok)) {
        token_ibuf_push(&out_stack,ibuf_get(aux_stack));
        ibuf_get(aux_stack) = tok;/*swap*/
      } else {
        token_ibuf_push(&aux_stack,tok);
      }
      break;
    case OPEN_DELIM_TOKEN:
      token_ibuf_push(&aux_stack,tok);
      break;
    case CLOSE_DELIM_TOKEN:
      if (tok.close == LM_DEF_CLOSE){ /*the open para's were a lambda declaration*/
          
      } else {
        while(!ibuf_empty(aux_stack) &&
              ibuf_get(aux_stack).type != OPEN_DELIM_TOKEN)
          token_ibuf_push(&out_stack, ibuf_pop(aux_stack));
      }
      //assert(ibuf_get(aux_stack).type == OPEN_DELIM_TOKEN);
      ibuf_pop(aux_stack);
      break;
    default: break;
    }
  }
  while(!ibuf_empty(aux_stack))
    token_ibuf_push(&out_stack, ibuf_pop(aux_stack));
  
  _f(token_ibuf_free(&tok_stack),0);
  verbose(*state) && dump_stack(&out_stack, ",");
  /*evaluate*/
  if(evaluate(state, &out_stack, out_stack.pos, &aux_stack, _g))
    goto end; 
  _f(token_ibuf_free(&out_stack),2);
  
  /*printing out stack element.*/
  res = ibuf_get(aux_stack);
  tok_to_val(res);
  
  if(res.type != VALUE_TOKEN)
    gotoerr(res.real_pos, SYNTAX_ERROR, "Final token is not a value?");
  print_value(res.v); printf("\n");
  /*assign value to _*/
  tmp = (var){.type=VALUE_ID,
		.name = "_",
		.v = res.v};
  if((st=var_setdefault(&tmp, state->table)))
    gotoerr(res.real_pos, st, "Error assigning to _");
  _f(token_ibuf_free(&aux_stack),1);
  ret = 0;
 end:
  if (_g.alloc & 0x01)
    {verbose(*state) && printf("freeing tok\n"); token_ibuf_free(&tok_stack);}
  if (_g.alloc & 0x02)
    {verbose(*state) && printf("freeing aux\n"); token_ibuf_free(&aux_stack);}
  if (_g.alloc & 0x04)
    {verbose(*state) && printf("freeing out\n"); token_ibuf_free(&out_stack);}
  if (! ret) return 0;
  /*error reporting*/
  print_err(ret);
  line_error(line, n, errstr);
  return ret;
}
#undef tok_to_val

astate_mk(astate *in){
  hash_init(in->table);
  in->flags=0; //returns 0;
}

astate_free(astate* in)
{
  var_hash_free(in->table,NULL);
}

inspect_state(astate* in)
{
  for(int i=0;i<HASH_SIZE;++i)
    {
      var_list *l=0;
      if(!in->table[i]) continue;
      printf("%04d: %p\n",i,in->table[i]);
      l=in->table[i];
      for(;l!=NULL;l=l->next)
	{
	  printf(" name: %s\n",l->data.name);
	  printf(" type: %s\n",l->data.type == VALUE_ID ? "value" : "function");
	  if (l->data.type == VALUE_ID)
	    { printf(" value:"); print_value(l->data.v); printf("\n");}
	}
      printf("done.\n");
    }
  return 0;
}

const char online_help[] = "\
usage:\n\
  At the > prompt, enter an expression to evaluate it.\n\
  For special commands, use a command starting with #.\n\
commands:\n\
  #h           Output this help.\n\
  #i           Inspect the hash table of vars.\n\
  #v           Toggle verbosity during evaluation.\n\
  #q           Quit.";

main(int ac, char *av[])
{
  char* line;
  astate state;
  
  astate_mk(&state);
  for(;;)
    {
      line = readline(">");
      if (!line)
	{printf("\n"); break;}
      add_history(line);
      if (!first_few(line,"#q"))
	{ break; }
      else if(!first_few(line, "#i"))
	{ inspect_state(&state); continue; }
      else if(!first_few(line, "#v"))
	{ state.flags ^= VERBOSE_AFLAG; continue; }
      else if(!first_few(line, "#h"))
	{ puts(online_help); continue;}
      aifaleene(line, &state);
      free(line);
    }
  astate_free(&state);
  return 0;
}
