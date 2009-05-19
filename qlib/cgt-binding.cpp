#include "BitmapIndex.h"
#include "CallGraph.h"
#include "Cgt.h"
#include "Color.h"
#include "Linker.h"
#include "PathFinder.h"
#include "VertexFilter.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/python.hpp>

using namespace boost::python;

typedef CallGraph TGraph;

struct ProgramSymbol;
typedef std::vector<ProgramSymbol *> psym_vect;
typedef std::vector<psym_vect *> path_vect;

class ProgramSymbol {
    public:
        typedef boost::graph_traits<TGraph>             Traits;
        typedef Traits::vertex_descriptor               TVertex;

    public:
        ProgramSymbol(const TGraph &graph, TVertex vertex):
            graph_(graph),
            v_(vertex),
            fnc_(get(get(FncProp(), graph), vertex))
        {
        }

        int get_id() const {
            return v_;
        }

        bool is_static() const {
            return !fnc_->isGlobal;
        }

        bool is_decl() const {
            return !fnc_->isDefined;
        }

        bool is_var() const {
            return false;
        }

        int get_line_number() const {
            return fnc_->loc.lineno;
        }

        psym_vect* get_callees() {
            psym_vect *pv = new psym_vect;
            Traits::out_edge_iterator ei, ei_end;
            for (tie(ei, ei_end) = out_edges(v_, graph_); ei != ei_end; ++ei)
                pv->push_back(new ProgramSymbol (graph_, target(*ei, graph_)));
            return pv;
        }

        psym_vect* get_callers() {
            psym_vect *pv = new psym_vect;
            Traits::in_edge_iterator ei, ei_end;
            for (tie(ei, ei_end) = in_edges(v_, graph_); ei != ei_end; ++ei)
                pv->push_back(new ProgramSymbol (graph_, source(*ei, graph_)));
            return pv;
        }

        char const* psym_get_name() const {
            return fnc_->name.c_str();
        }

        char const* get_file_name() const {
            return fnc_->loc.file->c_str();
        }

        int hash() const {
            return get_id();
        }

        int cmp(ProgramSymbol & other) const {
            const unsigned q1 = get_id();
            const unsigned q2 = other.get_id();
            if (q1 < q2)
                return -1;
            else if (q1 > q2)
                return 1;
            else
                return 0;
        }

        char const* repr() const {
            return psym_get_name();
        }

    private:
        const TGraph    &graph_;
        TVertex         v_;
        PFnc            fnc_;
};

// TODO: remove the following nonsense
struct psym_vect_binder {
    static std::string repr(psym_vect &pv) {
        std::string s("(");

        psym_vect::const_iterator i = pv.begin();
        if (i != pv.end())
            s += ((*i++)->psym_get_name());
        for (; i != pv.end(); ++i) {
            s += (", ");
            s += ((*i)->psym_get_name());
        }

        return s + ")";
    }
};

template <typename TCont>
class py_iterator {
    public:
        typedef typename TCont::value_type TItem;

    public:
        py_iterator(TCont &cont):
            i_(cont.begin()),
            end_(cont.end())
        {
        }

    public:
        static py_iterator* create(TCont &cont) {
            return new py_iterator(cont);
        }

        TItem next() {
            if (i_ == end_) {
                PyErr_SetNone(PyExc_StopIteration);
                throw_error_already_set();

                // notreached
                return *static_cast<TItem *>(NULL);
            }

            return *i_++;
        }

    private:
        typename TCont::iterator i_, end_;
};

template <typename TCont>
class py_vertex_iterator {
    public:
        py_vertex_iterator(TCont &cont):
            graph_(cont.graph()),
            i_(cont.begin()),
            end_(cont.end())
        {
        }

    public:
        static py_vertex_iterator* create(TCont &cont) {
            return new py_vertex_iterator(cont);
        }

        ProgramSymbol* next() {
            if (i_ == end_) {
                PyErr_SetNone(PyExc_StopIteration);
                throw_error_already_set();

                // notreached
                return static_cast<ProgramSymbol *>(NULL);
            }

            return new ProgramSymbol(graph_, *i_++);
        }

    private:
        const TGraph &graph_;
        typename TCont::iterator i_, end_;
};

class vset;
class cgfile {
    public:
        typedef BitmapIndexer<TGraph>                   TIndexer;

    public:
        cgfile():
            //graph_(linker_.output())
            indexer_(graph_)
        {
        }

