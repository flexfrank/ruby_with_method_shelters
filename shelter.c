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
  st_table *exposed_method_table; /*klass->symbol->symbol*/
  st_table *hidden_method_table;  /*klass->symbol->symbol*/
} shelter_t;

static inline shelter_t* current_shelter();

typedef struct shelter_node_struct{
    shelter_t* shelter;
    struct shelter_node_struct** exposed_imports;
    long exposed_num;
    struct shelter_node_struct** hidden_imports;
    long hidden_num;
    struct shelter_node_struct* parent;
} shelter_node_t;


static shelter_node_t*
make_shelter_node(
        shelter_t* shelter, 
        shelter_node_t** exposed_imports, int exposed_num,
        shelter_node_t** hidden_imports, int hidden_num
){
    long i;
    shelter_node_t* node=malloc(sizeof(shelter_node_t));
    node->shelter=shelter;
    node->exposed_imports=malloc(sizeof(shelter_node_t*)*exposed_num);
    for(i=0; i < exposed_num;i++){
        node->exposed_imports[i]=exposed_imports[i];
    }
    node->exposed_num=exposed_num;
    node->hidden_imports=malloc(sizeof(shelter_node_t*)*hidden_num);
    for(i=0;i < hidden_num;i++){
        node->hidden_imports[i]=hidden_imports[i];
    }
    node->hidden_num=hidden_num;
    node->parent=NULL;
}

static void
free_shelter_node(shelter_node_t* node){
    long i;
    for(i=0;i < node->exposed_num;i++){
        free_shelter_node(node->exposed_imports[i]);
    }
    free(node->exposed_imports);
    for(i=0;i < node->hidden_num;i++){
        free_shelter_node(node->hidden_imports[i]);
    }
    free(node->hidden_imports);
    free(node);
}


static int
method_table_mark_entries(st_data_t key, st_data_t val, st_data_t arg){
  rb_gc_mark((VALUE)key);/*symbol*/
  rb_gc_mark((VALUE)val);/*symbol*/
  return ST_CONTINUE;
}

static int
method_table_mark(st_data_t key, st_data_t val, st_data_t arg){
  rb_gc_mark((VALUE)key);/*klass*/
  st_foreach((st_table*)val,method_table_mark_entries,0);
  return ST_CONTINUE;
}

static void 
shelter_mark(void* shelter){
  shelter_t* s=shelter;
  rb_gc_mark(s->name);
  rb_gc_mark(s->exposed_imports);
  rb_gc_mark(s->hidden_imports);
  st_foreach(s->exposed_method_table,method_table_mark,0);
  st_foreach(s->hidden_method_table,method_table_mark,0);

}

static int
method_shelter_table_free(st_data_t key,st_data_t val,st_data_t arg){
  st_table_free((st_table*)val);
  return ST_CONTINUE;
}

static void 
shelter_free(void* shelter){
  shelter_t* s=shelter;
  st_foreach(s->exposed_method_table,method_table_mark,0);
  st_foreach(s->hidden_method_table,method_table_mark,0);
  st_free_table(s->exposed_method_table);
  st_free_table(s->hidden_method_table);
  free(shelter);
}



ID
shelter_convert_method_name(VALUE klass,ID methodname){
  shelter_t* s=current_shelter();
  if(s){
    st_table* conv_name_tbl;
    st_table* mtbl;
    VALUE newname_sym;
    VALUE newname=rb_str_new_cstr("shelter#");
    rb_str_concat(newname,rb_sprintf("%p#",s));
    rb_str_concat(newname,rb_id2str(methodname));
    newname_sym=rb_str_intern(newname);
    rb_p(newname);

    if(s->hidden){
      mtbl=s->hidden_method_table;
    }else{
      mtbl=s->exposed_method_table;
    }


    if(!st_lookup(mtbl,klass,(st_data_t*)&conv_name_tbl)){
      conv_name_tbl=st_init_numtable();
      st_insert(mtbl,klass,(st_data_t)conv_name_tbl);
    }

    st_insert(conv_name_tbl,ID2SYM(methodname),newname_sym);
   

    return SYM2ID(newname_sym);
  }else{
    return methodname;
  }
}

static VALUE 
shelter_alloc(VALUE klass,VALUE name){
  Check_Type(name,T_SYMBOL);
  shelter_t* s = ALLOC(shelter_t);
  s->name=name;
  s->exposed_imports=rb_ary_tmp_new(0);
  s->hidden_imports=rb_ary_tmp_new(0);
  s->exposed_method_table=st_init_numtable();
  s->hidden_method_table=st_init_numtable();
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
      if(RTEST(last)){
        shelter_t* mp;
        Data_Get_Struct(last, shelter_t, mp);
        return mp;
      }else{
        return NULL;
      }
    }
  }
  return NULL;
}

int
is_in_shelter(){
    return current_shelter != NULL;
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

VALUE
search_shelter_method_name(VALUE id, VALUE klass){
    //rb_p(id);
    return id;
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
