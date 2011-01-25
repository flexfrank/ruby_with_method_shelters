#include "ruby/ruby.h"
#include "ruby/util.h"
#include "ruby/st.h"
#include "eval_intern.h"
#define ARRAY_LAST(ary) (RARRAY_PTR(ary)[RARRAY_LEN(ary)-1])

VALUE rb_cMethodpack;

typedef struct methodpack_struct{
  VALUE name;
  VALUE exposed_imports;
  VALUE hidden_imports;
} methodpack_t;

static void 
methodpack_mark(void* mpack){
  methodpack_t* methodpack=mpack;
  rb_gc_mark(methodpack->name);
  rb_gc_mark(methodpack->exposed_imports);
  rb_gc_mark(methodpack->hidden_imports);
}
static void 
methodpack_free(void* mpack){
  methodpack_t* methodpack=mpack;
  methodpack->name=Qnil;
  methodpack->exposed_imports=Qnil;
  methodpack->hidden_imports=Qnil;
  free(methodpack);
}
static VALUE 
methodpack_alloc(VALUE klass,VALUE name){
  Check_Type(name,T_SYMBOL);
  methodpack_t* mpack = ALLOC(methodpack_t);
  mpack->name=name;
  mpack->exposed_imports=rb_ary_tmp_new(0);
  mpack->hidden_imports=rb_ary_tmp_new(0);
  return Data_Wrap_Struct(klass,methodpack_mark,methodpack_free, mpack);
}

static VALUE
get_methodpack(VALUE name){
  Check_Type(name, T_SYMBOL);
  ID mpackid=SYM2ID(name);
  if (!rb_is_const_id(mpackid)) {
    rb_name_error(mpackid, "wrong constant name %s", rb_id2name(mpackid));
  }
  
  VALUE methodpack=Qnil;
  if(rb_const_defined_at(rb_cMethodpack,mpackid)){
    VALUE val=rb_const_get_at(rb_cMethodpack,mpackid);
    if(CLASS_OF(val)==rb_cMethodpack){
      methodpack=val;
    }else{
      rb_raise(rb_eTypeError, "%s is not a methodpack", rb_id2name(mpackid));
    }
  } 
  return methodpack;
}
/* 
 * visibility:
 *   exposed: Qtrue
 *   hidden: Qfalse
 * */
static VALUE 
define_methodpack(VALUE self,VALUE name){
  Check_Type(name, T_SYMBOL);
  ID mpackid = SYM2ID(name);
  if (!rb_is_const_id(mpackid)) {
    rb_name_error(mpackid, "wrong constant name %s", rb_id2name(mpackid));
  }
  
  VALUE methodpack;
  if(rb_const_defined_at(rb_cMethodpack,mpackid)){
    VALUE val=rb_const_get_at(rb_cMethodpack,mpackid);
    if(CLASS_OF(val)==rb_cMethodpack){
      methodpack=val;
    }else{
      rb_raise(rb_eTypeError, "%s is not a methodpack", rb_id2name(mpackid));
    }
  }else{
    methodpack=methodpack_alloc(rb_cMethodpack, name);
    rb_define_const(rb_cMethodpack, rb_id2name(mpackid),methodpack);
  }
  rb_thread_t *th = GET_THREAD();
  if(!th->methodpack_stack){
    th->methodpack_stack=rb_ary_tmp_new(1);
  }
  if(!th->methodpack_visibility_stack){
    th->methodpack_visibility_stack=rb_ary_tmp_new(1);
  }
  rb_ary_push(th->methodpack_stack,methodpack);
  rb_ary_push(th->methodpack_visibility_stack,Qtrue);;
  VALUE mp=ARRAY_LAST(th->methodpack_stack);
  VALUE val= rb_yield(methodpack); 
  rb_ary_pop(th->methodpack_stack);
  rb_ary_pop(th->methodpack_visibility_stack);
  return val;
}

static inline methodpack_t*
current_methodpack(){
  rb_thread_t* th=GET_THREAD();
  if(th->methodpack_stack){
    long len=RARRAY_LEN(th->methodpack_stack);
    if(len>0){
      VALUE last=ARRAY_LAST(th->methodpack_stack);
      methodpack_t* mp;
      Data_Get_Struct(last, methodpack_t, mp);

      return mp;
    }
  }
  return NULL;
}

static VALUE
import_methodpack(VALUE self,VALUE import_sym){
  Check_Type(import_sym,T_SYMBOL);
  methodpack_t* current=current_methodpack();
  VALUE importpack=get_methodpack(import_sym);
  rb_thread_t *th = GET_THREAD();
  /* exposed */
  if(RTEST(ARRAY_LAST(th->methodpack_visibility_stack))){
    if(current && !rb_ary_includes(current->exposed_imports,importpack)){
      rb_ary_push(current->exposed_imports,importpack);
    }
  }else{
    if(current && !rb_ary_includes(current->hidden_imports,importpack)){
      rb_ary_push(current->hidden_imports,importpack);
    }
  }
  return Qnil;
}

static VALUE
methodpack_enable_hidden(VALUE self){
  rb_thread_t *th = GET_THREAD();
  if(th->methodpack_visibility_stack){
    VALUE result;
    rb_ary_push(th->methodpack_visibility_stack,Qfalse);
    result=rb_yield_values2(0,NULL);
    rb_ary_pop(th->methodpack_visibility_stack);
  }else{
    rb_raise(rb_eScriptError,"you should call this method inside a methodpack");
  }
}

static VALUE
methodpack_to_s(VALUE self){
  methodpack_t* mpack;
  Data_Get_Struct(self,methodpack_t, mpack);
  VALUE result;
  if(RTEST(mpack->name)){
    result=rb_str_new2("");
    rb_str_cat2(result, "#<Methodpack:");
    rb_str_concat(result, rb_sym_to_s(mpack->name));
    rb_str_cat2(result,">");
  }else{
    result=rb_any_to_s(self);
  }
  return result;
}

void Init_Methodpack(void){
  rb_define_singleton_method(rb_vm_top_self(),"methodpack", define_methodpack, 1);
  rb_define_singleton_method(rb_vm_top_self(),"import", import_methodpack, 1);
  rb_define_singleton_method(rb_vm_top_self(),"hidden", methodpack_enable_hidden, 0);
  rb_cMethodpack = rb_define_class("Methodpack", rb_cObject);
  rb_define_method(rb_cMethodpack,"to_s",methodpack_to_s,0);
  rb_undef_alloc_func(rb_cMethodpack);
}
