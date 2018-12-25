#include <iostream>
#include <cassert>
#include <set>

#include <gcc-plugin.h>
#include <plugin-version.h>
#include <tree.h>
#include <tree-iterator.h>
#include <tree-pass.h>
#include <context.h>
#include <cgraph.h>

int plugin_is_GPL_compatible;

namespace
{
  const char *
  type_name (tree type)
  {
    assert (TYPE_P (type));
    tree t = TYPE_NAME (type);
    if (t != NULL_TREE)
      return IDENTIFIER_POINTER (t);
    return "";
  }

  const char *
  decl_name (tree decl)
  {
    tree t = DECL_NAME (decl);
    if (t != NULL_TREE)
      return IDENTIFIER_POINTER (t);
    // E.g. a nameless argument.
    return "";
  }

  const char *
  assembler_name (tree decl)
  {
    tree t = DECL_ASSEMBLER_NAME (decl);
    if (t != NULL_TREE)
      return IDENTIFIER_POINTER (t);
    return decl_name (decl);
  }

  void
  die (const char *message)
  {
    std::cerr << "XXXXXXXXXXXXXXXXXXXXXXXXXXXX" << std::endl
              << message << std::endl;
    std::exit (1);
  }
}

namespace
{
  tree
  cgr_callee (tree fn)
  {
    tree t = fn;
    if (TREE_CODE (fn) == ADDR_EXPR)
      t = TREE_OPERAND (fn, 0);

    switch (static_cast <int> (TREE_CODE (t)))
      {
      case PARM_DECL:
      case VAR_DECL:
      case FUNCTION_DECL:
        return t;

      case COMPONENT_REF:
        // Operand 1 is the field (a node of type FIELD_DECL).
        return TREE_OPERAND (t, 1);
      }

    return NULL_TREE;
  }

  void cgr_walk (tree t, unsigned level = 0);

  void
  cgr_walk_operands (tree t, unsigned level)
  {
    for (int i = 0; i < tree_operand_length (t); ++i)
      cgr_walk (TREE_OPERAND (t, i), level + 1);
  }

  const char *
  spaces (unsigned level)
  {
    static char buf[40];
    if (!*buf)
      memset (buf, ' ', (sizeof buf) - 1);
    const char *lastp = buf + (sizeof buf) - 1;
    const char *ptr = lastp - level;
    if (ptr < buf)
      return buf;
    return ptr;
  }

  void
  cgr_walk (tree t, unsigned level)
  {
    std::cerr << spaces (level)
              << get_tree_code_name (TREE_CODE (t)) << std::endl;
    switch (static_cast <int> (TREE_CODE (t)))
      {
      case RESULT_DECL:
        break;

      case CALL_EXPR:
        {
          tree callee = cgr_callee (CALL_EXPR_FN (t));
          if (callee == NULL_TREE)
            die ("unknown callee");
          if (TREE_CODE (callee) == FIELD_DECL)
            std::cerr << "CALL "
                      << type_name (DECL_CONTEXT (callee)) << "::"
                      << decl_name (callee) << std::endl;
          else
            std::cerr << "CALL "
                      << decl_name (callee) << std::endl;

          for (int i = 0, nargs = call_expr_nargs (t); i < nargs; ++i)
            cgr_walk (CALL_EXPR_ARG (t, i), level + 1);
        }
        break;

      case BIND_EXPR:
        {
          tree body = BIND_EXPR_BODY (t);
          cgr_walk (body, level + 1);
        }
        break;

      case STATEMENT_LIST:
        for (tree_stmt_iterator it = tsi_start (t); !tsi_end_p (it);
             tsi_next (&it))
          cgr_walk (tsi_stmt (it), level + 1);
        break;

      case INDIRECT_REF:
        cgr_walk_operands (t, level);
        break;

      default:
        if (CONSTANT_CLASS_P (t))
          return;

        if (DECL_P (t))
          {
            std::cerr << spaces (level) << "-decl: "
                      << decl_name (t) << std::endl;
            if (tree init = DECL_INITIAL (t))
              {
                std::cerr << spaces (level) << "-init: "
                          << get_tree_code_name (TREE_CODE (init)) << std::endl;
                if (TREE_CODE (t) != PARM_DECL)
                  cgr_walk (init, level + 1);
              }
          }
        else if (COMPARISON_CLASS_P (t)
                 || UNARY_CLASS_P (t)
                 || BINARY_CLASS_P (t)
                 || EXPRESSION_CLASS_P (t)
                 || STATEMENT_CLASS_P (t))
          cgr_walk_operands (t, level);
        else
          die ("unhandled node");
      }
  }

