/*
  LC_ALL=C gcc -std=c99 -Wall -fpic -shared plug.c -o plug.so -iquote	\
  $HOME/gcc-4.3.0-20080428/libcpp/include -iquote $HOME/gcc-4.3.0-20080428/gcc/ \
  -iquote $HOME/gcc-build/gcc -I $HOME/gcc-4.3.0-20080428/include
*/

#include "config.h"

#undef DEBUG

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define IN_GCC
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "function.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "toplev.h"

static char const* tree_codes[] __attribute__((unused)) = {
#define DEFTREECODE(SYM, STRING, TYPE, NARGS) STRING,
#include "tree.def"
"ERROR"
#undef DEFTREECODE
};

static int tree_ar[] = {
#define DEFTREECODE(SYM, STRING, TYPE, NARGS) NARGS,
#include "tree.def"
-1
#undef DEFTREECODE
};

static FILE * output = NULL;
char const* last_file = NULL;

static int
process_loc (tree decl)
{
#ifdef DEBUG
  fprintf(stderr, "process_loc\n");
#endif
  unsigned line = 0;

  char const* file = DECL_SOURCE_FILE (decl);
  if (file != last_file)
    {
      fprintf (output, "F %s\n", file);
#ifdef DEBUG
      fprintf (stderr, "F %s\n", file);
#endif
      last_file = file;
    }
  line = DECL_SOURCE_LINE (decl);

  return line;
}

static void
dumpdecl (FILE *output, tree t)
{
  tree name_tree = DECL_ASSEMBLER_NAME (t);
#ifdef DEBUG
  fprintf(stderr, "dumpdecl ");
  fprintf(stderr, "%s\n", IDENTIFIER_POINTER (name_tree));
#endif
  fprintf (output, "%d (%d) @decl %s%s%s\n",
	   DECL_UID (t),
	   process_loc (t),
	   TREE_PUBLIC (t) ? "" : "@static ",
	   TREE_CODE (t) == VAR_DECL ? "@var " : "",
	   IDENTIFIER_POINTER (name_tree));
}

int
gcc_plugin_init (const char *file, const char *arg, char **prior_pass)
{
#ifdef DEBUG
  fprintf(stderr, "gcc_plugin_init\n");
#endif

  char const* in = main_input_filename;
  char const* filename = NULL;

  /* If we've gotten an arg, it describes mapping between input file
     names and output file names (where the graph should be
     stored).  */
  if (arg && *arg)
    {
      size_t in_len = strlen (in);
      size_t remaining = strlen (arg);
      bool gotcha = false;

      while (remaining > 1)
	{
	  char *colon = NULL;
	  size_t comp_len = strtoul (arg, &colon, 10);
	  if (colon == arg || *colon != ':'
	      || comp_len == 0 || comp_len >= remaining
	      || colon[comp_len + 1] != '=')
	    {
	    error:
	      fprintf(stderr, "warning: invalid plugin arg\n");
	      break;
	    }

	  bool skipping = comp_len != in_len;
	  colon++;
	  if (!skipping
	      && strncmp (colon, in, comp_len) == 0)
	    gotcha = true;

	  char *narg = colon + comp_len + 1;
	  remaining -= narg - arg;
	  arg = narg;

	  comp_len = strtoul (arg, &colon, 10);
	  if (colon == arg || *colon != ':'
	      || comp_len == 0 || comp_len >= remaining
	      || (colon[comp_len + 1] != ' '
		  && colon[comp_len + 1] != 0))
	    goto error;

	  colon++;
	  if (!skipping)
	    {
	      char *buf = alloca (comp_len + 1);
	      strncpy (buf, colon, comp_len);
	      buf[comp_len] = 0;
	      filename = buf;
	      break;
	    }

	  narg = colon + comp_len + 1;
	  remaining -= narg - arg;
	  arg = narg;
	}
    }

  if (filename == NULL)
    {
      static char const ext[] = ".cg";
      char const* period = strrchr (in, '.');
      char const* slash = strrchr (in, '/');
      char *buf = alloca (strlen (in) + sizeof (ext)); // ext includes traling zero

      if (period == NULL || slash > period
	  || strcmp (period, ext) == 0)
	strcpy (stpcpy (buf, in), ext);
      else
	strcpy (stpncpy (buf, in, period - in), ext);

      filename = buf;
    }

  fprintf (stderr, "Callgraph will be saved in `%s'\n", filename);
  output = fopen (filename, "w");
  fprintf (output, "F %s\n", in);
  last_file = in;

#ifdef DEBUG
  fprintf(stderr, "builtins...\n");
#endif
  for (int i = 0; i < (int) END_BUILTINS; ++i)
    if (built_in_decls[i] != NULL_TREE)
      dumpdecl (output, built_in_decls[i]);
#ifdef DEBUG
  fprintf(stderr, "builtins done...\n");
#endif

  return 0;
}

int
gcc_plugin_post_parse (void)
{
  return 0;
}

