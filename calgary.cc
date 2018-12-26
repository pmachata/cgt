#include <iostream>
#include <cassert>
#include <fstream>
#include <map>
#include <set>
#include <string>
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

using namespace std::string_literals;

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
  public:
    static constexpr unsigned NODE_DEF    = 1 << 0;
    static constexpr unsigned NODE_STATIC = 1 << 1;

  private:
    tree m_dfsrc;
    std::map <tree, unsigned> m_nodes; // All functions.
    std::set <std::tuple <tree, tree>> m_edges; // (caller, callee)

  public:
    explicit callgraph (tree dfsrc)
      : m_dfsrc {dfsrc}
    {}

    callgraph ()
      : callgraph (NULL_TREE)
    {}

    void
    add_node (tree node, unsigned flags)
    {
      m_nodes[node] |= flags;
    }

    void
    add (tree dst)
    {
      add (m_dfsrc, dst);
    }

    void
    add (tree src, tree dst)
    {
      // Make sure non-function nodes are added as well.
      add_node (src, 0);
      add_node (dst, 0);

      m_edges.insert ({src, dst});

      if (!true)
        std::cerr << dump_callee (src) << " -> "
                  << dump_callee (dst) << std::endl;
    }

    static std::pair <const char *, unsigned>
    get_loc (tree decl)
    {
      return {DECL_SOURCE_FILE (decl),
              DECL_SOURCE_LINE (decl)};
    }

    static int
    process_loc (std::ostream &os, tree decl)
    {
      std::pair <const char *, unsigned> loc = get_loc (decl);
      const char *file = std::get <0> (loc);
      unsigned line = std::get <1> (loc);

      if (static const char *last_file = nullptr;
          file != last_file)
        {
          os << "F " << file << std::endl;
          last_file = file;
        }
      return line;
    }

    void
    dump_one (std::ostream &os, tree src)
    {
      unsigned lineno = process_loc (os, src);
      unsigned flags = m_nodes[src];
      os << DECL_UID (src) << " (" << lineno << ") "
         << (flags & NODE_STATIC ? "@static " : "")
         << (flags & NODE_DEF ? "" : "@decl ")
         << dump_callee (src);
    }

    void
    dump (std::ostream &os)
    {
      struct node_less
      {
        bool
        operator() (tree a, tree b) const
        {
          std::string fn_a = std::get <0> (get_loc (a));
          std::string fn_b = std::get <0> (get_loc (b));
          return std::make_tuple (fn_a, a)
               < std::make_tuple (fn_b, b);
        }
      };

      std::map <tree, std::vector <tree>, node_less> cgr;
      for (auto n: m_nodes)
        cgr[n.first] = {};
      for (auto const &a: m_edges)
        cgr[std::get <0> (a)].push_back (std::get <1> (a));

      std::map <const char *, std::vector <tree>> fmap;
      std::set <tree> seen;
      std::set <tree> unseen;
      for (auto const &c: cgr)
        {
          tree src = c.first;
          dump_one (os, src);
          seen.insert (src);
          unseen.erase (src);

          for (auto const &dst: c.second)
            {
              if (seen.find (dst) == seen.end ())
                unseen.insert (dst);
              os << ' ' << DECL_UID (dst);
            }
          os << std::endl;
        }

      for (auto src: unseen)
        {
          dump_one (os, src);
          os << std::endl;
        }
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
  walk_operand (tree t, unsigned i, callgraph &cg, unsigned level,
                bool may_be_null = false)
  {
    if (tree ti = TREE_OPERAND (t, i))
      walk (ti, cg, level + 1);
    else
      assert (may_be_null);
  }

  void
  walk_operands (tree t, callgraph &cg, unsigned level)
  {
    for (int i = 0; i < tree_operand_length (t); ++i)
      walk_operand (t, i, cg, level + 1);
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
        return walk_operands (t, cg, level);

      case COND_EXPR:
        // Operand 0 is the condition.
        walk_operand (t, 0, cg, level);
        // Operand 1 is the then-value.
        walk_operand (t, 1, cg, level);
        // Operand 2 is the else-value.
        walk_operand (t, 2, cg, level, true);
        return;

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
        // One operand, an expression for a pointer.
        return walk_operand (t, 0, cg, level);

      case COMPONENT_REF:
        // Operand 0 is the structure or union expression;
        return walk_operand (t, 0, cg, level);

      case ARRAY_REF:
        // Operand 0 is the array; operand 1 is a (single) array index.
        walk_operand (t, 0, cg, level);
        walk_operand (t, 1, cg, level);
        return;
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
  std::ofstream m_ofs;
  callgraph m_cg;

public:
  explicit calgary (std::string ofn)
    : m_ofs {ofn}
  {}

  void
  finish_parse_function (tree fn)
  {
    assert (TREE_CODE (fn) == FUNCTION_DECL);

    if (!true)
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


    unsigned flags = callgraph::NODE_DEF;
    if (!TREE_PUBLIC (fn))
      flags |= callgraph::NODE_STATIC;
    m_cg.add_node (fn, flags);

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

      case FUNCTION_DECL:
        m_cg.add_node (decl, TREE_PUBLIC (decl) ? 0 : callgraph::NODE_STATIC);
        return;

      default:
        return;
      }

    if (!true)
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
    m_cg.dump (m_ofs);
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

  std::string ofn = "/dev/stdout";
  for (int i = 0; i < plugin_info->argc; ++i)
    {
      if ("o"s == plugin_info->argv[i].key)
        ofn = plugin_info->argv[i].value;
      else
        {
          std::cerr << "Unknown argument: " << plugin_info->argv[i].key
                    << std::endl;
          return 1;
        }
    }

  calgary *cgr = new calgary (ofn);

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
