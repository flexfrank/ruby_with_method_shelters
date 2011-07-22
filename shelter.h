#ifndef RUBY_SHELTER_H
#define RUBY_SHELTER_H

#define SHELTER_CURRENT_NODE() ((shelter_node_t*) GET_THREAD()->cfp->shelter_node)

typedef enum{
    SHELTER_IMPORT_ROOT,
    SHELTER_IMPORT_EXPOSED,
    SHELTER_IMPORT_HIDDEN,
} SHELTER_IMPORT_TYPE;

typedef enum{
    SEARCH_ROOT_UNDEFINED=0,
    SEARCH_ROOT_EXPOSED,
    SEARCH_ROOT_HIDDEN
} SHELTER_SEARCH_ROOT_TYPE;
typedef struct shelter_node_struct shelter_node_t;
typedef struct shelter_struct{
    VALUE name;
    int hidden;
    VALUE exposed_imports;
    VALUE hidden_imports;
    st_table *exposed_method_table; /*klass->symbol->symbol*/
    st_table *hidden_method_table;  /*klass->symbol->symbol*/
    shelter_node_t* root_node;
    VALUE vm_state;
} shelter_t;

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
    char *opt_redefined_flag;
    st_table *method_cache_table;
    struct shelter_node_struct* empty_import;
};

typedef struct shelter_node_chache_entry{
    VALUE vm_state;
    ID shelter_method_id;
    rb_method_entry_t* me;
    shelter_node_t* next_node;
} shelter_cache_entry;

struct shelter_cache_hit_log{
    unsigned long lookup_count;
    unsigned long ic_hit_count;
    unsigned long cache_hit_count;
};

struct shelter_cache_hit_log shelter_total_cache_hit;
struct shelter_cache_hit_log shelter_current_cache_hit;

#define SHELTER_LOG_CACHE_HIT 0


ID shelter_convert_method_name(VALUE klass,ID methodname);
int is_in_shelter();
/*void* shelter_search_method(ID name, VALUE klass, void** next_node,IC ic);*/
shelter_cache_entry* shelter_search_method_without_ic(ID id, VALUE klass,shelter_node_t* current_node);
//can be called only from vm_shelter_search_method
shelter_cache_entry* shelter_search_method(ID id, VALUE klass,shelter_node_t* current_node);
rb_method_entry_t* shelter_tmp_lookup(VALUE klass, ID mid);
shelter_cache_entry* shelter_method_entry(VALUE klass, ID id);
VALUE shelter_name_of_node(shelter_node_t* node);
rb_method_entry_t* shelter_original_method_entry(VALUE klass, ID name);
void shelter_set_opt_redefined_flag(long bop);
void shelter_add_opt_method(rb_method_entry_t* me, long bop);
#endif /*RUBY_SHELTER_H*/
