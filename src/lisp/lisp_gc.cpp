/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com or
 *  Jonathan Clark.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "lisp.h"
#ifdef NO_LIBS
#include "fakelib.h"
#else
#include "macs.h"
#endif

#include "stack.h"

/*  Lisp garbage collection: uses copy/free algorithm
    Places to check:
      symbol
        values
    functions
    names
      stack
*/

// Stack where user programs can push data and have it GCed
grow_stack<void> l_user_stack(150);
// Stack of user pointers
grow_stack<void *> l_ptr_stack(1500);

size_t reg_ptr_total = 0;
size_t reg_ptr_list_size = 0;
void ***reg_ptr_list = NULL;

static uint8_t *cstart, *cend, *collected_start, *collected_end;

static void dump_memory(void *mem, int before, int after)
{
  uint8_t *p = (uint8_t *)mem;

  fprintf(stderr, "dumping memory around %p:\n", p);
  for (int i = -before; i < after; i++)
  {
    if (!(i & 15))
      fprintf(stderr, "%p: ", p + i);
    fprintf(stderr, "%c%02x%c", i ? ' ' : '[', p[i], i ? ' ' : ']');
    if (!((i + 1) & 15))
      fprintf(stderr, "\n");
  }
}

void register_pointer(void **addr)
{
  if (reg_ptr_total >= reg_ptr_list_size)
  {
    reg_ptr_list_size += 0x100;
    reg_ptr_list = (void ***)realloc(reg_ptr_list, sizeof(void **) * reg_ptr_list_size);
  }
  reg_ptr_list[reg_ptr_total++] = addr;
}

void unregister_pointer(void **addr)
{
  void ***reg_on = reg_ptr_list;
  for (size_t i = 0; i < reg_ptr_total; i++, reg_on++)
  {
    if (*reg_on == addr)
    {
      reg_ptr_total--;
      for (size_t j = i; j < reg_ptr_total; j++, reg_on++)
        reg_on[0] = reg_on[1];
      return ;
    }
  }
  fprintf(stderr, "Unable to locate ptr to unregister");
}

static void *collect_object(void *x);
static void *collect_array(void *x)
{
    long s = ((LispArray *)x)->size;
    LispArray *a = LispArray::Create(s, NULL);
    LispObject **src = ((LispArray *)x)->GetData();
    LispObject **dst = a->GetData();
    for (int i = 0; i < s; i++)
        dst[i] = (LispObject *)collect_object(src[i]);

    return a;
}

inline void *collect_cons_cell(void *x)
{
  LispList *last = NULL, *first = NULL;
  if (!x) return x;
  for (; x && item_type(x) == L_CONS_CELL; )
  {
    LispList *p = new_cons_cell();
    void *old_car = ((LispList *)x)->car;
    void *old_cdr = ((LispList *)x)->cdr;
    void *old_x = x;
    x = CDR(x);
    ((LispRedirect *)old_x)->type = L_COLLECTED_OBJECT;
    ((LispRedirect *)old_x)->new_reference = p;

    p->car = collect_object(old_car);
    p->cdr = collect_object(old_cdr);
    
    if (last) last->cdr = p;
    else first = p;
    last = p;
  }
  if (x)
    last->cdr = collect_object(x);
  return first;                    // we already set the collection pointers
}

