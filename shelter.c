#include "ruby/ruby.h"
#include "ruby/util.h"
#include "ruby/st.h"
#include "eval_intern.h"
#define ARRAY_LAST(ary) (RARRAY_PTR(ary)[RARRAY_LEN(ary)-1])

VALUE rb_cShelter;

typedef struct shelter_struct{
  VALUE name;
  int hidden;
  VALUE exposed_imports;
  VALUE hidden_imports;
  VALUE method_table_keys;
  st_table *exposed_method_table;
  st_table *hidden_method_table;
} shelter_t;

typedef struct shelter_node_struct{
  shelter_t* shelter;
  struct shelter_node_struct* exposed_imports;
  struct shelter_node_struct* hidden_imports;
  struct shelter_node_struct* parent;
} shelter_node_t;

static void 
shelter_mark(void* shelter){
  shelter_t* s=shelter;
  rb_gc_mark(s->name);
  rb_gc_mark(s->exposed_imports);
  rb_gc_mark(s->hidden_imports);
  rb_gc_mark(s->method_table_keys);
}
static void 
shelter_free(void* shelter){
  shelter_t* s=shelter;
  st_free_table(s->exposed_method_table);
  st_free_table(s->hidden_method_table);
  free(shelter);
}

static int
shelter_method_cmp(st_data_t x, st_data_t y) {
    VALUE *key1=(VALUE*)x;
    VALUE *key2=(VALUE*)y;
    return key1[0]==key2[0] && key1[1]==key2[1];
}

st_index_t
shelter_method_hash(st_data_t n)
{
    VALUE *key=(VALUE*)n;
    return (st_index_t)key[0]+key[1]>>2;
}


static const struct st_hash_type type_shelter_method_hash = {
    shelter_method_cmp,
    shelter_method_hash,
};


static VALUE 
shelter_alloc(VALUE klass,VALUE name){
  Check_Type(name,T_SYMBOL);
  shelter_t* s = ALLOC(shelter_t);
  s->name=name;
  s->exposed_imports=rb_ary_tmp_new(0);
  s->hidden_imports=rb_ary_tmp_new(0);
  s->method_table_keys=rb_ary_tmp_new(0);
  s->exposed_method_table=st_init_table(&type_shelter_method_hash);
  s->hidden_method_table=st_init_table(&type_shelter_method_hash);
  s->hidden=0;
  return Data_Wrap_Struct(klass,shelter_mark,shelter_free, s);
}

static VALUE
get_shelter(VALUE name){
  Check_Type(name, T_SYMBOL);
  ID shelterid=SYM2ID(name);
  if (!rb_is_const_id(shelterid)) {
    rb_name_error(shelterid, "wrong constant name %s", rb_id2name(shelterid));
  }
  
  VALUE shelter=Qnil;
  if(rb_const_defined_at(rb_cShelter,shelterid)){
    VALUE val=rb_const_get_at(rb_cShelter,shelterid);
    if(CLASS_OF(val)==rb_cShelter){
      shelter=val;
    }else{
      rb_raise(rb_eTypeError, "%s is not a shelter", rb_id2name(shelterid));
    }
  } 
  return shelter;
}

static VALUE 
define_shelter(VALUE self,VALUE name){
  Check_Type(name, T_SYMBOL);
  ID shelterid = SYM2ID(name);
  if (!rb_is_const_id(shelterid)) {
    rb_name_error(shelterid, "wrong constant name %s", rb_id2name(shelterid));
  }
  
  VALUE shelter;
  if(rb_const_defined_at(rb_cShelter,shelterid)){
    VALUE val=rb_const_get_at(rb_cShelter,shelterid);
    if(CLASS_OF(val)==rb_cShelter){
      shelter=val;
    }else{
      rb_raise(rb_eTypeError, "%s is not a shelter", rb_id2name(shelterid));
    }
  }else{
    shelter=shelter_alloc(rb_cShelter, name);
    rb_define_const(rb_cShelter, rb_id2name(shelterid),shelter);
  }
  rb_thread_t *th = GET_THREAD();
  if(!th->shelter_stack){
    th->shelter_stack=rb_ary_tmp_new(1);
  }
  rb_ary_push(th->shelter_stack,shelter);
  VALUE val= rb_yield(shelter); 
  rb_ary_pop(th->shelter_stack);
  return val;
}


