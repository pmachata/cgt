#include <iostream>
#include <cassert>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <sstream>

#include <gcc-plugin.h>
#include <plugin-version.h>
#include <print-tree.h>
#include <stringpool.h>
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
  __attribute__ ((unused)) const char *
  tcn (tree t)
  {
    if (t == NULL_TREE)
      return "NULL_TREE";
    else
      return get_tree_code_name (TREE_CODE (t));
  }

  // Resolve typedefs and cv-qualifiers.
  tree
  find_main_type (tree type)
  {
    assert (TYPE_P (type));

    if (tree tn = TYPE_NAME (type))
      if (TREE_CODE (tn) == TYPE_DECL)
        if (tree orig = DECL_ORIGINAL_TYPE (tn))
          return find_main_type (orig);

    return TYPE_MAIN_VARIANT (type);
  }

  const char *
  type_name (tree type)
  {
    assert (TYPE_P (type));
    tree tn = TYPE_NAME (find_main_type (type));
    if (tn == NULL_TREE)
      return "";

    assert (TREE_CODE (tn) == IDENTIFIER_NODE);
    return IDENTIFIER_POINTER (tn);
  }

  const char *
  decl_name (tree decl)
  {
    assert (DECL_P (decl));
    tree t = DECL_NAME (decl);
    if (t != NULL_TREE)
      return IDENTIFIER_POINTER (t);
    // E.g. a nameless argument.
    return "";
  }

  const char *
  assembler_name (tree decl)
  {
    assert (DECL_P (decl));
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
    assert (false);
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
  class decl_fab
  {
    // (callee, arg#) -> parm_decl
    std::map <std::tuple <tree, unsigned>, tree> m_fab_decls;

  public:
    tree
    get (unsigned i, tree type, tree callee)
    {
      auto it = m_fab_decls.find ({callee, i});
      if (it == m_fab_decls.end ())
        {
          std::stringstream ss;
          ss << "(arg:" << i << ')';
          tree id = get_identifier (ss.str ().c_str ());

          location_t loc = DECL_SOURCE_LOCATION (callee);
          tree decl = build_decl (loc, PARM_DECL, id, type);
          DECL_CONTEXT (decl) = callee;

          it = m_fab_decls.insert (std::make_pair (std::make_tuple (callee, i),
                                                   decl)).first;
        }
      return it->second;
    }

  };

  class callgraph
  {
  public:
    static constexpr unsigned NODE_DEF    = 1 << 0;
    static constexpr unsigned NODE_STATIC = 1 << 1;

    class decl_fab &decl_fab;

  private:
    tree m_dfsrc;
    std::map <tree, unsigned> m_nodes; // All functions.

    // (caller, callee)
    std::set <std::tuple <tree, tree>> m_edges;

    // struct -> name
    std::map <tree, std::string> m_typedefs;

  public:
    // xxx All this looks like it could be folded to one function that
    // recursively traverses higher contexts and dumps each as necessary.
    std::string dump_rcontext (tree record)
    {
      if (std::string name = type_name (record); name != "")
        return name + "::";
      else if (auto it = m_typedefs.find (record);
               it != m_typedefs.end ())
        return std::get <1> (*it) + "::";
      else
        die ("Unknown structure name");
    }

    std::string
    dump_fcontext (tree decl)
    {
      if (decl != NULL_TREE)
        switch (static_cast <int> (TREE_CODE (decl)))
          {
          case FUNCTION_DECL:
            {
              std::string ret;
              if (tree ctx = DECL_CONTEXT (decl);
                  ctx != NULL_TREE && TREE_CODE (ctx) == FUNCTION_DECL)
                ret += dump_fcontext (ctx);
              return ret + decl_name (decl) + "()::";
            }

          case FIELD_DECL: // Fall through.
          case VAR_DECL:
            // The callee is a fabricated function argument for calls to
            // function pointers.
            return dump_rcontext (DECL_CONTEXT (decl))
                 + decl_name (decl) + "::";
          }
      return "";
    }

    std::string
    dump_callee (tree callee)
    {
      assert (DECL_P (callee));

      std::string ret = dump_fcontext (DECL_CONTEXT (callee));

      if (TREE_CODE (callee) == FIELD_DECL)
        ret += dump_rcontext (DECL_CONTEXT (callee));

      if (TREE_CODE (callee) == RESULT_DECL)
        ret += "(ret)";
      else
        ret += decl_name (callee);

      return ret;
    }

    explicit callgraph (class decl_fab &fab, tree dfsrc = NULL_TREE)
      : decl_fab {fab}
      , m_dfsrc {dfsrc}
    {}

    void
    add_node (tree node, unsigned flags)
    {
      if (!true)
        std::cerr << "add node:" << dump_callee (node) << std::endl;
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

    void
    add_typename (tree type, std::string name)
    {
      // xxx is it guaranteed that names are going to be the same between
      // compilation units?
      m_typedefs.insert ({type, name}).second;
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
          if (TREE_CODE (src) == VAR_DECL)
            {
              if (tree ctx = DECL_CONTEXT (src);
                  ctx != NULL_TREE && TREE_CODE (ctx) == FUNCTION_DECL)
                {
                  internal[src].insert (dst);
                  it = m_edges.erase (it);
                  continue;
                }
            }

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

    switch (static_cast <int> (TREE_CODE (t)))
    case POINTER_TYPE:
    case ARRAY_TYPE:
      return is_function_type (TREE_TYPE (t));

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

      case ARRAY_REF:
        // Operand 0 is the array; operand 1 is a (single) array index.
        return get_callee (TREE_OPERAND (t, 0));
      }

    std::cerr << tcn (t) << std::endl;
    die ("get_callee: unhandled code");
  }

  tree
  get_destination (tree dst)
  {
    switch (static_cast <int> (TREE_CODE (dst)))
      {
      case COMPONENT_REF:
        return TREE_OPERAND (dst, 1);
      case VAR_DECL:
      case RESULT_DECL:
        return dst;
      }

    std::cerr << tcn (dst) << std::endl;
    die ("get_destination: unhandled code");
  }

  tree
  translate_parm_decl (tree decl, callgraph &cg)
  {
    assert (TREE_CODE (decl) == PARM_DECL);

    tree ctxfn = DECL_CONTEXT (decl);
    unsigned j = 0;
    for (tree a = DECL_ARGUMENTS (ctxfn); a != NULL_TREE;
         a = TREE_CHAIN (a), j++)
      if (a == decl)
        return cg.decl_fab.get (j, TREE_TYPE (decl), ctxfn);

    die ("translate_parm_decl: Can't determine parameter number");
  }

  tree
  find_record_type (tree type)
  {
    assert (TYPE_P (type));

    switch (static_cast <int> (TREE_CODE (type)))
      {
      case RECORD_TYPE:
        return type;
      case ARRAY_TYPE:
        return find_record_type (TREE_TYPE (type));
      }

    return NULL_TREE;
  }

  void walk (tree t, callgraph &cg, unsigned level = 0);
  void walk_call_expr (tree call_expr, tree fn, callgraph &cg, unsigned level);
  void walk_initializer (tree src, tree in, callgraph &cg, unsigned level);

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
  walk_initializer_operand (tree src, tree in, unsigned i, callgraph &cg,
                            unsigned level, bool may_be_null = false)
  {
    if (tree ti = TREE_OPERAND (in, i))
      walk_initializer (src, ti, cg, level + 1);
    else
      assert (may_be_null);
  }

  void
  walk_call_expr_operand (tree call_expr, tree fn, unsigned i, callgraph &cg,
                          unsigned level, bool may_be_null = false)
  {
    if (tree ti = TREE_OPERAND (fn, i))
      walk_call_expr (call_expr, ti, cg, level + 1);
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
  walk_initializer (tree src, tree in, callgraph &cg, unsigned level)
  {
    if (!true)
      std::cerr << spaces (level) << "init:" << tcn (in) << std::endl;

    if (DECL_P (in))
      {
        if (TREE_CODE (in) == PARM_DECL)
          in = translate_parm_decl (in, cg);
        return cg.add (src, in);
      }

    switch (static_cast <int> (TREE_CODE (in)))
      {
      case ADDR_EXPR: // Fall through.
      case SAVE_EXPR: // Fall through.
      case NOP_EXPR:
        return walk_initializer (src, TREE_OPERAND (in, 0), cg, level + 1);

      case CALL_EXPR:
        if (tree fn = CALL_EXPR_FN (in))
          {
            tree callee = get_callee (fn);
            cg.add (src, DECL_RESULT (callee));
          }
        return;

      case COMPONENT_REF:
        // Operand 1 is the field (a node of type FIELD_DECL).
        {
          tree dst = TREE_OPERAND (in, 1);
          cg.add (src, dst);
        }
        return;

      case ARRAY_REF:
        // Operand 0 is the array; operand 1 is a (single) array index.
        return walk_initializer (src, TREE_OPERAND (in, 0), cg, level + 1);

      case INTEGER_CST:
        // This is likely NULL initialization.
        return;

      case COND_EXPR:
        {
          // A condition doesn't influence what the callee will be, so walk it
          // normally. Dispatch to walk_initializer for the two branches.
          walk_operand (in, 0, cg, level + 1);
          walk_initializer_operand (src, in, 1, cg, level + 1);
          walk_initializer_operand (src, in, 2, cg, level + 1, true);
        }
        return;
      }

    std::cerr << tcn (in) << std::endl;
    die ("walk_initializer: unhandled code");
  }

  void
  walk_decl (tree decl, callgraph &cg, unsigned level)
  {
    assert (DECL_P (decl));

    switch (static_cast <int> (TREE_CODE (decl)))
      {
      case TYPE_DECL:
        return;
      case FUNCTION_DECL:
        // finish_parse_function() will have already seen body of this function.
        return;
      case VAR_DECL:
        if (tree init = DECL_INITIAL (decl))
          {
            if (is_function_type (TREE_TYPE (decl)))
              walk_initializer (decl, init, cg, level);
            walk (init, cg, level + 1);
          }
        return;
      }

    std::cerr << spaces (level) << "!" << tcn (decl) << std::endl;
    die ("walk_decl: unhandled code");
  }

  void
  walk_call_expr (tree call_expr, tree fn, callgraph &cg, unsigned level)
  {
    if (!true)
      std::cerr << spaces (level) << "call:" << tcn (fn) << std::endl;
    if (TREE_CODE (fn) == COND_EXPR)
      {
        // A condition doesn't influence what the callee will be, so walk it
        // normally. Dispatch to walk_call_expr for the then and else branches.
        walk_operand (fn, 0, cg, level + 1);
        walk_call_expr_operand (call_expr, fn, 1, cg, level + 1);
        walk_call_expr_operand (call_expr, fn, 2, cg, level + 1, true);
        return;
      }

    tree callee = get_callee (fn);
    assert (callee != NULL_TREE);

    cg.add (TREE_CODE (callee) != PARM_DECL ? callee
            : translate_parm_decl (callee, cg));

    // Types of callee arguments.
    std::vector <tree> callee_arg_types;
    if (TREE_CODE (callee) == FUNCTION_DECL)
      for (tree a = DECL_ARGUMENTS (callee); a; a = TREE_CHAIN (a))
        callee_arg_types.push_back (TREE_TYPE (a));
    else
      {
        assert (DECL_P (callee));

        tree type = TREE_TYPE (callee);
        if (TREE_CODE (type) == POINTER_TYPE)
          type = TREE_TYPE (type);
        assert (TREE_CODE (type) == FUNCTION_TYPE);

        for (tree a = TYPE_ARG_TYPES (type); a != NULL_TREE; a = TREE_CHAIN (a))
          callee_arg_types.push_back (TREE_VALUE (a));
      }

    for (unsigned i = 0, nargs = call_expr_nargs (call_expr);
         i < nargs; ++i)
      {
        tree arg = CALL_EXPR_ARG (call_expr, i);

        // Vararg functions have fewer formal arguments than actual ones.
        // xxx The useful thing to do in that case would be something like:
        //       foo()::<varargs> -> some_func
        // And then when foo ends up calling through varargs, do:
        //       foo -> foo()::<varargs>
        // etc. for assignment or parameter passing.
        if (i < callee_arg_types.size ())
          {
            tree callee_arg_type = callee_arg_types[i];
            if (is_function_type (callee_arg_type)
                || is_function_type (TREE_TYPE (arg)))
              walk_initializer (cg.decl_fab.get (i, callee_arg_type, callee),
                                arg, cg, level);
          }

        walk (arg, cg, level + 1);
      }
  }

  void
  walk_function_type (tree index, tree value, callgraph &cg, unsigned level)
  {
    if (TREE_CODE (value) == CONSTRUCTOR)
      {
        // Constructor of an array of function type.
        for (unsigned i = 0; i < CONSTRUCTOR_NELTS (value); ++i)
          {
            constructor_elt *elt = CONSTRUCTOR_ELT (value, i);
            walk_initializer (index, elt->value, cg, level);
          }
      }
    else
      walk_initializer (index, value, cg, level);
  }

  void
  walk (tree t, callgraph &cg, unsigned level)
  {
    if (!true)
      std::cerr << spaces (level) << tcn (t) << std::endl;

    // Declarations are processed in __finish_decl.
    if (DECL_P (t))
      return;

    if (CONSTANT_CLASS_P (t))
      return;

    switch (static_cast <int> (TREE_CODE (t)))
      {
      case CALL_EXPR:
        if (tree fn = CALL_EXPR_FN (t))
          walk_call_expr (t, fn, cg, level + 1);
        return;

      case DECL_EXPR:
        return walk_decl (DECL_EXPR_DECL (t), cg, level);

      case MODIFY_EXPR:
        {
          tree dst = TREE_OPERAND (t, 0);
          if (is_function_type (TREE_TYPE (dst)))
            {
              tree val = TREE_OPERAND (t, 1);
              if (tree dst2 = get_destination (dst))
                walk_initializer (dst2, val, cg, level);
            }
        }
        return walk_operands (t, cg, level);

      case COND_EXPR:
        // Operand 0 is the condition.
        // Operand 1 is the then-value.
        // Operand 2 is the else-value.
        walk_operand (t, 0, cg, level);
        walk_operand (t, 1, cg, level);
        walk_operand (t, 2, cg, level, true);
        return;

      case SWITCH_EXPR:
        // Operand 0 is the expression used to perform the branch,
        // Operand 1 is the body of the switch. It may also be NULL.
        // Operand 2 is either NULL_TREE or a TREE_VEC of the CASE_LABEL_EXPRs
        walk_operand (t, 0, cg, level);
        walk_operand (t, 1, cg, level, true);
        walk_operand (t, 2, cg, level, true);
        return;

      case CASE_LABEL_EXPR:
        // Operand 0 is CASE_LOW. It may be NULL_TREE
        // Operand 1 is CASE_HIGH.  If it is NULL_TREE [...]
        // Operand 2 is CASE_LABEL, which is is the corresponding LABEL_DECL.
        // Operand 3 is CASE_CHAIN. This operand is only used in tree-cfg.c
        walk_operand (t, 0, cg, level, true);
        walk_operand (t, 1, cg, level, true);
        return;

      case TARGET_EXPR:
        // operand 1 is the initializer for the target, which may be void
        // operand 2 is the cleanup for this node, if any.
        // operand 3 is the saved initializer after this node has been expanded
        walk_operand (t, 0, cg, level, true);
        walk_operand (t, 1, cg, level, true);
        walk_operand (t, 2, cg, level, true);
        return;

      case ASM_EXPR:
        for (tree list = ASM_OUTPUTS (t); list != NULL_TREE;
             list = TREE_CHAIN (list))
          walk (TREE_VALUE (list), cg, level + 1);
        for (tree list = ASM_INPUTS (t); list != NULL_TREE;
             list = TREE_CHAIN (list))
          walk (TREE_VALUE (list), cg, level + 1);
        for (tree list = ASM_CLOBBERS (t); list != NULL_TREE;
             list = TREE_CHAIN (list))
          walk (TREE_VALUE (list), cg, level + 1);
        return;

      case RETURN_EXPR:
        // RETURN.  Evaluates operand 0, then returns from the current function.
        // The operand may be null.
        return walk_operand (t, 0, cg, level, true);

      case CONSTRUCTOR:
        for (unsigned i = 0; i < CONSTRUCTOR_NELTS (t); ++i)
          {
            constructor_elt *elt = CONSTRUCTOR_ELT (t, i);
            tree type = TREE_TYPE (elt->index);
            if (is_function_type (type))
              walk_function_type (elt->index, elt->value, cg, level);

            walk (elt->index, cg, level + 1);
            walk (elt->value, cg, level + 1);
          }
        return;

      case BIND_EXPR:
        {
          tree body = BIND_EXPR_BODY (t);
          return walk (body, cg, level + 1);
        }

      case IMAGPART_EXPR: // Fall through.
      case REALPART_EXPR:
        return walk_operand (t, 0, cg, level);

      case STATEMENT_LIST:
        for (tree_stmt_iterator it = tsi_start (t); !tsi_end_p (it);
             tsi_next (&it))
          walk (tsi_stmt (it), cg, level + 1);
        return;

      case INDIRECT_REF:
        // One operand, an expression for a pointer.
        return walk_operand (t, 0, cg, level);

      case COMPONENT_REF: // Fall through.
      case BIT_FIELD_REF:
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
  decl_fab m_fab;
  callgraph m_cg;

public:
  explicit calgary (std::string ofn)
    : m_ofs {ofn}
    , m_fab {}
    , m_cg {m_fab}
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
        callgraph cg {m_fab, fn};
        walk (body, cg);
        cg.propagate ();
        m_cg.merge (std::move (cg));
      }
  }

  void
  process_aliases (tree decl)
  {
    for (tree attr = DECL_ATTRIBUTES (decl);
         (attr = lookup_attribute ("alias", attr));
         attr = TREE_CHAIN (attr))
      {
        tree arg = TREE_VALUE (attr);
        tree ident = TREE_PURPOSE (attr);
        const char *name = TREE_STRING_POINTER (TREE_VALUE (arg));
        assert (TREE_PURPOSE (arg) == NULL_TREE);
        assert (TREE_CHAIN (arg) == NULL_TREE);
        assert (*TREE_STRING_POINTER (ident) == 0);

        m_cg.dump_one (m_ofs, decl);
        m_ofs << " -> " << name << std::endl;
      }
  }

  void
  finish_decl (tree decl)
  {
    assert (DECL_P (decl));

    switch (static_cast <int> (TREE_CODE (decl)))
      {
      case VAR_DECL:
        // A variable name may be used as an identifier of an anonymous
        // structure as well.
        if (tree type = find_record_type (TREE_TYPE (decl)))
          if (type_name (type) == ""s)
            {
              // When a variable is used as a context (whether it's global or
              // local), we need to mangle it, so that a type:: doesn't clash
              // with a var::.
              //
              // Local variables on their own never get to the final call graph,
              // so their name is irrelevant. For global variables, it's better
              // if the name stays unmangled. Therefore just mangle here,
              // instead of doing it in dump_callee.
              std::string prefix;
              if (tree context = DECL_CONTEXT (decl))
                prefix = m_cg.dump_fcontext (context);
              m_cg.add_typename (find_main_type (type),
                                 prefix + "."s + decl_name (decl));
            }

        // Local variables are processed when walking the function body.
        if (tree context = DECL_CONTEXT (decl))
          if (TREE_CODE (context) == FUNCTION_DECL)
            return;
        break;

      case FUNCTION_DECL:
        m_cg.add_node (decl, TREE_PUBLIC (decl) ? 0 : callgraph::NODE_STATIC);
        process_aliases (decl);
        return;

      case TYPE_DECL:
        if (tree type = DECL_ORIGINAL_TYPE (decl))
          m_cg.add_typename (find_main_type (type), decl_name (decl));
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
        callgraph cg {m_fab, decl};
        walk (init, cg);
        cg.propagate ();
        m_cg.merge (std::move (cg));
      }
    process_aliases (decl);
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
      const char *arg = plugin_info->argv[i].key;
      if (arg == "o"s)
        ofn = plugin_info->argv[i].value;
      else if (arg == "opat"s)
        {
          // Substitution references like in make, e.g.: %.c:%.cg. Note we use a
          // ":" to separate the pattern from the substitution, because gcc
          // screws up passing arguments that contain a "=".
          std::string val = plugin_info->argv[i].value;
          std::string pat;
          std::string subst;

          {
            auto pos = val.find (':');
            if (pos == std::string::npos)
              {
                std::cerr << "Pattern '" << val << "' without substitution.\n"
                          << "Example usage: -fplugin-arg-calgary-opat=%.c=%.cg\n";
                return 1;
              }
            pat = val.substr (0, pos);
            subst = val.substr (pos + 1);
          }

          if (pat.find ('%') == std::string::npos)
            pat = "%"s + pat;
          if (subst.find ('%') == std::string::npos)
            subst = "%"s + subst;

          std::string stem;
          {
            auto pos = pat.find ('%');
            auto pfx = pat.substr (0, pos);
            auto sfx = pat.substr (pos + 1);
            std::string ifn = main_input_filename;
            if (pfx.length () >= ifn.length ()
                || sfx.length () >= ifn.length ()
                || (pfx.length () + sfx.length ()) > ifn.length ()
                || pfx != ifn.substr (0, pfx.length ())
                || sfx != ifn.substr (ifn.length () - sfx.length ()))
              {
                std::cerr << "Pattern '" << pat
                          << "' doesn't match the input file name '" << ifn
                          << std::endl;
                return 1;
              }

            size_t len = ifn.length () - pfx.length () - sfx.length ();
            stem = ifn.substr (pfx.length (), len);
          }

          auto pos = subst.find ('%');
          auto pfx = subst.substr (0, pos);
          auto sfx = subst.substr (pos + 1);
          ofn = pfx + stem + sfx;
        }
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
