#ifndef RUBY_SHELTER_H
#define RUBY_SHELTER_H

#define SHELTER_CURRENT_NODE() ((shelter_node_t*) GET_THREAD()->cfp->shelter_node)
typedef struct shelter_node_struct shelter_node_t;
typedef struct shelter_node_chache_entry{
    VALUE vm_state;
    VALUE shelter_method_name;
    rb_method_entry_t* me;
    shelter_node_t* next_node;
} shelter_cache_entry;
ID shelter_convert_method_name(VALUE klass,ID methodname);
/*int is_in_shelter();*/
/*void* shelter_search_method(ID name, VALUE klass, void** next_node,IC ic);*/
shelter_cache_entry* shelter_search_method_without_ic(ID id, VALUE klass,shelter_node_t* current_node);
#endif /*RUBY_SHELTER_H*/