        const TGraph& graph() const {
            return graph_;
        }

        TIndexer& indexer() {
            return indexer_;
        }

        void include(char const* filename) {
            std::fstream str(filename, std::ios::in);
            if (!str) {
                std::cerr << Color(C_LIGHT_RED) << "can't open " << Color(C_NO_COLOR)
                    << filename << std::endl;
                return;
            }
            std::cerr << "--- "
                << Color(C_YELLOW) << filename << Color(C_NO_COLOR)
                << std::endl;

            // parse input
            /*TGraph graph;
            CgtGraphBuilder<TGraph> builder(graph);*/
            CgtGraphBuilder<TGraph> builder(graph_);
            CgtReader reader(&builder);
            reader.read(str, true);
            str.close();

            // link
            /*VertexFilter<TGraph, DropUnusedDeclarations> filtered(graph);
            linker_.link(filtered);*/
        }

        void compute_callers() {
            indexer_.build();
        }

        vset* all_program_symbols();

    private:
        /*typedef Linker<TGraph>                          TLinker;

        TLinker linker_;
        const TGraph &graph_;*/
        TGraph      graph_;
        TIndexer    indexer_;
};

class vset {
    public:
        typedef cgfile::TIndexer                        TIndexer;
        typedef TBitmapIndex                            TBitmap;

        class iterator {
            public:
                typedef boost::graph_traits<TGraph>     Traits;
                typedef Traits::vertex_iterator         TVertIterator;

            public:
                typedef std::forward_iterator_tag       iterator_category;
                typedef TVertIterator::value_type       value_type;
                typedef TVertIterator::difference_type  difference_type;
                typedef TVertIterator::pointer          pointer;
                typedef TVertIterator::reference        reference;

            public:
                bool operator==(const vset::iterator &other) const {
                    return i_ == other.i_;
                }

                iterator& operator++() {
                    assert(i_ != end_);
                    ++i_;

                    while ((i_ != end_) && !bitmap_[*i_])
                        ++i_;

                    return *this;
                }

                value_type operator*() {
                    assert(i_ != end_);
                    return *i_;
                }

            private:
                iterator(const vset &vs, bool end):
                    bitmap_(vs.bitmap())
                {
                    tie(i_, end_) = vertices(vs.graph());

                    if (end) {
                        i_ = end_;
                        return;
                    }

                    while ((i_ != end_) && !bitmap_[*i_])
                        ++i_;
                }

            private:
                TVertIterator   i_, end_;
                const TBitmap   &bitmap_;

                friend class vset;
        };

        typedef iterator                                const_iterator;

    public:
        vset(vset &other, const TBitmap &bitmap):
            graph_(other.graph_),
            indexer_(other.indexer_),
            bitmap_(bitmap)
        {
        }

        vset(cgfile &cg, bool init = false):
            graph_(cg.graph()),
            indexer_(cg.indexer())
        {
            bitmap_.resize(num_vertices(graph_), init);
        }

        const TGraph& graph() const {
            return graph_;
        }

        TIndexer& indexer() {
            return indexer_;
        }

        const TBitmap& bitmap() const {
            return bitmap_;
        }

        iterator begin() const {
            return iterator(*this, false);
        }

        iterator end() const {
            return iterator(*this, true);
        }

        void add(ProgramSymbol &ps) {
            bitmap_[ps.get_id()] = true;
        }

        size_t size() const {
            return bitmap_.count();
        }

        void update(const vset &other) {
            bitmap_ |= other.bitmap_;
        }

        void update(const psym_vect &pv) {
            BOOST_FOREACH(ProgramSymbol *psym, pv) {
                TVertex v = psym->get_id();
                bitmap_[v] = true;
            }
        }

        void intersection_update(const vset &other) {
            bitmap_ &= other.bitmap_;
        }

        void sub(const vset &other) {
            TBitmap mask(other.bitmap_);
            mask.flip();
            bitmap_ &= mask;
        }

        template <template <typename> class TUniq>
        path_vect* find_paths(const vset &dstSet);

        vset* find_reachable(bool reverse);

    private:
        typedef boost::graph_traits<TGraph>             Traits;
        typedef Traits::vertex_descriptor               TVertex;
        typedef PathFinder<TGraph, VoidPathFinder>      TVoidPathFinder;
        typedef TVoidPathFinder::TPathList              TPathList;
        typedef TVoidPathFinder::TPathSet               TPathSet;
        typedef TVoidPathFinder::TPath                  TPath;