static inline shelter_t*
current_shelter(){
  rb_thread_t* th=GET_THREAD();
  if(th->shelter_stack){
    long len=RARRAY_LEN(th->shelter_stack);
    if(len>0){
      VALUE last=ARRAY_LAST(th->shelter_stack);
      shelter_t* mp;
      Data_Get_Struct(last, shelter_t, mp);

      return mp;
    }
  }
  return NULL;
}

rb_method_entry_t*
method_entry_in_shelter(VALUE klass,ID mid){
  shelter_t* shelter = current_shelter();
  VALUE method_sym=ID2SYM(mid);
  if(shelter){
    if(shelter->hidden){
      printf("hidden\n");
    }else{
      printf("exposed\n");
    }
    rb_p(klass);
    rb_p(method_sym);
    printf("define method\n");
  }
  return NULL;
}


static VALUE
import_shelter(VALUE self,VALUE import_sym){
  Check_Type(import_sym,T_SYMBOL);
  shelter_t* current=current_shelter();
  VALUE importshelter=get_shelter(import_sym);
  rb_thread_t *th = GET_THREAD();
  /* exposed */
  if(current){
    if(current->hidden){
      if(current && !rb_ary_includes(current->hidden_imports,importshelter)){
        rb_ary_push(current->hidden_imports,importshelter);
      }
    }else{
      if(current && !rb_ary_includes(current->exposed_imports,importshelter)){
        rb_ary_push(current->exposed_imports,importshelter);
      }
    }
  }
  return Qnil;
}

static VALUE
hide_shelter(VALUE self){
  shelter_t* current = current_shelter();
  current->hidden=TRUE;
  return Qnil;
}
static VALUE
expose_shelter(VALUE self){
  shelter_t* current = current_shelter();
  current->hidden=FALSE;
  return Qnil;
}

static VALUE
shelter_to_s(VALUE self){
  shelter_t* s;
  Data_Get_Struct(self,shelter_t, s);
  VALUE result;
  if(RTEST(s->name)){
    result=rb_str_new2("");
    rb_str_cat2(result, "#<Shelter:");
    rb_str_concat(result, rb_sym_to_s(s->name));
    rb_str_cat2(result,">");
  }else{
    result=rb_any_to_s(self);
  }
  return result;
}
static VALUE
shelter_children_string(VALUE ary){
  VALUE result=rb_str_new2("[");
  long i;
  for(i=0;i<RARRAY_LEN(ary);i++){
    if(i>0){
      rb_str_cat2(result,",");
    }
    rb_str_concat(result,shelter_to_s(RARRAY_PTR(ary)[i]));
  }
  rb_str_cat2(result,"]");
  return result;
}

static VALUE
shelter_inspect(VALUE self){
  shelter_t* s;
  Data_Get_Struct(self,shelter_t, s);
  VALUE result;
  result=rb_str_new2("");
  rb_str_cat2(result, "#<Shelter:");
  if(RTEST(s->name)){
    rb_str_concat(result, rb_sym_to_s(s->name));
  }
  rb_str_cat2(result," exposed=");
  rb_str_concat(result, shelter_children_string(s->exposed_imports));
  rb_str_cat2(result," hidden=");
  rb_str_concat(result, shelter_children_string(s->hidden_imports));
  rb_str_cat2(result,">");
  return result;
  
}
void Init_Shelter(void){
  rb_define_singleton_method(rb_vm_top_self(),"shelter", define_shelter, 1);
  rb_define_singleton_method(rb_vm_top_self(),"import", import_shelter, 1);
  rb_define_singleton_method(rb_vm_top_self(),"hide", hide_shelter, 0);
  rb_define_singleton_method(rb_vm_top_self(),"expose", expose_shelter, 0);
  rb_cShelter = rb_define_class("Shelter", rb_cObject);
  rb_define_method(rb_cShelter,"to_s",shelter_to_s,0);
  rb_define_method(rb_cShelter,"inspect",shelter_inspect,0);
  rb_undef_alloc_func(rb_cShelter);
}