static void *collect_object(void *x)
{
  void *ret = x;

  if (((uint8_t *)x) >= cstart && ((uint8_t *)x) < cend)
  {
    //dump_memory(x, 32, 48);
    switch (item_type(x))
    {
      case L_BAD_CELL:
        lbreak("error: GC corrupted cell\n");
        break;
      case L_NUMBER:
        ret = new_lisp_number(((LispNumber *)x)->num);
        break;
      case L_SYS_FUNCTION:
        ret = new_lisp_sys_function(((LispSysFunction *)x)->min_args,
                                    ((LispSysFunction *)x)->max_args,
                                    ((LispSysFunction *)x)->fun_number);
        break;
      case L_USER_FUNCTION:
#ifndef NO_LIBS
        ret = new_lisp_user_function(((LispUserFunction *)x)->alist,
                                     ((LispUserFunction *)x)->blist);

#else
        {
          void *arg = collect_object(((LispUserFunction *)x)->arg_list);
          void *block = collect_object(((LispUserFunction *)x)->block_list);
          ret = new_lisp_user_function(arg, block);
        }
#endif
        break;
      case L_STRING:
        ret = new_lisp_string(lstring_value(x));
        break;
      case L_CHARACTER:
        ret = new_lisp_character(lcharacter_value(x));
        break;
      case L_C_FUNCTION:
        ret = new_lisp_c_function(((LispSysFunction *)x)->min_args,
                                  ((LispSysFunction *)x)->max_args,
                                  ((LispSysFunction *)x)->fun_number);
        break;
      case L_C_BOOL:
        ret = new_lisp_c_bool(((LispSysFunction *)x)->min_args,
                              ((LispSysFunction *)x)->max_args,
                              ((LispSysFunction *)x)->fun_number);
        break;
      case L_L_FUNCTION:
        ret = new_user_lisp_function(((LispSysFunction *)x)->min_args,
                                     ((LispSysFunction *)x)->max_args,
                                     ((LispSysFunction *)x)->fun_number);
        break;
      case L_POINTER:
        ret = new_lisp_pointer(lpointer_value(x));
        break;
      case L_1D_ARRAY:
        ret = collect_array(x);
        break;
      case L_FIXED_POINT:
        ret = new_lisp_fixed_point(lfixed_point_value(x));
        break;
      case L_CONS_CELL:
        ret = collect_cons_cell((LispList *)x);
        break;
      case L_OBJECT_VAR:
        ret = new_lisp_object_var(((LispObjectVar *)x)->number);
        break;
      case L_COLLECTED_OBJECT:
        ret = ((LispRedirect *)x)->new_reference;
        break;
      default:
        dump_memory(x, 8, 196);
        //*(char *)NULL = 0;
        lbreak("shouldn't happen. collecting bad object 0x%x\n",
               item_type(x));
        break;
    }
    ((LispRedirect *)x)->type = L_COLLECTED_OBJECT;
    ((LispRedirect *)x)->new_reference = ret;
  }
  else if ((uint8_t *)x < collected_start || (uint8_t *)x >= collected_end)
  {
    if (item_type(x) == L_CONS_CELL) // still need to remap cons_cells outside of space
    {
      for (; x && item_type(x) == L_CONS_CELL; x = CDR(x))
        ((LispList *)x)->car = collect_object(((LispList *)x)->car);
      if (x)
        ((LispList *)x)->cdr = collect_object(((LispList *)x)->cdr);
    }
  }

  return ret;
}

static void collect_symbols(LispSymbol *root)
{
  if (root)
  {
    root->value = collect_object(root->value);
    root->function = collect_object(root->function);
    root->name = (LispString *)collect_object(root->name);
    collect_symbols(root->left);
    collect_symbols(root->right);
  }
}

static void collect_stacks()
{
  long t = l_user_stack.son;

  void **d = l_user_stack.sdata;
  for (int i = 0; i < t; i++, d++)
    *d = collect_object(*d);

  t = l_ptr_stack.son;
  void ***d2 = l_ptr_stack.sdata;
  for (int i = 0; i < t; i++, d2++)
  {
    void **ptr = *d2;
    *ptr = collect_object(*ptr);
  }

  d2 = reg_ptr_list;
  for (size_t i = 0; i < reg_ptr_total; i++, d2++)
  {
    void **ptr = *d2;
    *ptr = collect_object(*ptr);
  }
}

void collect_space(int which_space) // should be tmp or permanent
{
  return; /* XXX */

  int old_space = current_space;
  cstart = space[which_space];
  cend = free_space[which_space];

  space_size[GC_SPACE] = space_size[which_space];
  uint8_t *new_space = (uint8_t *)malloc(space_size[GC_SPACE]);
  current_space = GC_SPACE;
  free_space[GC_SPACE] = space[GC_SPACE] = new_space;

  collected_start = new_space;
  collected_end = new_space + space_size[GC_SPACE];

//dump_memory((char *)lsym_root->name, 128, 196);
//dump_memory((char *)0xb6782025, 32, 48);
  collect_symbols(lsym_root);
  collect_stacks();

  // for debuging clear it out
  memset(space[which_space], 0, space_size[which_space]);
  free(space[which_space]);

  space[which_space] = new_space;
  free_space[which_space] = new_space
                          + (free_space[GC_SPACE] - space[GC_SPACE]);
  current_space = old_space;
}

