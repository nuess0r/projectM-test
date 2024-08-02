#include "m_pd.h"

static t_class *tex_test_class = NULL;

typedef struct _tex_test {
  t_object x_obj;
} t_tex_test;  

void tex_test_bang(t_tex_test *x) {
  (void)x; // silence unused variable warning
  post("Hello world!");
}  

void *tex_test_new(void) {
  t_tex_test *x = (t_tex_test *)pd_new(tex_test_class);
  return (void *)x;
}

void tex_test_setup(void) {
  tex_test_class = class_new(gensym("tex_test"),
    (t_newmethod)tex_test_new, NULL,
    sizeof(t_tex_test), CLASS_DEFAULT, 0);
  class_addbang(tex_test_class, tex_test_bang);
}
