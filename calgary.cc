#include <iostream>
#include <cassert>
#include <map>
#include <set>
#include <tuple>
#include <vector>

#include <gcc-plugin.h>
#include <plugin-version.h>
#include <tree.h>
#include <tree-iterator.h>
#include <tree-pass.h>
#include <tree-pretty-print.h>
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

  void __attribute__ ((noreturn))
  die (const char *message)
  {
    std::cerr << "XXXXXXXXXXXXXXXXXXXXXXXXXXXX" << std::endl
              << message << std::endl;
    std::exit (1);
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
}

namespace
{
  std::string
  dump_callee (tree callee)
  {
    assert (DECL_P (callee));

    std::string ret;
    if (TREE_CODE (callee) == FIELD_DECL)
      ret = std::string (type_name (DECL_CONTEXT (callee))) + "::";
    else if (TREE_CODE (callee) == PARM_DECL
             || TREE_CODE (callee) == RESULT_DECL)
      ret = std::string (decl_name (DECL_CONTEXT (callee))) + "()::";

    if (TREE_CODE (callee) == RESULT_DECL)
      ret += "<ret>";
    else
      ret += decl_name (callee);

    return ret;
  }

  class callgraph
  {
    tree m_dfsrc;
    std::set <std::tuple <tree, tree>> m_edges; // (caller, callee)

  public:
    explicit callgraph (tree dfsrc)
      : m_dfsrc {dfsrc}
    {}

    callgraph ()
      : callgraph (NULL_TREE)
    {}

    void
    add (tree dst)
    {
      add (m_dfsrc, dst);
    }

    void
    add (tree src, tree dst)
    {
      if (false)
        std::cerr << dump_callee (src) << " -> "
                  << dump_callee (dst) << std::endl;
      m_edges.insert ({src, dst});
    }

    int
    process_loc (tree decl)
    {
      unsigned line = 0;

      static const char *last_file = nullptr;
      char const *file = DECL_SOURCE_FILE (decl);
      if (file != last_file)
        {
          fprintf (stderr, "F %s\n", file);
          last_file = file;
        }
      line = DECL_SOURCE_LINE (decl);

      return line;
    }

    void
    dump ()
    {
      std::map <tree, std::vector <tree>> cgr;
      for (auto const &a: m_edges)
        cgr[std::get <0> (a)].push_back (std::get <1> (a));

      std::set <tree> seen;
      std::set <tree> unseen;
      for (auto const &c: cgr)
        {
          tree src = c.first;

          fprintf (stderr, "%d (%d) %s",
                   DECL_UID (src),
                   process_loc (src),
                   dump_callee (src).c_str ());
          seen.insert (src);
          unseen.erase (src);

          for (auto const &dst: c.second)
            {
              if (seen.find (dst) == seen.end ())
                unseen.insert (dst);
              fprintf (stderr, " %d", DECL_UID (dst));
            }
          fprintf (stderr, "\n");
        }

      for (auto src: unseen)
        fprintf (stderr, "%d (%d) @decl %s\n",
                 DECL_UID (src),
                 process_loc (src),
                 dump_callee (src).c_str ());
    }

    void
    propagate ()
    {
      // Get rid of the nodes representing function-local variables and
      // propagate the information to the periphery of the function.
      std::map <tree, std::set <tree>> internal;
      for (auto it = m_edges.begin (); it != m_edges.end (); )
        {
          tree src = std::get <0> (*it);
          tree dst = std::get <1> (*it);
          if (TREE_CODE (src) == VAR_DECL
              && TREE_CODE (DECL_CONTEXT (src)) == FUNCTION_DECL)
            {
              internal[src].insert (dst);
              it = m_edges.erase (it);
            }
          else
            ++it;
        }

      bool changed;
      do {
        changed = false;
        for (auto it = m_edges.begin (); it != m_edges.end (); )
          {
            tree src = std::get <0> (*it);
            tree dst = std::get <1> (*it);
            auto jt = internal.find (dst);
            if (jt != internal.end ())
              {
                it = m_edges.erase (it);
                for (auto dst2: jt->second)
                  m_edges.insert ({src, dst2});
                changed = true;
              }
            else
              ++it;
          }
      } while (changed);
    }

    void
    merge (callgraph &&other)
    {
      m_edges.merge (std::move (other.m_edges));
    }
  };

