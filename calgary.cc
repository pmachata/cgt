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
#include <output.h>

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

    // Builtins may have a DECL_ORIGINAL_TYPE of NULL_TREE and can't thus be
    // fully resolved. Therefore, tolerate TYPE_DECL names here.
    if (tn != NULL_TREE && TREE_CODE (tn) == TYPE_DECL)
      tn = DECL_NAME (tn);

    if (tn == NULL_TREE)
      return "";

    assert (TREE_CODE (tn) == IDENTIFIER_NODE);
    return IDENTIFIER_POINTER (tn);
  }

  bool
  no_type_name (tree type)
  {
    return type_name (type)[0] == '\0';
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

    // callee -> result_decl
    std::map <tree, tree> m_fab_results;

    // decl -> (callee, arg#)
    // For result decls, arg# is -1u.
    std::map <tree, std::tuple <tree, unsigned>> m_fab_ctx;

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

          auto ctx = std::make_tuple (callee, i);
          it = m_fab_decls.insert (std::make_pair (ctx, decl)).first;
          m_fab_ctx.insert (std::make_pair (decl, ctx));
        }
      return it->second;
    }

    void
    track_result_decl (tree callee, tree resdecl)
    {
      assert (callee != NULL_TREE);
      assert (resdecl != NULL_TREE);
      m_fab_ctx.insert (std::make_pair (resdecl,
                                        std::make_tuple (callee, -1u)));
    }

    tree
    get_result_decl (tree callee)
    {
      auto it = m_fab_results.find (callee);
      if (it == m_fab_results.end ())
        {
          // This is what C and C++ front ends do.
          location_t loc = DECL_SOURCE_LOCATION (callee);
          tree restype = TREE_TYPE (TREE_TYPE (callee));
          tree resdecl = build_decl (loc, RESULT_DECL, NULL_TREE, restype);
          DECL_CONTEXT (resdecl) = callee;
          DECL_ARTIFICIAL (resdecl) = 1;
          DECL_IGNORED_P (resdecl) = 1;
          it = m_fab_results.insert (std::make_pair (callee, resdecl)).first;
          track_result_decl (callee, resdecl);
        }
      return it->second;
    }

    std::tuple <tree, unsigned>
    find_context (tree decl)
    {
      if (auto it = m_fab_ctx.find (decl);
          it != m_fab_ctx.end ())
        return it->second;
      else
        return std::make_tuple (NULL_TREE, -1u);
    }

  };

  class callgraph
  {
  public:
    static constexpr unsigned NODE_DEF    = 1 << 0;
    static constexpr unsigned NODE_STATIC = 1 << 1;
    static constexpr unsigned NODE_VAR    = 1 << 2;

    class decl_fab &decl_fab;

  private:
    tree m_dfsrc;
    // caller or callee -> flags
    std::map <tree, unsigned> m_nodes;

    // (caller, callee)
    std::set <std::tuple <tree, tree>> m_edges;

    // struct -> tree used to name the type
    std::map <tree, tree> &m_typedefs;

    // field/var -> context
    // Keeps track of the context in which a field or a variable is defined.
    // This could be a function, a structure (for a field), a typedef or
    // variable name (for a field in an anonymous structure) or perhaps another
    // disambiguator.
    std::map <tree, tree> &m_context;

  public:
    std::string
    dump_callee (tree callee)
    {
      assert (DECL_P (callee));

      std::string ret;
      for (tree context = node_context (callee); context != NULL_TREE;
           context = node_context (context))
        {
          std::string prefix = TYPE_P (context)
            ? type_name (context) : decl_name (context);
          switch (static_cast <int> (TREE_CODE (context)))
            {
            case FUNCTION_DECL:
              prefix += "()";
              break;
            case VAR_DECL:
              prefix = "." + prefix;
              break;
            }
          ret = prefix + "::" + ret;
        }

      if (TREE_CODE (callee) == RESULT_DECL)
        ret += "(ret)";
      else
        ret += decl_name (callee);

      return ret;
    }

    explicit callgraph (class decl_fab &fab,
                        std::map <tree, tree> &typedefs,
                        std::map <tree, tree> &context,
                        tree dfsrc = NULL_TREE)
      : decl_fab {fab}
      , m_dfsrc {dfsrc}
      , m_typedefs {typedefs}
      , m_context {context}
    {}

    tree
    tree_context (tree node)
    {
      if (TYPE_P (node))
        return TYPE_CONTEXT (node);

      assert (DECL_P (node));
      return DECL_CONTEXT (node);
    }

    tree
    node_context (tree node)
    {
      if (auto it = m_context.find (node);
          it != m_context.end ())
        {
          tree context = it->second;

          // Context of anonymous structures is whatever lends them name.
          if (TYPE_P (context) && no_type_name (context))
            {
              if (auto jt = m_typedefs.find (context);
                  jt != m_typedefs.end ())
                context = jt->second;
              else
                die ("Unknown structure name");
            }

          return context;
        }

      if (tree context = DECL_CONTEXT (node))
        if (TREE_CODE (context) != TRANSLATION_UNIT_DECL)
          return context;

      return NULL_TREE;
   }

    tree
    get_toplev_context (tree node)
    {
      // This will return `node' if it's itself a top level function.
      while (tree ctx = node_context (node))
        node = ctx;

      return node;
    }

    void
    add_node (tree node, unsigned flags)
    {
      assert (node != NULL_TREE);
      if (!true)
        std::cerr << "add node:" << dump_callee (node) << std::endl;

      tree parnode = node;
      switch (static_cast <int> (TREE_CODE (node)))
        {
        case PARM_DECL:
          parnode = DECL_CONTEXT (node);
          // Fall through.
        case VAR_DECL:
          if (!DECL_EXTERNAL (parnode))
            // Fall through.
        case FIELD_DECL:
        case RESULT_DECL:
            flags |= callgraph::NODE_DEF;
          flags |= callgraph::NODE_VAR;
          // Fall through.
        default:
          if (tree toplev_ctx = get_toplev_context (node))
            if (!TREE_PUBLIC (toplev_ctx))
              // Types are implicitly static, but in C it is common to see
              // structure sharing by inlining the same type definition in
              // several different translation units. Correspondingly, pretend
              // that types are actually extern.
              if (!(TYPE_P (toplev_ctx)
                    || TREE_CODE (toplev_ctx) == TYPE_DECL))
                flags |= callgraph::NODE_STATIC;
        }

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
      assert (src != NULL_TREE);
      assert (dst != NULL_TREE);

      // Make sure non-function nodes are added as well.
      add_node (src, 0);
      add_node (dst, 0);

      m_edges.insert ({src, dst});

      if (!true)
        std::cerr << dump_callee (src) << " -> "
                  << dump_callee (dst) << std::endl;
    }

    void
    add_typename (tree type, tree name)
    {
      // xxx is it guaranteed that names are going to be the same between
      // compilation units?
      m_typedefs.insert ({type, name}).second;
    }

    void
    add_context (tree node, tree context)
    {
      if (auto it = m_context.find (node);
          it != m_context.end ())
        assert (it->second == context);
      else
        m_context.insert (std::make_pair (node, context));
    }

    void
    add_context_to (tree node)
    {
      if (tree context = tree_context (node))
        if (TREE_CODE (context) != TRANSLATION_UNIT_DECL)
          add_context (node, context);
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
         << (flags & NODE_VAR ? "@var " : "")
         << dump_callee (src);
    }

    void
    dump_context_ref (std::ostream &os, tree src)
    {
      if (TREE_CODE (src) == PARM_DECL
          || TREE_CODE (src) == RESULT_DECL)
        {
          auto ctx = decl_fab.find_context (src);
          tree fn = std::get <0> (ctx);
          assert (fn != NULL_TREE);

          if (unsigned arg_n = std::get <1> (ctx);
              arg_n != -1u)
            os << " arg " << arg_n << ' ' << DECL_UID (fn);
          else
            os << " rv " << DECL_UID (fn);
        }
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
          dump_context_ref (os, src);
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
          dump_context_ref (os, src);
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
      // Merge the subset of nodes used by edges that survived the pruning done
      // by propagate.
      for (auto const &edge: other.m_edges)
        {
          tree src = std::get <0> (edge);
          tree dst = std::get <1> (edge);
          m_nodes[src] |= other.m_nodes.find (src)->second;
          m_nodes[dst] |= other.m_nodes.find (dst)->second;
        }

      m_edges.merge (std::move (other.m_edges));
      m_context.merge (std::move (other.m_context));
    }
  };

  tree
  get_function_type (tree t)
  {
    assert (TYPE_P (t));

    switch (static_cast <int> (TREE_CODE (t)))
      {
      case POINTER_TYPE:
      case ARRAY_TYPE:
        return get_function_type (TREE_TYPE (t));
      case FUNCTION_TYPE:
        return t;
      default:
        return NULL_TREE;
      }
  }

  bool
  is_function_type (tree t)
  {
    return get_function_type (t) != NULL_TREE;
  }

  tree
  get_callee (tree t)
  {
    switch (static_cast <int> (TREE_CODE (t)))
      {
      case PARM_DECL:
      case VAR_DECL:
      case FUNCTION_DECL:
        return t;

      case COMPONENT_REF:
        // Operand 1 is the field (a node of type FIELD_DECL).
        return TREE_OPERAND (t, 1);

      case ADDR_EXPR:
        // Operand 0 is the address at which the operand's value resides.
        return get_callee (TREE_OPERAND (t, 0));

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
  get_result_decl (tree callee, callgraph &cg)
  {
    switch (static_cast <int> (TREE_CODE (callee)))
      {
      case FUNCTION_DECL:
        if (tree result = DECL_RESULT (callee))
          return result;

        // Fall through.

      case PARM_DECL:
      case VAR_DECL:
      case FIELD_DECL:
        return cg.decl_fab.get_result_decl (callee);
      }

    die ("get_result_decl: Unhandled code");
  }

  tree
  find_record_type (tree type)
  {
    assert (TYPE_P (type));

    switch (static_cast <int> (TREE_CODE (type)))
      {
      case UNION_TYPE: // Fall through.
      case RECORD_TYPE:
        return type;
      case ARRAY_TYPE:
        return find_record_type (TREE_TYPE (type));
      }

    return NULL_TREE;
  }

  void walk (tree src, tree t, callgraph &cg, unsigned level = 0);
  void walk_call_expr (tree src, tree call_expr, tree fn, callgraph &cg,
                       unsigned level);

  void
  walk_operand (tree src, tree t, unsigned i, callgraph &cg, unsigned level,
                bool may_be_null = false)
  {
    if (tree ti = TREE_OPERAND (t, i))
      walk (src, ti, cg, level + 1);
    else
      assert (may_be_null);
  }

  void
  walk_call_expr_operand (tree src, tree call_expr, tree fn, unsigned i,
                          callgraph &cg,
                          unsigned level, bool may_be_null = false)
  {
    if (tree ti = TREE_OPERAND (fn, i))
      walk_call_expr (src, call_expr, ti, cg, level + 1);
    else
      assert (may_be_null);
  }

  void
  walk_operands (tree src, tree t, callgraph &cg, unsigned level)
  {
    for (int i = 0; i < tree_operand_length (t); ++i)
      walk_operand (src, t, i, cg, level + 1);
  }

  std::vector <bool> m_seen_decls;
  bool
  should_walk_decl (tree node)
  {
    size_t uid = DECL_UID (node);
    if (uid >= m_seen_decls.size ())
      m_seen_decls.resize (uid + 1, false);
    bool seen = m_seen_decls[uid];
    m_seen_decls[uid] = true;
    return !seen;
  }

  void
  walk_decl (tree decl, callgraph &cg, unsigned level)
  {
    if (!true)
      std::cerr << spaces (level) << "decl:" << tcn (decl)
                << " (" << decl_name (decl) << ')' << std::endl;

    assert (DECL_P (decl));
    if (!should_walk_decl (decl))
      return;

    cg.add_context_to (decl);

    switch (static_cast <int> (TREE_CODE (decl)))
      {
      case TYPE_DECL:
      case FIELD_DECL:
      case PARM_DECL:
        return;
      case FUNCTION_DECL:
        // finish_parse_function() will have already seen body of this function.
        return;
      case VAR_DECL:
        if (tree init = DECL_INITIAL (decl))
          walk (decl, init, cg, level + 1);
        return;
      }

    std::cerr << spaces (level) << "!" << tcn (decl) << std::endl;
    die ("walk_decl: unhandled code");
  }

  void
  walk_call_expr (tree src, tree call_expr, tree fn, callgraph &cg,
                  unsigned level)
  {
    if (!true)
      std::cerr << spaces (level) << "call:" << tcn (fn) << std::endl;
    if (TREE_CODE (fn) == COND_EXPR)
      {
        // A condition doesn't influence what the callee will be, so walk it
        // normally. Dispatch to walk_call_expr for the then and else branches.
        walk_operand (src, fn, 0, cg, level + 1);
        walk_call_expr_operand (src, call_expr, fn, 1, cg, level + 1);
        walk_call_expr_operand (src, call_expr, fn, 2, cg, level + 1, true);
        return;
      }

    tree callee = get_callee (fn);
    assert (callee != NULL_TREE);
    assert (DECL_P (callee));

    cg.add (TREE_CODE (callee) != PARM_DECL ? callee
            : translate_parm_decl (callee, cg));
    if (src != NULL_TREE)
      // Returns function pointer?
      if (tree callee_type = TREE_TYPE (callee);
          is_function_type (TREE_TYPE (callee_type)))
        cg.add (src, get_result_decl (callee, cg));

    // Types of callee arguments.
    std::vector <tree> callee_arg_types;
    {
      tree type = TREE_TYPE (callee);
      tree fn_type = get_function_type (type);
      assert (fn_type != NULL_TREE);
      for (tree a = TYPE_ARG_TYPES (fn_type); a != NULL_TREE; a = TREE_CHAIN (a))
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
        tree src2 = NULL;
        if (i < callee_arg_types.size ())
          {
            tree callee_arg_type = callee_arg_types[i];
            if (is_function_type (callee_arg_type)
                || is_function_type (TREE_TYPE (arg)))
              src2 = cg.decl_fab.get (i, callee_arg_type, callee);
          }

        walk (src2, arg, cg, level + 1);
      }
  }

  void
  walk (tree src, tree t, callgraph &cg, unsigned level)
  {
    if (!true)
      std::cerr << spaces (level) << tcn (t) << std::endl;

    if (CONSTANT_CLASS_P (t))
      return;

    switch (static_cast <int> (TREE_CODE (t)))
      {
      case CALL_EXPR:
        if (tree fn = CALL_EXPR_FN (t))
          walk_call_expr (src, t, fn, cg, level + 1);
        return;

      case DECL_EXPR:
        return walk_decl (DECL_EXPR_DECL (t), cg, level + 1);

      case PARM_DECL:
        t = translate_parm_decl (t, cg);
        // Fall through.
      case FUNCTION_DECL:
      case VAR_DECL:
      case TYPE_DECL:
      case FIELD_DECL:
        if (src != NULL_TREE)
          cg.add (src, t);
        return walk_decl (t, cg, level + 1);

      case LABEL_DECL:
        return;

      case MODIFY_EXPR:
        // Operand 0 is what to set; 1, the new value.
        walk (src, TREE_OPERAND (t, 1), cg, level + 1);
        // If operand 0 is interesting, rerun with a new src. That's currently
        // the only way to add an edge from two sources.
        if (tree dst = TREE_OPERAND (t, 0);
            is_function_type (TREE_TYPE (dst)))
          if (tree dst2 = get_destination (dst))
            {
              if (TREE_CODE (dst2) == RESULT_DECL)
                cg.decl_fab.track_result_decl (DECL_CONTEXT (dst2), dst2);
              walk (dst2, TREE_OPERAND (t, 1), cg, level + 1);
            }
        return;

      case ADDR_EXPR: // Fall through.
      case SAVE_EXPR: // Fall through.
      case NOP_EXPR:
        return walk_operand (src, t, 0, cg, level + 1);

      case COND_EXPR:
        // Operand 0 is the condition.
        // Operand 1 is the then-value.
        // Operand 2 is the else-value.
        walk_operand (src, t, 0, cg, level);
        walk_operand (src, t, 1, cg, level);
        walk_operand (src, t, 2, cg, level, true);
        return;

      case SWITCH_EXPR:
        // Operand 0 is the expression used to perform the branch,
        // Operand 1 is the body of the switch. It may also be NULL.
        // Operand 2 is either NULL_TREE or a TREE_VEC of the CASE_LABEL_EXPRs
        walk_operand (src, t, 0, cg, level);
        walk_operand (src, t, 1, cg, level, true);
        walk_operand (src, t, 2, cg, level, true);
        return;

      case CASE_LABEL_EXPR:
        // Operand 0 is CASE_LOW. It may be NULL_TREE
        // Operand 1 is CASE_HIGH.  If it is NULL_TREE [...]
        // Operand 2 is CASE_LABEL, which is is the corresponding LABEL_DECL.
        // Operand 3 is CASE_CHAIN. This operand is only used in tree-cfg.c
        walk_operand (src, t, 0, cg, level, true);
        walk_operand (src, t, 1, cg, level, true);
        return;

      case TARGET_EXPR:
        // operand 1 is the initializer for the target, which may be void
        // operand 2 is the cleanup for this node, if any.
        // operand 3 is the saved initializer after this node has been expanded
        walk_operand (src, t, 0, cg, level, true);
        walk_operand (src, t, 1, cg, level, true);
        walk_operand (src, t, 2, cg, level, true);
        return;

      case ASM_EXPR:
        for (tree list = ASM_OUTPUTS (t); list != NULL_TREE;
             list = TREE_CHAIN (list))
          walk (src, TREE_VALUE (list), cg, level + 1);
        for (tree list = ASM_INPUTS (t); list != NULL_TREE;
             list = TREE_CHAIN (list))
          walk (src, TREE_VALUE (list), cg, level + 1);
        for (tree list = ASM_CLOBBERS (t); list != NULL_TREE;
             list = TREE_CHAIN (list))
          walk (src, TREE_VALUE (list), cg, level + 1);
        return;

      case RETURN_EXPR:
        // RETURN.  Evaluates operand 0, then returns from the current function.
        // The operand may be null.
        return walk_operand (src, t, 0, cg, level, true);

      case CONSTRUCTOR:
        for (unsigned i = 0; i < CONSTRUCTOR_NELTS (t); ++i)
          {
            constructor_elt *elt = CONSTRUCTOR_ELT (t, i);
            tree src2 = src;
            if (tree type = TREE_TYPE (elt->index);
                is_function_type (type))
              // Constructor index is a function type: initializing a field in a
              // structure.
              src2 = elt->index;
            walk (NULL_TREE, elt->index, cg, level + 1);
            walk (src2, elt->value, cg, level + 1);
          }
        return;

      case BIND_EXPR:
        {
          tree body = BIND_EXPR_BODY (t);
          return walk (src, body, cg, level + 1);
        }

      case IMAGPART_EXPR: // Fall through.
      case REALPART_EXPR:
        return walk_operand (src, t, 0, cg, level);

      case STATEMENT_LIST:
        for (tree_stmt_iterator it = tsi_start (t); !tsi_end_p (it);
             tsi_next (&it))
          walk (src, tsi_stmt (it), cg, level + 1);
        return;

      case INDIRECT_REF:
        // One operand, an expression for a pointer.
        return walk_operand (src, t, 0, cg, level);

      case MEM_REF:
        // Operands are a pointer and a tree constant integer byte offset of the
        // pointer type.
        return walk_operand (src, t, 0, cg, level);

      case COMPONENT_REF: // Fall through.
      case BIT_FIELD_REF:
        // Operand 0 is the structure or union expression;
        walk_operand (NULL_TREE, t, 0, cg, level);

        // Operand 1 is the field (a node of type FIELD_DECL).
        if (src != NULL_TREE)
          if (tree dst = TREE_OPERAND (t, 1);
              is_function_type (TREE_TYPE (dst)))
            cg.add (src, dst);
        return;

      case ARRAY_REF:
        // Operand 0 is the array; operand 1 is a (single) array index.
        walk_operand (src, t, 0, cg, level);
        walk_operand (src, t, 1, cg, level);
        return;
      }

    if (COMPARISON_CLASS_P (t)
        || UNARY_CLASS_P (t)
        || BINARY_CLASS_P (t)
        || EXPRESSION_CLASS_P (t)
        || STATEMENT_CLASS_P (t))
      return walk_operands (src, t, cg, level);

    std::cerr << tcn (t) << std::endl;
    die ("unhandled node");
  }

  void
  walk_type (tree t, callgraph &cg, unsigned level = 0)
  {
    if (!true)
      std::cerr << spaces (level) << "type:" << tcn (t) << std::endl;

    switch (static_cast <int> (TREE_CODE (t)))
      {
      case UNION_TYPE: // Fall through.
      case RECORD_TYPE:
        cg.add_context_to (t);
        for (tree field = TYPE_FIELDS (t); field != NULL_TREE;
             field = TREE_CHAIN (field))
          walk (NULL_TREE, field, cg, level + 1);
        break;
      }
  }
}

class calgary
{
  std::stringstream m_ofs;
  decl_fab m_fab;
  std::map <tree, tree> m_typedefs;
  std::map <tree, tree> m_context;
  callgraph m_cg;
  std::set <tree> m_seen_types;

public:
  explicit calgary ()
    : m_fab {}
    , m_cg {m_fab, m_typedefs, m_context}
  {}

  void
  possible_anon_struct_name (tree type, tree decl)
  {
    if (tree rec = find_record_type (type))
      if (no_type_name (rec))
        m_cg.add_typename (find_main_type (rec), decl);
  }

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
    m_cg.add_node (fn, flags);

    possible_anon_struct_name (TREE_TYPE (TREE_TYPE (fn)), fn);

    if (tree body = DECL_SAVED_TREE (fn))
      {
        callgraph cg {m_fab, m_typedefs, m_context, fn};
        walk (NULL_TREE, body, cg);
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
        possible_anon_struct_name (TREE_TYPE (decl), decl);

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
          m_cg.add_typename (find_main_type (type), decl);
        return;

      default:
        return;
      }

    if (!true)
      std::cerr << "\n- decl ------\n"
                << decl_name (decl) << std::endl
                << "-------------\n";

    if (DECL_INITIAL (decl))
      {
        callgraph cg {m_fab, m_typedefs, m_context, decl};
        walk_decl (decl, cg, 0);
        cg.propagate ();
        m_cg.merge (std::move (cg));
      }
    process_aliases (decl);
  }

  void
  finish_type (tree type)
  {
    assert (TYPE_P (type));

    if (!m_seen_types.insert (type).second)
      return;

    if (!true)
      std::cerr << "\n- type ------\n"
                << type_name (type) << std::endl
                << "-------------\n";

    callgraph cg {m_fab, m_typedefs, m_context};
    walk_type (type, cg);
    cg.propagate ();
    m_cg.merge (std::move (cg));
  }

  void
  emit ()
  {
    m_cg.dump (m_ofs);
    m_ofs << "---\n";

    unsigned flags = SECTION_NOTYPE | SECTION_DEBUG | SECTION_STRINGS | 1;
    switch_to_section (get_section (".calgary.callgraph", flags, NULL_TREE));

    std::string data = m_ofs.str ();
    assemble_string (data.c_str (), data.length ());
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
  ipa_passes_end (void *gcc_data, void *user_data)
  {
    auto cgr = static_cast <calgary *> (user_data);
    cgr->emit ();
  }

  void
  finish (void *gcc_data, void *user_data)
  {
    auto cgr = static_cast <calgary *> (user_data);
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
      std::cerr << "Unknown argument: " << plugin_info->argv[i].key
                << std::endl;
      return 1;
    }

  calgary *cgr = new calgary ();

  register_callback (plugin_info->base_name, PLUGIN_FINISH_PARSE_FUNCTION,
                     finish_parse_function, cgr);
  register_callback (plugin_info->base_name, PLUGIN_FINISH_DECL,
                     finish_decl, cgr);
  register_callback (plugin_info->base_name, PLUGIN_FINISH_TYPE,
                     finish_type, cgr);
  register_callback (plugin_info->base_name, PLUGIN_ALL_IPA_PASSES_END,
                     ipa_passes_end, cgr);
  register_callback (plugin_info->base_name, PLUGIN_FINISH,
                     finish, cgr);

  return 0;
}
