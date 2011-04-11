#ifndef RUBY_SHELTER_H
#define RUBY_SHELTER_H
ID shelter_convert_method_name(VALUE klass,ID methodname);
/*int is_in_shelter();*/
ID shelter_search_method_name(ID name, VALUE klass, void** next_node);
#endif /*RUBY_SHELTER_H*/
