#include "ruby/ruby.h"
#include "ruby/util.h"
#include "ruby/st.h"
#include "eval_intern.h"
#include "vm_core.h"
#include "shelter.h"
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
  //shelter_node_t* root_node;
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

typedef struct shelter_node_chache_key shelter_cache_key;
struct shelter_node_struct{
    shelter_t* shelter;
    struct shelter_node_struct** exposed_imports;
    long exposed_num;
    struct shelter_node_struct** hidden_imports;
    long hidden_num;
    struct shelter_node_struct* parent;
    struct shelter_node_struct* search_root;
    SHELTER_IMPORT_TYPE import_type;
    SHELTER_SEARCH_ROOT_TYPE search_root_type;
    //shelter_cache_key* cache_keys;
    st_table *method_cache_table;
};

struct shelter_node_chache_key{
    VALUE klass;
    ID name;
    //struct shelter_node_cache_key* next_key;
};

/*typedef struct shelter_node_chache_entry{
    VALUE vm_state;
    VALUE shelter_method_name;
    rb_method_entry_t* me;
    shelter_node_t* next_node;
} shelter_cache_entry;*/

static int
compare_shelter_cache(shelter_cache_key* key1, shelter_cache_key* key2){
    
    int result=key1->klass==key2->klass && key1->name==key2->name;
    return !result;
}

static st_index_t
hash_shelter_cache(shelter_cache_key* key){
    return 31*key->klass+key->name*7;
}

static struct st_hash_type shelter_cache_hash_type={compare_shelter_cache,hash_shelter_cache};

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
    //node->cache_keys=NULL;
    node->method_cache_table = st_init_table(&shelter_cache_hash_type);
}

static int
mark_node_cache_table(st_data_t key, st_data_t val, st_data_t arg){
    shelter_cache_entry* entry=(shelter_cache_entry*)val;
    shelter_cache_key* keyp=(shelter_cache_key*)key;
    if(entry->me){
        rb_mark_method_entry(entry->me);
    }
    return ST_CONTINUE;
}

static void shelter_mark(void*);
static void
mark_shelter_node(shelter_node_t* node){
    long i;
    //fprintf(stderr,"node_marked(%p)",node);
    shelter_mark(node->shelter);
    for(i=0;i < node->exposed_num;i++){
        mark_shelter_node(node->exposed_imports[i]);
    }
    for(i=0;i < node->hidden_num;i++){
        mark_shelter_node(node->hidden_imports[i]);
    }
    st_foreach(node->method_cache_table,mark_node_cache_table,0);
}

static int
free_cache_entries(shelter_cache_key* key, shelter_cache_entry* val, st_data_t arg){
    free(key);
    free(val);
    return ST_CONTINUE;
}

static void
free_shelter_node(shelter_node_t* node){
    long i;
    if(node==NULL){return;}
    for(i=0;i < node->exposed_num;i++){
        free_shelter_node(node->exposed_imports[i]);
    }
    free(node->exposed_imports);
    for(i=0;i < node->hidden_num;i++){
        free_shelter_node(node->hidden_imports[i]);
    }
    free(node->hidden_imports);

    st_foreach(node->method_cache_table, free_cache_entries, 0);
    st_free_table(node->method_cache_table);


    //fprintf(stderr,"freed_node(%p)\n",node);
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
    //fprintf(stderr,"shelter_marked(%p)",shelter);
  shelter_t* s=shelter;
  rb_gc_mark(s->name);
  rb_gc_mark(s->exposed_imports);
  rb_gc_mark(s->hidden_imports);
  st_foreach(s->exposed_method_table,method_table_mark,0);
  st_foreach(s->hidden_method_table,method_table_mark,0);

}

static int
method_table_free(st_data_t key,st_data_t val,st_data_t arg){
  st_free_table((st_table*)val);
  return ST_CONTINUE;
}

static void 
shelter_free(void* shelter){
  shelter_t* s=shelter;
  st_foreach(s->exposed_method_table,method_table_free,0);
  st_foreach(s->hidden_method_table,method_table_free,0);
  st_free_table(s->exposed_method_table);
  st_free_table(s->hidden_method_table);
  //free_shelter_node(s->root_node);
  //fprintf(stderr,"shelter_freed:%p",shelter);
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
  //s->root_node=0;
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
  if(!RTEST(th->shelter_stack)){
    th->shelter_stack=rb_ary_tmp_new(1);
  }
  rb_ary_push(th->shelter_stack,shelter);
  VALUE val= rb_yield(shelter); 
  rb_ary_pop(th->shelter_stack);
  return val;
}