        const TGraph        &graph_;
        TIndexer            &indexer_;
        TBitmap             bitmap_;
};

inline bool operator!=(const vset::iterator &a, const vset::iterator &b) {
    return !(a == b);
}

inline vset::iterator operator++(vset::iterator &i, int) {
    vset::iterator tmp(i);
    ++i;
    return tmp;
}

inline vset update(vset self, const vset &other) {
    self.update(other);
    return self;
}

inline vset intersection_update(vset self, const vset &other) {
    self.intersection_update(other);
    return self;
}

inline vset sub(vset self, const vset &other) {
    self.sub(other);
    return self;
}

struct vset_binder {
    static std::string repr(vset &ps) {
        const TGraph g = ps.graph();
        std::string s("(");

        vset::iterator i = ps.begin();
        if (i != ps.end())
            s += (ProgramSymbol(g, *i++).psym_get_name());
        for (; i != ps.end(); ++i) {
            s += (", ");
            s += (ProgramSymbol(g, *i).psym_get_name());
        }

        return s + ")";
    }
};

class vset_subgraph {
    public:
        vset_subgraph(vset &vs):
            filteredGraph_(vs.graph(), vs.bitmap())
        {
        }

        std::string dump_for_graphviz();

    private:
        typedef BitmapFilter<TGraph>                    TFilter;
        TFilter filteredGraph_;
};

vset* cgfile::all_program_symbols() {
    return new vset(*this, true);
}

template <template <typename> class TUniq>
path_vect* vset::find_paths(const vset &dstSet) {
    typedef TBitmapIndex                        TBitmap;
    typedef BitmapFilter<TGraph>                TFilter;

    const size_t nVert = num_vertices(graph_);

    // compute mask for srcSet
    TBitmap srcMask;
    srcMask.resize(nVert, false);
    BOOST_FOREACH(TVertex src, (*this)) {
        srcMask[src] = true;
        srcMask |= indexer_.index(src, TIndexer::OUT);
    }

    // compute mask for dstSet
    TBitmap dstMask;
    dstMask.resize(nVert, false);
    BOOST_FOREACH(TVertex dst, dstSet) {
        dstMask[dst] = true;
        dstMask |= indexer_.index(dst, TIndexer::IN);
    }

    // prune graph by the masks
    TFilter prunedGraph(graph_, srcMask & dstMask);

    // search all paths
    TPathSet pset;
    BOOST_FOREACH(TVertex src, (*this)) {
        PathFinder<TFilter, TUniq> finder(prunedGraph);
        finder.compute(src);

        BOOST_FOREACH(TVertex dst, dstSet) {
            finder.paths(dst, pset);
        }
    }

#if DEBUG_PATH_FINDER
    std::cerr << "--- total solutions found: " << pset.size() << std::endl;
#endif

    // convert edge defined paths to vertex defined paths
    path_vect *plist = new path_vect;

    TPathSet::const_iterator pi;
    for (pi = pset.begin(); pi != pset.end(); ++pi) {
        const TPath &p = *pi;
        psym_vect *vlist = new psym_vect;
        TPath::const_iterator ei = p.begin();
        if (ei != p.end()) {
            vlist->push_back(new ProgramSymbol(graph_, source(*ei, graph_)));
            for (; ei != p.end(); ei++)
                vlist->push_back(new ProgramSymbol(graph_, target(*ei, graph_)));
        }
        plist->push_back(vlist);
    }

    return plist;
}

vset* vset::find_reachable(bool reverse) {
    typedef TBitmapIndex                        TBitmap;
    typedef BitmapFilter<TGraph>                TFilter;

    const size_t nVert = num_vertices(graph_);

    TBitmap mask;
    mask.resize(nVert, false);
    BOOST_FOREACH(TVertex src, (*this)) {
        mask |= indexer_.index(src, reverse ? TIndexer::IN : TIndexer::OUT);
    }

    return new vset(*this, mask);
}

template <typename TGraph>
class SimpleFncWriter {
    public:
        typedef boost::graph_traits<TGraph>             Traits;
        typedef typename Traits::vertex_descriptor      TVertex;

    public:
        SimpleFncWriter(const TGraph &graph):
            fncProp_(get(FncProp(), graph)),
            reTmpl_("<[^()]*>")
        {
        }

