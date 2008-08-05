import cgt
import re

class SymbolSet (object):
    def __init__(self, parent, contains):
        """Parent is supposed to be the ultimate "whole-world"
        callgraph.  It's used for computation of inverted set."""
        self.contains = set(contains)
        self.parent = parent
        self.callees_set = None
        self.callers_set = None

    def __iter__(self):
        return self.contains.__iter__()

    def __nonzero__(self):
        return len(self.contains) > 0

    def __isub__(self, other):
        self.contains -= other.contains
        return self

    def __imul__(self, other):
        self.contains.intersection_update(other.contains)
        return self

    def __iadd__(self, other):
        self.contains.update(other.contains)
        return self

    def __sub__(self, other):
        return SymbolSet(self.parent, self.contains - other.contains)

    def __mul__(self, other):
        return SymbolSet(self.parent, self.contains.intersection(other.contains))

    def __add__(self, other):
        return SymbolSet(self.parent, self.contains.union(other.contains))

    def __invert__(self):
        return self.parent - self

    def callees(self):
        if self.callees_set != None:
            return self.callees_set

        callees_set = set()
        for sym in self.contains:
            callees_set.update(sym.callees())
        self.callees_set = SymbolSet(self.parent, callees_set)
        return self.callees_set

    def callers(self):
        if self.callers_set != None:
            return self.callers_set

        callers_set = set()
        for sym in self.contains:
            callers_set.update(sym.callers())
        self.callers_set = SymbolSet(self.parent, callers_set)
        return self.callers_set

    def tcallers(self):
        return tclose(self, callers)

    def tcallees(self):
        return tclose(self, callees)

    def trcallers(self):
        return trclose(self, callers)

    def trcallees(self):
        return trclose(self, callees)

    def __repr__(self):
        return "{" + ", ".join(s.name for s in self.contains) + "}"

    def __getitem__(self, pattern):
        if callable(pattern) and pattern != all:
            c = set()
            for sym in self.contains:
                if pattern(self.parent, sym):
                    c.add(sym)
            return SymbolSet(self.parent, c)

        else:
            if pattern == Ellipsis:
                components = [None, None, None]
            elif type(pattern) != slice:
                components = [None, None, pattern]
            elif pattern.step == None: # file:name
                components = [pattern.start, None, pattern.stop]
            else: # file:line:name
                components = [pattern.start, pattern.stop, pattern.step]

            filep = components[0]
            if filep == all or filep == None:
                filep = ".*"
            filep = re.compile(filep)

            namep = components[2]
            if namep == all or namep == None:
                namep = ".*"
            namep = re.compile("^" + namep + "$")

            line = components[1]
            if type(line) == int or type(line) == long:
                line = [line]
            elif line == all:
                line = None

            return self[lambda cg, sym:
                            (namep.search(sym.name)
                             and filep.search(sym.file)
                             and (line == None or sym.line in line))]

    def pop(self):
        return self.contains.pop()

    def __len__(self):
        return len(self.contains)

    def paths(self, dest):
        def fpaths(graph, start, end, path=[]):
            path = path + [start]
            if start == end:
                return [path]
            found = []
            for node in start.callees():
                if node not in path:
                    newpaths = fpaths(graph, node, end, path)
                    for newpath in newpaths:
                        found.append(newpath)
            return found

        ret = []
        for a in self:
            for b in dest:
                ret += fpaths(self.parent, a, b)
        return PathSet(self.parent, list(set(tuple(i) for i in ret)))

class CG (SymbolSet):
    def __init__(self, file):
        self.cg = cgt.cgfile() # keep the reference
        self.cg.include(file)
        self.cg.compute_callers()
        SymbolSet.__init__(self, self, self.cg.all_program_symbols())

