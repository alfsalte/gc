
CXX := g++
CXXFLAGS := -g -std=c++17
ODIR := obj
O := .o

GC_SDIR := ../private
GC_ODIR := $(GC_SDIR)/obj

A_SOURCES := a.cxx
A_OFILES := $(patsubst %.cxx,$(ODIR)/%$(O),$(A_SOURCES))

GC_SOURCES_PLAIN := gcpriv.cxx \
moved.cxx removed.cxx fremoved.cxx head.cxx tail.cxx \
minipool.cxx \
pool.cxx gcpool.cxx fpool.cxx lpool.cxx ptrpool.cxx gcstat.cxx \
gcerror.cxx dangling_pointer.cxx gc_allocation_error.cxx \
gcobj.cxx gcdataobj.cxx

GC_SOURCES := $(patsubst %.cxx,$(GC_SDIR)/%.cxx,$(GC_SOURCES_PLAIN))
# GC_OFILES := $(patsubst %.cxx,$(GC_ODIR)/%$(O),$(GC_SOURCES))
GC_OFILES := $(GC_ODIR)/gc.o

$(ODIR)/%$(O): %.cxx
	$(CXX) -c $(CXXFLAGS) -o $@ $<

all: a

a: $(A_OFILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(GC_OFILES) ../../format/obj/format.o

$(ODIR)/a$(O): a.cxx ../gc.hxx
	$(CXX) -c $(CXXFLAGS) -o $@ $<