static inline VALUE
cur_shelter(VALUE self){
  rb_thread_t* th=GET_THREAD();
  if(th->shelter_stack){
    long len=RARRAY_LEN(th->shelter_stack);
    if(len>0){
      VALUE last=ARRAY_LAST(th->shelter_stack);
      if(RTEST(last)){
        return last;
      }else{
        return Qnil;
      }
    }
  }
  return Qnil;
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
    fprintf(stderr,"is_in:%p\n",current_shelter());
    return current_shelter() != NULL;
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

VALUE rb_yield_with_shelter_node(void* shelter_node,VALUE node_val);

VALUE
shelter_eval(VALUE self, VALUE shelter_symbol){
    VALUE shelter_val=get_shelter(shelter_symbol);  
    shelter_t* shelter;
    rb_thread_t* th=GET_THREAD();
    shelter_node_t* node;
    volatile VALUE node_val;
    VALUE result;

    Data_Get_Struct(shelter_val,shelter_t, shelter);
    node= make_shelter_tree(shelter,0,SHELTER_IMPORT_ROOT);
    //shelter->root_node=node;
    //rb_p(rb_sprintf("nodeBefore:%p",node));
    node_val=Data_Wrap_Struct(rb_cShelterNode,shelter_node_mark, shelter_node_free,node);
    result=rb_yield_with_shelter_node(node,node_val);

    //dump_shelter_tree(node,0);
    return result;
}

static shelter_node_t*
cur_node(){
    rb_thread_t* th=GET_THREAD(); 
    rb_control_frame_t* cfp=th->cfp;
    shelter_node_t* n= cfp->shelter_node;
    return n;
}
 

static VALUE
current_node(VALUE self){
    shelter_node_t* node = cur_node();
    if(node){
        return node->shelter->name;
    }else{
        return Qnil;
    }
}
static int dump_method_table_entry(st_data_t,st_data_t,st_data_t);
static int dump_method_table(st_data_t,st_data_t,st_data_t);
static VALUE
shelter_lookup_method_table(st_table *table, VALUE klass, VALUE name){
    st_table* name2name_table=NULL;
    if(st_lookup(table,klass,(st_data_t*)&name2name_table)){
        VALUE conv_method_name=Qfalse;
        if(st_lookup(name2name_table,name,&conv_method_name)){
            return conv_method_name;
        }
    
    }    
    return Qfalse;
}

static VALUE
lookup_exposed(VALUE klass, VALUE name, shelter_node_t *node, shelter_node_t **next_node);

static inline VALUE
lookup_imports(VALUE klass, VALUE name,shelter_node_t* node, shelter_node_t** imports, long num, shelter_node_t** found_node){
    long i;
    VALUE* found_names=malloc(sizeof(VALUE)*num);
    shelter_node_t** found_nodes= malloc(sizeof(shelter_node_t*)*num);
    long found_num=0;
    VALUE result;
    for(i=0;i<num;i++){
        shelter_node_t* n=imports[i];
        shelter_node_t* fnode=NULL;
        VALUE fname=lookup_exposed(klass,name,n,&fnode);

        if(RTEST(fname)){
            found_names[found_num]=fname;
            found_nodes[found_num]=fnode;
            found_num++;
        }
    }
    if(found_num==0){
        if(found_node)*found_node=NULL;
        result=Qfalse;
    }else{
        result=found_names[0];
        if(found_node)*found_node=found_nodes[0];
        for(i=1;i<found_num;i++){
            if(result!=found_names[i]){
                result=Qfalse;
                if(found_node)*found_node=NULL;
                break;
            }
        }
    }
    
    free(found_names);
    free(found_nodes);
    if(found_num>0 && !RTEST(result)){
        rb_raise(rb_eRuntimeError,"method %s of class %s in shelter %s is duplicated",
                rb_id2name(SYM2ID(name)),
                rb_class2name(klass),
                rb_id2name(SYM2ID(node->shelter->name))
        );
    }
    return result;
}

static VALUE
lookup_hidden(VALUE klass, VALUE name, shelter_node_t *node, shelter_node_t **next_node){
    VALUE conv_name=shelter_lookup_method_table(node->shelter->hidden_method_table,klass,name);
    if(RTEST(conv_name)){
        if(next_node)*next_node=node;
        return conv_name;
    }else{
        return lookup_imports(klass,name,node,node->hidden_imports,node->hidden_num,next_node);

    }
}
static int
dump_method_table_entry(st_data_t key, st_data_t val, st_data_t arg){
    fprintf(stderr,"  %s(%lx) -> %s\n",rb_id2name(SYM2ID((VALUE)key)),key,rb_id2name(SYM2ID((VALUE)val)));
    return  ST_CONTINUE;

}
static int
dump_method_table(st_data_t key, st_data_t val, st_data_t arg){
    fprintf(stderr,"class %s(%lx)\n",rb_class2name((VALUE)key), key);
    st_foreach((st_table*)val,dump_method_table_entry,0);
    return ST_CONTINUE;
}
static VALUE
lookup_exposed(VALUE klass, VALUE name, shelter_node_t *node, shelter_node_t **next_node){
    VALUE conv_name=shelter_lookup_method_table(node->shelter->exposed_method_table,klass,name);
    if(RTEST(conv_name)){
        if(next_node)*next_node=node;
        return conv_name;
    }else{
        return lookup_imports(klass,name,node,node->exposed_imports,node->exposed_num,next_node);
    }
}
static VALUE
lookup_in_shelter_on_class(VALUE klass, VALUE name, shelter_node_t *node, shelter_node_t **next_node)
{
    VALUE conv_name=Qfalse;
    if((conv_name=lookup_hidden(klass,name,node,next_node))){
        return conv_name;
    }else{
        shelter_node_t* root=search_root(node);
        SHELTER_SEARCH_ROOT_TYPE type=search_root_type(node);
        if(type==SEARCH_ROOT_EXPOSED){
            conv_name=lookup_exposed(klass,name,root,next_node);
        }else if(type ==SEARCH_ROOT_HIDDEN){
            conv_name=lookup_hidden(klass,name,root,next_node);
        }
        if(!RTEST(conv_name) && st_lookup(RCLASS_M_TBL(klass), SYM2ID(name), NULL)){
            conv_name=name;
            *next_node=node;
        }
        return conv_name;
    }
}

static inline VALUE
lookup_in_shelter(VALUE klass, VALUE name, shelter_node_t *node, shelter_node_t **next_node){
    VALUE conv_name=Qfalse;
    VALUE cls=klass;
    while(!(conv_name=lookup_in_shelter_on_class(cls,name,node,next_node))){
        cls=RCLASS_SUPER(cls);
        if(!cls){
            *next_node=node;
            return name;
        }
    }
    /*VALUE ary=rb_ary_new();
    rb_ary_push(ary,cls);
    rb_ary_push(ary,conv_name);
    rb_ary_push(ary,(*next_node)->shelter->name);
    rb_p(ary);*/
    return conv_name;

}

static VALUE
shelter_lookup(VALUE self,VALUE name){
    shelter_node_t *next_node;
    return lookup_in_shelter(CLASS_OF(self),name, cur_node(),&next_node); 
}

static inline VALUE
shelter_search_method_name_symbol(VALUE klass, VALUE name, shelter_node_t* current_node,shelter_node_t** next_node){
    shelter_node_t* nnode=NULL;

    VALUE conv_name=lookup_in_shelter(klass,name, cur_node(),&nnode);
    /*if(GET_VM()->running&& conv_name){
        rb_p(rb_str_new2("method:"));
        rb_p(name);
        rb_p(conv_name);
    }*/

    if(next_node){*next_node=nnode;}
    return RTEST(conv_name) ? conv_name : name;
}
//extern VALUE ruby_vm_global_state_version;

shelter_cache_entry*
shelter_search_method_without_ic(ID id, VALUE klass,shelter_node_t* current_node){
    shelter_cache_entry* entry;
    shelter_cache_key key={klass,id};
    shelter_node_t* next_node;
    if(LIKELY(st_lookup(current_node->method_cache_table,(st_data_t)&key,(st_data_t*)&entry))){
        if(LIKELY(entry->vm_state == GET_VM_STATE_VERSION())){
            return entry;
        }
    }
    ID conv_id=SYM2ID(shelter_search_method_name_symbol(klass,ID2SYM(id),current_node,&next_node));
    rb_method_entry_t* me=rb_method_entry(klass,conv_id);

    shelter_cache_key* new_key;
    if(!st_get_key(current_node->method_cache_table,(st_data_t)&key,(st_data_t*)&new_key)){
        new_key=malloc(sizeof(shelter_cache_key));
        new_key->klass=klass;
        new_key->name=id;
    }
    entry=malloc(sizeof(shelter_cache_entry));
    entry->vm_state=GET_VM_STATE_VERSION();
    entry->shelter_method_id=conv_id;
    entry->me=me;
    entry->next_node=next_node;
    st_insert(current_node->method_cache_table,(st_data_t)new_key,(st_data_t)entry);
    return entry;
}


shelter_cache_entry*
shelter_method_entry(VALUE klass, ID id){
   return  shelter_search_method_without_ic(id, klass, cur_node());
}
VALUE shelter_name_of_node(shelter_node_t* node){
    return node->shelter->name;
}

void Init_Shelter(void){
  rb_define_singleton_method(rb_vm_top_self(),"shelter", define_shelter, 1);
  rb_define_singleton_method(rb_vm_top_self(),"import", import_shelter, 1);
  rb_define_singleton_method(rb_vm_top_self(),"hide", hide_shelter, 0);
  rb_define_singleton_method(rb_vm_top_self(),"expose", expose_shelter, 0);
  rb_define_method(rb_cObject,"current_node", current_node, 0);
  rb_define_method(rb_cObject,"current_shelter", cur_shelter, 0);
  rb_define_method(rb_cObject,"shelter_eval", shelter_eval, 1);
  rb_define_method(rb_cObject, "sl", shelter_lookup,1);
  rb_cShelter = rb_define_class("Shelter", rb_cObject);
  rb_cShelterNode = rb_define_class_under(rb_cShelter,"ShelterNode", rb_cObject);
  rb_define_method(rb_cShelter,"to_s",shelter_to_s,0);
  rb_define_method(rb_cShelter,"inspect",shelter_inspect,0);
  rb_undef_alloc_func(rb_cShelter);
}