        void operator()(std::ostream &out, TVertex vertex) {
            PFnc fnc = get(fncProp_, vertex);
            out << "[label=\""
                << boost::regex_replace(fnc->name, reTmpl_, "")
                << "\"]";
        }

    private:
        typedef boost::property_map<TGraph, FncProp>    TPropMapping;
        typedef typename TPropMapping::const_type       TProp;
        TProp               fncProp_;
        const boost::regex  reTmpl_;
};

std::string vset_subgraph::dump_for_graphviz() {
    std::ostringstream str;
    SimpleFncWriter<TFilter> vertexWriter(filteredGraph_);
    write_graphviz(str, filteredGraph_, vertexWriter);
    return str.str();
}

BOOST_PYTHON_MODULE(cgt)
{
    class_<py_iterator<path_vect> >("py_iterator<path_vect>", no_init)
        .def("next",                &py_iterator<path_vect>:: next,
                return_value_policy<manage_new_object>())
        ;

    class_<py_iterator<psym_vect> >("py_iterator<psym_vect>", no_init)
        .def("next",                &py_iterator<psym_vect>:: next,
                return_value_policy<manage_new_object>())
        ;

    class_<py_vertex_iterator<vset> >("py_vertex_iterator<vset>", no_init)
        .def("next",                &py_vertex_iterator<vset>:: next,
                return_value_policy<manage_new_object>())
        ;

    class_<psym_vect>("psym_vect", no_init)
        .def("__repr__",            &psym_vect_binder:: repr)
        .def("__iter__",            &py_iterator<psym_vect>:: create,
                return_value_policy<manage_new_object>())
        ;

    class_<path_vect>("path_vect", no_init)
        .def("__iter__",            &py_iterator<path_vect>:: create,
                return_value_policy<manage_new_object>())
        ;

    class_<cgfile>("cgfile")
        .def("include",             &cgfile:: include)
        .def("compute_callers",     &cgfile:: compute_callers)

        .def("all_program_symbols", &cgfile:: all_program_symbols,
                return_value_policy<manage_new_object>())

        ;

    class_<vset>("vset",            init<cgfile &>())
        .def("__repr__",            &vset_binder:: repr)
        .def("__iter__",            &py_vertex_iterator<vset>:: create,
                return_value_policy<manage_new_object>())

        .def("__len__",             &vset:: size)

        .def("add",                 &vset:: add)

        .def("update",              (void (vset:: *)(const vset &))
                                    &vset:: update)

        .def("update",              (void (vset:: *)(const psym_vect &))
                                    &vset:: update)

        .def("__iadd__",            (void (vset:: *)(const vset &))
                                    &vset:: update)

        .def("__imul__",            &vset:: intersection_update)

        .def("__isub__",            &vset:: sub)

        .def("__add__",             &update)
        .def("union",               &update)

        .def("__mul__",             &intersection_update)
        .def("intersection",        &intersection_update)

        .def("__sub__",             &sub)

        .def("find_paths",          &vset:: find_paths<UniqVertexPath>,
                return_value_policy<manage_new_object>())

        .def("find_trails",         &vset:: find_paths<UniqEdgePath>,
                return_value_policy<manage_new_object>())

        .def("find_reachable",      &vset:: find_reachable,
                return_value_policy<manage_new_object>())
        ;

    class_<vset_subgraph>("vset_subgraph", init<vset &>())
        .def("dump_for_graphviz",   &vset_subgraph:: dump_for_graphviz)
        ;

    class_<ProgramSymbol>("ProgramSymbol", no_init)
        .add_property("id",         &ProgramSymbol:: get_id)
        .add_property("name",       &ProgramSymbol:: psym_get_name)
        .add_property("static",     &ProgramSymbol:: is_static)
        .add_property("decl",       &ProgramSymbol:: is_decl)
        .add_property("var",        &ProgramSymbol:: is_var)
        .add_property("line",       &ProgramSymbol:: get_line_number)
        .add_property("file",       &ProgramSymbol:: get_file_name)
        .def("__cmp__",             &ProgramSymbol:: cmp)
        .def("__hash__",            &ProgramSymbol:: hash)
        .def("__repr__",            &ProgramSymbol:: repr)

        .def("callees",             &ProgramSymbol:: get_callees,
                return_value_policy<manage_new_object>())

        .def("callers",             &ProgramSymbol:: get_callers,
                return_value_policy<manage_new_object>())
        ;
}
