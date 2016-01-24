#include "aifaleene.h"

/* token_buf */
void
token_buf_clean(token_buf* in) {
  for (int i=0; i != in->sz; ++i)
    token_clean(in->data[i]);
}


/* var */

var_list*
var_list_find(const char* id,var_list* l) {
  for(;l!=NULL; l=l->next)
    if ( !strcmp(l->data.name,id) ) break;
  return l;
}

var_get(const char* id,
        var_hash_table table,
        var** out) {
  int h = hash(id);
  var_list *l = table[h];
  if (!l || !(l=var_list_find(id,l)))
    return NOTFOUND_ERROR;
  *out = &l->data;
  return 0;
}

var_setdefault(const var* in,
               var_hash_table table) {
  int h = hash(in->name);
  
  var_list *l;
  if (table[h]
      && (l=var_list_find(in->name,table[h])))
    { l->data = *in; return 0;} 
  else
    { return var_list_pushp(table+h,in) ? MEMORY_ERROR : 0; }
}
		      

var_add(var* in,
        var_hash_table table) {
  int h = hash(in->name);
  if (table[h] && var_list_find(in->name,table[h]))
    return NOTFOUND_ERROR; /*here it is a "found" error*/
  var_list_pushp(table+h,in);
  return 0;
}
