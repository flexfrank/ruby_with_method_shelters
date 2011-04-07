#include "ruby/ruby.h"
#include "ruby/util.h"
#include "ruby/st.h"
#include "eval_intern.h"
#define ARRAY_LAST(ary) (RARRAY_PTR(ary)[RARRAY_LEN(ary)-1])


VALUE rb_cShelter;
VALUE rb_cShelterNode;

typedef struct shelter_struct{
  VALUE name;
  int hidden;
  VALUE exposed_imports;
  VALUE hidden_imports;
  st_table *exposed_method_table; /*klass->symbol->symbol*/
  st_table *hidden_method_table;  /*klass->symbol->symbol*/
} shelter_t;

static inline shelter_t* current_shelter();

typedef enum{
    SHELTER_IMPORT_ROOT,
    SHELTER_IMPORT_EXPOSED,
    SHELTER_IMPORT_HIDDEN
} SHELTER_IMPORT_TYPE;

typedef enum{
    SEARCH_ROOT_UNDEFINED=0,
    SEARCH_ROOT_EXPOSED,
    SEARCH_ROOT_HIDDEN
} SHELTER_SEARCH_ROOT_TYPE;

typedef struct shelter_node_struct{
    shelter_t* shelter;
    struct shelter_node_struct** exposed_imports;
    long exposed_num;
    struct shelter_node_struct** hidden_imports;
    long hidden_num;
    struct shelter_node_struct* parent;
    struct shelter_node_struct* search_root;
    SHELTER_IMPORT_TYPE import_type;
    SHELTER_SEARCH_ROOT_TYPE search_root_type;
} shelter_node_t;




static shelter_node_t*
make_shelter_node(
        shelter_t* shelter, 
        shelter_node_t** exposed_imports, long exposed_num,
        shelter_node_t** hidden_imports, long hidden_num,
        SHELTER_IMPORT_TYPE type
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
    node->search_root=NULL;
    node->import_type=type;
}