class PathSet (object):
    def __init__(self, parent, paths):
        self.parent = parent
        self.paths = paths

    def __iter__(self):
        return self.paths.__iter__()

    def unique(self):
        d={}
        for p in self.paths:
            x = (p[0], p[-1])
            if (x not in d) or (len(d[x]) > len(p)):
                d[x] = p
        return PathSet(self.parent, d.values())

    def __getitem__(self, pattern):
        if callable(pattern) and pattern != all:
            p = []
            for path in self.paths:
                if pattern(self.parent, path):
                    p.append(path)
            return PathSet(self.parent, p)

        else:
            def compile_pattern(pat):
                if pat == all or pat == None:
                    pat = ".*"
                return re.compile("^" + pat + "$")

            # paths[...] -> SymbolSet containing all symbols in all paths
            if pattern == Ellipsis:
                return self[".*"]

            # paths["main.*"] -> SymbolSet containing just symbols matching "main.*"
            if type(pattern) != slice:
                pattern = compile_pattern(pattern)
                sset = set()
                for path in self.paths:
                    for sym in path:
                        if pattern.search(sym.name):
                            sset.add(sym)
                return SymbolSet(sset)

            # paths["main":] -> subpaths from "main" on
            # paths["main":"fun"] -> subpaths between "main" and "fun", "fun" is not included
            # paths[1:4], paths[:-1] -> traditional slice behavior
            def get_index_function(pattern):
                if pattern == all or pattern == None:
                    pattern = -1

                try:
                    return lambda p: pattern + 0
                except TypeError:
                    pattern = compile_pattern(pattern);
                    def find_first_in_path(path):
                        for i, sym in enumerate(path):
                            if pattern.search(sym.name):
                                return i
                        raise ValueError("not in list")
                    return find_first_in_path

            start_index = get_index_function(pattern.start)
            end_index = get_index_function(pattern.end)

            paths = []
            for path in paths:
                paths.append(path[start_index(path):end_index(path)])
            return PathSet(self.parent, paths)

    def __repr__(self):
        return self.paths.__repr__()

    def __add__(self, other):
        return PathSet(self.parent, list(set(self.paths).union(set(other.paths))))

    def __mul__(self, other):
        return PathSet(self.parent, list(set(self.paths).intersection(set(other.paths))))

    def __sub__(self, other):
        return PathSet(self.parent, list(set(self.paths) - set(other.paths)))

callees = SymbolSet.callees
callers = SymbolSet.callers

def trclose(sset, fun):
    """Compute transitive reflective closure on given function fun.
    `callees' and `callers' will form good cadidates for fun."""
    ret = sset
    old = ()
    while len(ret) > len(old):
        old = ret
        ret = ret + fun(ret)
    return ret

def tclose(sset, fun):
    """Compute transitive (non-reflexive) closure on given function
    fun."""
    return trclose(fun(sset), fun)

class FlagPattern (object):
    def __init__(self):
        pass
    def __and__(self, other):
        return BinaryFlagPattern((lambda a, b: a and b),
                                 self, other)
    def __or__(self, other):
        return BinaryFlagPattern((lambda a, b: a or b),
                                 self, other)
    def __invert__(self):
        return UnaryFlagPattern((lambda a: not a), self)

class BinaryFlagPattern (FlagPattern):
    def __init__(self, operator, op1, op2):
        FlagPattern.__init__(self)
        self.operator = operator
        self.op1 = op1
        self.op2 = op2

    def __call__(self, cg, symbol):
        return self.operator(self.op1(cg, symbol),
                             self.op2(cg, symbol))

class UnaryFlagPattern (FlagPattern):
    def __init__(self, operator, operand):
        FlagPattern.__init__(self)
        self.operator = operator
        self.operand = operand

    def __call__(self, cg, symbol):
        return self.operator(self.operand(cg, symbol))

class Flag (FlagPattern):
    def __init__(self, test):
        self.test = test

    def __call__(self, cg, symbol):
        return self.test(symbol)

def build_rp (func):
    class RelationshipPattern (FlagPattern):
        def __init__(self, what):
            FlagPattern.__init__(self)
            self.what = what
            self.cache = None
            self.lcg = None
        def __call__(self, cg, symbol):
            if type(self.what) == str:
                if self.cache == None or id(self.lcg) != id(cg):
                    what = cg[self.what]
                    self.lcg = cg
                    self.cache = what
                else:
                    what = self.cache
            else:
                what = self.what

            return len(func(SymbolSet(None, [symbol])) * what) > 0
    return RelationshipPattern

def build_srp (func):
    class SelfRelationshipPattern (FlagPattern):
        def __init__(self):
            FlagPattern.__init__(self)
        def __call__(self, cg, symbol):
            return len(func(SymbolSet(None, [symbol])) * SymbolSet(None, [symbol])) > 0
    return SelfRelationshipPattern

var = Flag(lambda s: s.var)
fun = ~var
static = Flag(lambda s: s.static)
extern = ~static
decl = Flag(lambda s: s.decl)

calls = build_rp(SymbolSet.callees)
called_by = build_rp(SymbolSet.callers)
tcalls = build_rp(SymbolSet.tcallees)
tcalled_by = build_rp(SymbolSet.tcallers)
trcalls = build_rp(SymbolSet.trcallees)
trcalled_by = build_rp(SymbolSet.trcallers)

calls_itself = build_srp(SymbolSet.callees)()
tcalls_itself = build_srp(SymbolSet.tcallees)()
