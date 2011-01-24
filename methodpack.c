#include "ruby/ruby.h"
#include "ruby/util.h"
#include "ruby/st.h"
#include "eval_intern.h"
VALUE rb_cMethodpack;
typedef struct methodpack_struct{
  VALUE name;
} methodpack_t;
/*static VALUE methodpack_forbid_alloc(VALUE klass){
  rb_raise(rb_eNotImpError, "do not call Methodpack.new");
  return Qnil;
}*/
static void 
methodpack_mark(void* mpack){
  methodpack_t* methodpack=mpack;
  rb_gc_mark(methodpack->name);
}
static void 
methodpack_free(void* mpack){
  methodpack_t* methodpack=mpack;
  methodpack->name=Qnil;
  free(methodpack);
}
static VALUE 
methodpack_alloc(VALUE klass,VALUE name){
  Check_Type(name,T_SYMBOL);
  methodpack_t* mpack = ALLOC(methodpack_t);
  mpack->name=name;
  return Data_Wrap_Struct(klass,methodpack_mark,methodpack_free, mpack);

}

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
  return rb_yield(methodpack); 

}


void Init_Methodpack(void){
  rb_define_singleton_method(rb_vm_top_self(),"methodpack", define_methodpack, 1);
  rb_cMethodpack = rb_define_class("Methodpack", rb_cObject);

  rb_undef_alloc_func(rb_cMethodpack);


}