  void
  __cgr_finish_parse_function (tree fn)
  {
    assert (TREE_CODE (fn) == FUNCTION_DECL);

    std::cerr << "\n- fn --------\n"
              << assembler_name (fn) << "(";

    for (tree arg = DECL_ARGUMENTS (fn); arg != NULL_TREE;
         arg = TREE_CHAIN (arg))
      {
        assert (TREE_CODE (arg) == PARM_DECL);
        std::cerr << decl_name (arg)
                  << (TREE_CHAIN (arg) ? ", " : "");
      }

    std::cerr << ")" << std::endl
              << "-------------\n";

    if (tree body = DECL_SAVED_TREE (fn))
      cgr_walk (body);
  }

  void
  __cgr_finish_decl (tree decl)
  {
    assert (DECL_P (decl));

    switch (static_cast <int> (TREE_CODE (decl)))
      {
      case VAR_DECL:
        // Skip local variables.
        if (tree context = DECL_CONTEXT (decl))
          if (TREE_CODE (context) == FUNCTION_DECL)
            return;
        break;

      default:
        return;
      }

    std::cerr << "\n- decl ------\n"
              << decl_name (decl) << std::endl
              << "-------------\n";

    for (tree attr = lookup_attribute ("alias", DECL_ATTRIBUTES (decl));
         attr != NULL_TREE; attr = TREE_CHAIN (attr))
      if (TREE_VALUE (attr))
        std::cerr << "alias "
                  << TREE_STRING_POINTER (TREE_VALUE (TREE_VALUE (attr)))
                  << std::endl;
  }

  void
  __cgr_finish_type (tree type)
  {
    assert (TYPE_P (type));

    static std::set <tree> seen;
    if (seen.find (type) != seen.end ())
      return;

    std::cerr << "\n- type ------\n"
              << type_name (type) << std::endl
              << "-------------\n";

    seen.insert (type);
  }

  void
  cgr_finish_parse_function (void *gcc_data, void *user_data)
  {
    __cgr_finish_parse_function (static_cast <tree> (gcc_data));
  }

  void
  cgr_finish_decl (void *gcc_data, void *user_data)
  {
    __cgr_finish_decl (static_cast <tree> (gcc_data));
  }

  void
  cgr_finish_type (void *gcc_data, void *user_data)
  {
    __cgr_finish_type (static_cast <tree> (gcc_data));
  }
}

int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
  std::cerr << plugin_info->base_name << " init\n";

  if (!plugin_default_version_check (version, &gcc_version))
    return 1;

  {
    ::plugin_info info =
      {
        "0.1",
        "xxx plugin help",
      };
    register_callback (plugin_info->base_name, PLUGIN_INFO, nullptr, &info);
  }

  for (int i = 0; i < plugin_info->argc; ++i)
    {
      std::cerr << "Unknown argument: " << plugin_info->argv[i].key
                << std::endl;
      return 1;
    }

  register_callback (plugin_info->base_name, PLUGIN_FINISH_PARSE_FUNCTION,
                     cgr_finish_parse_function, nullptr);
  register_callback (plugin_info->base_name, PLUGIN_FINISH_DECL,
                     cgr_finish_decl, nullptr);
  register_callback (plugin_info->base_name, PLUGIN_FINISH_TYPE,
                     cgr_finish_type, nullptr);

  return 0;
}
