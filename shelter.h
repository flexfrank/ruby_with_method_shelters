#ifndef RUBY_SHELTER_H
#define RUBY_SHELTER_H
ID shelter_convert_method_name(VALUE klass,ID methodname);
int is_in_shelter();
VALUE search_shelter_method_name(VALUE id, VALUE klass);
#endif /*RUBY_SHELTER_H*/