static void shelter_mark(void*);
static void
mark_shelter_node(shelter_node_t* node){
    long i;
    shelter_mark(node->shelter);
    for(i=0;i < node->exposed_num;i++){
        mark_shelter_node(node->exposed_imports[i]);
    }
    free(node->exposed_imports);
    for(i=0;i < node->hidden_num;i++){
        mark_shelter_node(node->hidden_imports[i]);
    }
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

static shelter_node_t*
search_root(shelter_node_t* node){
    if(node->search_root){
        return node->search_root;
    }else{
        shelter_node_t* sroot=NULL;
        SHELTER_SEARCH_ROOT_TYPE search_type=SEARCH_ROOT_UNDEFINED;
        switch(node->import_type){
            case SHELTER_IMPORT_ROOT:
                sroot=node;
                search_type=SEARCH_ROOT_EXPOSED;
                break;
            case SHELTER_IMPORT_EXPOSED:
                sroot= search_root(node->parent);
                search_type=SEARCH_ROOT_EXPOSED;
                break;
            case SHELTER_IMPORT_HIDDEN:
                sroot=node->parent;
                search_type=SEARCH_ROOT_HIDDEN;
                break;
        }
        node->search_root=sroot;
        node->search_root_type=search_type;
        return sroot;
    }
}

static SHELTER_SEARCH_ROOT_TYPE
search_root_type(shelter_node_t* node){
    if(node->search_root_type==SEARCH_ROOT_UNDEFINED){
        search_root(node);
    }
    return node->search_root_type;
}

static void
shelter_node_mark(void* node){
    mark_shelter_node((shelter_node_t*)node);
}

static void
shelter_node_free(void* node){
    free_shelter_node((shelter_node_t*)node);
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

enum {
    SHELTER_NODE_MAX_DEPTH=256
} shelter_node_max_depth;
static shelter_node_t*
make_shelter_tree(shelter_t* shelter,long depth,SHELTER_IMPORT_TYPE type){
    if(depth>SHELTER_NODE_MAX_DEPTH){
        rb_raise(rb_eRuntimeError,"depth of shelter is too large");
    }
    VALUE *ex,*hi;
    long ex_len,hi_len;
    shelter_node_t **ex_nodes,**hi_nodes;
    int i;
    shelter_node_t* result;



    ex=RARRAY_PTR(shelter->exposed_imports);
    ex_len=RARRAY_LEN(shelter->exposed_imports);
    hi=RARRAY_PTR(shelter->hidden_imports);
    hi_len=RARRAY_LEN(shelter->hidden_imports);

    ex_nodes=malloc(sizeof(shelter_node_t*)*ex_len);
    for(i=0;i<ex_len;i++){
        shelter_t* sh;
        Data_Get_Struct(ex[i],shelter_t,sh);
        ex_nodes[i]=make_shelter_tree(sh,depth+1,SHELTER_IMPORT_EXPOSED);    
    }
    

    hi_nodes=malloc(sizeof(shelter_node_t*)*hi_len);
    for(i=0;i<hi_len;i++){
        shelter_t* sh;
        Data_Get_Struct(hi[i],shelter_t,sh);
        hi_nodes[i]=make_shelter_tree(sh,depth+1,SHELTER_IMPORT_HIDDEN);
    }

    result=make_shelter_node(shelter,ex_nodes,ex_len,hi_nodes,hi_len,type);


    free(ex_nodes);
    free(hi_nodes);

    for(i=0; i < result->exposed_num; i++){
        result->exposed_imports[i]->parent=result;
    }
    for(i=0; i < result->hidden_num; i++){
        result->hidden_imports[i]->parent=result;
    }
    return result;
}

static void
print_indent(int depth){
    int i;
    for(i=0;i<depth;i++){
        printf("  ");
    }
}

static void
dump_shelter_tree(shelter_node_t* node,int depth){
    VALUE name;
    long i;
    const char* type="";
    switch(node->import_type){
    case SHELTER_IMPORT_ROOT: type="root";break;
    case SHELTER_IMPORT_EXPOSED: type="exposed";break;
    case SHELTER_IMPORT_HIDDEN: type="hidden";break;
    }
    name=rb_sym_to_s(node->shelter->name);
    print_indent(depth);
    printf("%s(%s)%p<%p->%p[\n",RSTRING_PTR(name),type,node,node->parent,search_root(node));
    for(i=0;i<node->exposed_num;i++){
        dump_shelter_tree(node->exposed_imports[i],depth+1);
    }
    for(i=0;i<node->hidden_num;i++){
        dump_shelter_tree(node->hidden_imports[i],depth+1);
    }
    print_indent(depth);
    printf("]\n");

}

VALUE rb_yield_with_shelter_node(void* shelter_node);

VALUE
shelter_eval(VALUE self, VALUE shelter_symbol){
    VALUE shelter_val=get_shelter(shelter_symbol);  
    shelter_t* shelter;
    rb_thread_t* th=GET_THREAD();
    shelter_node_t* node;
    VALUE node_val;

    Data_Get_Struct(shelter_val,shelter_t, shelter);
    node= make_shelter_tree(shelter,0,SHELTER_IMPORT_ROOT);
    rb_p(rb_sprintf("nodeBefore:%p",node));

    rb_yield_with_shelter_node(node);

    dump_shelter_tree(node,0);
    return Qnil;
}

static VALUE
current_node(VALUE self){
    rb_thread_t* th=GET_THREAD(); 
    rb_control_frame_t* cfp=th->cfp;
    shelter_node_t* node = cfp->shelter_node;
    rb_p(rb_sprintf("node:%p",node));
}

void Init_Shelter(void){
  rb_define_singleton_method(rb_vm_top_self(),"shelter", define_shelter, 1);
  rb_define_singleton_method(rb_vm_top_self(),"import", import_shelter, 1);
  rb_define_singleton_method(rb_vm_top_self(),"hide", hide_shelter, 0);
  rb_define_singleton_method(rb_vm_top_self(),"expose", expose_shelter, 0);
  rb_define_singleton_method(rb_vm_top_self(),"current_node", current_node, 0);
  rb_define_singleton_method(rb_vm_top_self(),"shelter_eval", shelter_eval, 1);
  rb_cShelter = rb_define_class("Shelter", rb_cObject);
  rb_cShelterNode = rb_define_class_under(rb_cShelter,"ShelterNode", rb_cObject);
  rb_define_method(rb_cShelter,"to_s",shelter_to_s,0);
  rb_define_method(rb_cShelter,"inspect",shelter_inspect,0);
  rb_undef_alloc_func(rb_cShelter);
}