  bool
  is_function_type (tree t)
  {
    assert (TYPE_P (t));

    // xxx cv-qual
    if (TREE_CODE (t) == POINTER_TYPE)
      t = TREE_TYPE (t);

    return TREE_CODE (t) == FUNCTION_TYPE;
  }

  tree
  get_callee (tree fn)
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

    std::cerr << get_tree_code_name (TREE_CODE (t)) << std::endl;
    die ("get_callee: unhandled code");
  }

  tree
  get_initializer (tree in)
  {
    while (true)
      {
        if (TREE_CODE (in) == ADDR_EXPR
            || TREE_CODE (in) == NOP_EXPR)
          in = TREE_OPERAND (in, 0);
        else
          break;
      }

    if (DECL_P (in))
      return in;

    if (TREE_CODE (in) == CALL_EXPR)
      {
        tree callee = get_callee (CALL_EXPR_FN (in));
        return DECL_RESULT (callee);
      }

    std::cerr << get_tree_code_name (TREE_CODE (in)) << std::endl;
    die ("get_initializer: unhandled code");
  }

  void walk (tree t, callgraph &cg, unsigned level = 0);

  void
  walk_operands (tree t, callgraph &cg, unsigned level)
  {
    for (int i = 0; i < tree_operand_length (t); ++i)
      walk (TREE_OPERAND (t, i), cg, level + 1);
  }

  void
  walk (tree t, callgraph &cg, unsigned level)
  {
    if (!true)
      std::cerr << spaces (level)
                << get_tree_code_name (TREE_CODE (t)) << std::endl;

    // Declarations are processed in __finish_decl.
    if (DECL_P (t))
      return;

    if (CONSTANT_CLASS_P (t))
      return;

    switch (static_cast <int> (TREE_CODE (t)))
      {
      case CALL_EXPR:
        {
          tree callee = get_callee (CALL_EXPR_FN (t));
          cg.add (callee);

          // Arguments of the callee.
          std::vector <tree> callee_args;
          if (TREE_CODE (callee) == FUNCTION_DECL)
            for (tree a = DECL_ARGUMENTS (callee); a; a = TREE_CHAIN (a))
              callee_args.push_back (a);
          else
            {
              tree callee_type = TREE_TYPE (callee);
              assert (TREE_CODE (callee_type) == FUNCTION_TYPE);
              tree args = TYPE_ARG_TYPES (callee_type);
              // xxx this is TREE_NULL :-(
              for (tree a = args; a; a = TREE_CHAIN (a))
                callee_args.push_back (TREE_VALUE (a));
            }

          for (unsigned i = 0, nargs = call_expr_nargs (t); i < nargs; ++i)
            {
              tree arg = CALL_EXPR_ARG (t, i);
              tree callee_arg = (i < callee_args.size ())
                ? callee_args[i] : NULL_TREE;
              if (callee_arg && (is_function_type (TREE_TYPE (callee_arg))
                                 || is_function_type (TREE_TYPE (arg))))
                if (tree callee = get_initializer (arg))
                  cg.add (callee_arg, callee);

              walk (arg, cg, level + 1);
            }
        }
        return;

      case DECL_EXPR:
        {
          tree decl = DECL_EXPR_DECL (t);
          assert (TREE_CODE (decl) == VAR_DECL);
          tree init = DECL_INITIAL (decl);
          if (init)
            {
              if (is_function_type (TREE_TYPE (decl)))
                if (tree callee = get_initializer (init))
                  cg.add (decl, callee);
              walk (init, cg, level + 1);
            }
        }
        return;

      case MODIFY_EXPR:
        {
          tree dst = TREE_OPERAND (t, 0);
          if (is_function_type (TREE_TYPE (dst)))
            {
              tree val = TREE_OPERAND (t, 1);
              if (tree callee = get_initializer (val))
                cg.add (dst, callee);
            }
        }
        break;

      case CONSTRUCTOR:
        for (unsigned i = 0; i < CONSTRUCTOR_NELTS (t); ++i)
          {
            constructor_elt *elt = CONSTRUCTOR_ELT (t, i);
            if (is_function_type (TREE_TYPE (elt->index)))
              if (tree callee = get_initializer (elt->value))
                cg.add (elt->index, callee);

            walk (elt->index, cg, level + 1);
            walk (elt->value, cg, level + 1);
          }
        return;

      case BIND_EXPR:
        {
          tree body = BIND_EXPR_BODY (t);
          return walk (body, cg, level + 1);
        }

      case STATEMENT_LIST:
        for (tree_stmt_iterator it = tsi_start (t); !tsi_end_p (it);
             tsi_next (&it))
          walk (tsi_stmt (it), cg, level + 1);
        return;

      case INDIRECT_REF:
        return walk_operands (t, cg, level);
      }

    if (COMPARISON_CLASS_P (t)
        || UNARY_CLASS_P (t)
        || BINARY_CLASS_P (t)
        || EXPRESSION_CLASS_P (t)
        || STATEMENT_CLASS_P (t))
      return walk_operands (t, cg, level);

    die ("unhandled node");
  }
}

