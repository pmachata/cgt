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
        return list(set(tuple(i) for i in ret))

class CG (SymbolSet):
    def __init__(self, file):
        self.cg = cgt.cgfile() # keep the reference
        self.cg.include(file)
        self.cg.compute_callers()
        SymbolSet.__init__(self, self, self.cg.all_program_symbols())

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