static void
dig_funcalls (tree t)
{
  if (t == NULL_TREE)
    return;

#ifdef DEBUG
  static unsigned level = 0;
  static char const spaces[] = "                                              "
    "                                                                         ";
  static char const* end = spaces + sizeof(spaces) - 1;
  char const* begin = (level > sizeof(spaces) ? begin : end - level);
  level++;
  fprintf(stderr, "%sdig_funcalls ", begin);
  fprintf(stderr, "%d ", TREE_CODE (t));
  fprintf(stderr, "%s\n", tree_codes[TREE_CODE (t)]);
#endif

  if (VL_EXP_CLASS_P (t))
    {
      int nargs = call_expr_nargs (t);
      tree args = CALL_EXPR_ARGS (t);
      tree func = TREE_OPERAND (t, 1);

      if (TREE_CODE (func) == ADDR_EXPR)
	func = TREE_OPERAND (func, 0);

      if (TREE_CODE (func) == VAR_DECL
	  || TREE_CODE (func) == PARM_DECL)
	/* This is a call through pointer.  Leave alone for now, but
	   later it would be nice to provide at least a list of
	   possibilities depending on which values are assigned to
	   this variable. */
	//fprintf (output, "  * %s\n", IDENTIFIER_POINTER (DECL_NAME (func)));
	fprintf (output, " *");
      else
	{
	  gcc_assert (TREE_CODE (func) == FUNCTION_DECL);
	  fprintf (output, " %d",
		   DECL_UID (func)/*,
		   IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (func))*/);
	}

      for (int i = 0; i < nargs; ++i)
	{
	  dig_funcalls (TREE_VALUE (args));
	  args = TREE_CHAIN (args);
	}
    }

  else if (TREE_CODE (t) == STATEMENT_LIST)
    for (tree_stmt_iterator it = tsi_start(t);
	 !tsi_end_p (it); tsi_next (&it))
      dig_funcalls (tsi_stmt (it));

  else if (COMPARISON_CLASS_P (t)
	   || UNARY_CLASS_P (t)
	   || BINARY_CLASS_P (t)
	   || EXPRESSION_CLASS_P (t)
	   || STATEMENT_CLASS_P (t))
    for (int i = 0; i < tree_ar[TREE_CODE (t)]; ++i)
      dig_funcalls (TREE_OPERAND (t, i));

  else if (TREE_CODE (t) == GIMPLE_MODIFY_STMT)
    for (int i = 0; i < tree_ar[TREE_CODE (t)]; ++i)
      dig_funcalls (GIMPLE_STMT_OPERAND (t, i));

#ifdef DEBUG
  level--;
  fprintf(stderr, "%sdone\n", begin);
#endif
}

int
gcc_plugin_pass (void)
{
#ifdef DEBUG
  fprintf(stderr, "gcc_plugin_pass ");
  fprintf(stderr, "%s\n", current_function_assembler_name ());
#endif

  tree decl = cfun->decl;
  gcc_assert(TREE_CODE (decl) == FUNCTION_DECL);

  bool is_static = !TREE_PUBLIC (decl);

  tree bind = DECL_SAVED_TREE (decl);
  tree body = BIND_EXPR_BODY (bind);
  fprintf (output, "%d (%d) %s%s",
	   DECL_UID (decl),
	   process_loc (decl),
	   is_static ? "@static " : "",
	   current_function_assembler_name ());

  dig_funcalls (body);
  fprintf (output, "\n");

  for (tree vars = BIND_EXPR_VARS (bind);
       vars != NULL_TREE; vars = TREE_CHAIN (vars))
    {
#ifdef DEBUG
      fprintf(stderr, " %p ", vars);
      fprintf(stderr, "%d ", TREE_CODE (vars));
      fprintf(stderr, "%s\n", tree_codes[TREE_CODE (vars)]);
#endif
      if (DECL_ARTIFICIAL (vars))
	{
	  continue;
	}
      else if (TREE_CODE (vars) == VAR_DECL)
	{
	  int line = process_loc (decl);
	  tree id = DECL_NAME (vars);
	  if (id == NULL_TREE)
	    continue;
	  fprintf (output, "%d (%d) @var %s%s\n",
		   DECL_UID (vars), line,
		   TREE_PUBLIC (vars) ? "" : "@static ",
		   IDENTIFIER_POINTER (id));
	}
    }

  return 0;
}

int
gcc_plugin_finish_decl (tree t)
{
#ifdef DEBUG
  fprintf(stderr, "gcc_plugin_finish_decl\n");
#endif

  bool is_static = !TREE_PUBLIC (t);
  char const* name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (t));

  dumpdecl (output, t);

  tree attr = lookup_attribute ("alias", DECL_ATTRIBUTES (t));
  if (attr != NULL_TREE)
    {
      for (; attr != NULL_TREE; attr = TREE_CHAIN (attr))
	if (TREE_VALUE (attr))
	  fprintf (output, "%d (%d) %s%s -> %s\n",
		   DECL_UID (t),
		   process_loc (t),
		   is_static ? "@static " : "",
		   name,
		   TREE_STRING_POINTER (TREE_VALUE (TREE_VALUE (attr))));
      return 0;
    }

  return 0;
}

int
gcc_plugin_finish_struct (tree t)
{
  return 0;
}

int
gcc_plugin_finish (void)
{
#ifdef DEBUG
  fprintf(stderr, "gcc_plugin_finish\n");
#endif

  fclose (output);
  return 0;
}