class calgary
{
  struct callgraph m_cg;

public:
  void
  finish_parse_function (tree fn)
  {
    assert (TREE_CODE (fn) == FUNCTION_DECL);

    if (false)
      {
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
      }

    if (tree body = DECL_SAVED_TREE (fn))
      {
        callgraph cg {fn};
        walk (body, cg);
        cg.propagate ();
        m_cg.merge (std::move (cg));
      }
  }

  void
  finish_decl (tree decl)
  {
    assert (DECL_P (decl));

    for (tree attr = lookup_attribute ("alias", DECL_ATTRIBUTES (decl));
         attr != NULL_TREE; attr = TREE_CHAIN (attr))
      if (TREE_VALUE (attr))
        std::cerr << "alias "
                  << TREE_STRING_POINTER (TREE_VALUE (TREE_VALUE (attr)))
                  << std::endl;

    switch (static_cast <int> (TREE_CODE (decl)))
      {
      case VAR_DECL:
        // Local variables are processed when walking the function body.
        if (tree context = DECL_CONTEXT (decl))
          if (TREE_CODE (context) == FUNCTION_DECL)
            return;
        break;

      default:
        return;
      }

    if (false)
      std::cerr << "\n- decl ------\n"
                << decl_name (decl) << std::endl
                << "-------------\n";

    if (tree init = DECL_INITIAL (decl))
      {
        callgraph cg {decl};
        walk (init, cg);
        cg.propagate ();
        m_cg.merge (std::move (cg));
      }
  }

  void
  finish_type (tree type)
  {
    assert (TYPE_P (type));
    return; // xxx drop this altogether

    static std::set <tree> seen;
    if (seen.find (type) != seen.end ())
      return;

    std::cerr << "\n- type ------\n"
              << type_name (type) << std::endl
              << "-------------\n";

    seen.insert (type);
  }

  void
  dump ()
  {
    m_cg.dump ();
  }
};

namespace
{
  void
  finish_parse_function (void *gcc_data, void *user_data)
  {
    auto fn = static_cast <tree> (gcc_data);
    auto cgr = static_cast <calgary *> (user_data);
    cgr->finish_parse_function (fn);
  }

  void
  finish_decl (void *gcc_data, void *user_data)
  {
    auto decl = static_cast <tree> (gcc_data);
    auto cgr = static_cast <calgary *> (user_data);
    cgr->finish_decl (decl);
  }

  void
  finish_type (void *gcc_data, void *user_data)
  {
    auto type = static_cast <tree> (gcc_data);
    auto cgr = static_cast <calgary *> (user_data);
    cgr->finish_type (type);
  }

  void
  finish (void *gcc_data, void *user_data)
  {
    auto cgr = static_cast <calgary *> (user_data);
    cgr->dump ();
    delete cgr;
  }
}

int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
  //std::cerr << plugin_info->base_name << " init\n";

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

  calgary *cgr = new calgary;

  register_callback (plugin_info->base_name, PLUGIN_FINISH_PARSE_FUNCTION,
                     finish_parse_function, cgr);
  register_callback (plugin_info->base_name, PLUGIN_FINISH_DECL,
                     finish_decl, cgr);
  register_callback (plugin_info->base_name, PLUGIN_FINISH_TYPE,
                     finish_type, cgr);
  register_callback (plugin_info->base_name, PLUGIN_FINISH,
                     finish, cgr);

  return 0;
}
