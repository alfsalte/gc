
GXX := g++
LD := ld
CXXFLAGS := -g -std=gnu++17
O := .o
ODIR := obj

SOURCES := gcpriv.cxx \
moved.cxx removed.cxx fremoved.cxx head.cxx tail.cxx \
minipool.cxx \
pool.cxx gcpool.cxx fpool.cxx lpool.cxx ptrpool.cxx fptrpool.cxx wptrpool.cxx \
gcstat.cxx \
gcerror.cxx dangling_pointer.cxx gc_allocation_error.cxx \
gcobj.cxx gcdataobj.cxx

OFILES := $(patsubst %.cxx,$(ODIR)/%$(O),$(SOURCES))

HFILES1 := moved.hxx removed.hxx fremoved.hxx tail.hxx minipool.hxx \
head.hxx tail.hxx

HFILES2 := $(HFILES1) \
pool.hxx gcpool.hxx fpool.hxx lpool.hxx \
ptrpool.hxx fptrpool.hxx wptrpool.hxx \
gcstat.hxx

$(ODIR)/%$(O): %.cxx
	$(GXX) -c $(CXXFLAGS) -o $@ $<

all: $(ODIR)/gc$(O)

$(ODIR)/gc$(O): gc.cxx $(HFILES2) ../gc.hxx $(SOURCES)
	$(GXX) -c $(CXXFLAGS) -o $@ $<

$(ODIR)/moved$(O): moved.cxx moved.hxx ../gc.hxx

$(ODIR)/removed$(O): removed.cxx removed.hxx ../gc.hxx

$(ODIR)/fremoved$(O): fremoved.cxx fremoved.hxx removed.hxx ../gc.hxx

$(ODIR)/head$(O): head.cxx $(HFILES1) ../gc.hxx

$(ODIR)/tail$(O): tail.cxx $(HFILES1) ../gc.hxx

$(ODIR)/minipool$(O): minipool.cxx minipool.hxx $(HFILES1) ../gc.hxx

$(ODIR)/pool$(O): pool.cxx pool.hxx ../gc.hxx

$(ODIR)/gcpool$(O): gcpool.cxx gcpool.hxx $(HFILES2) ../gc.hxx

$(ODIR)/fpool$(O): fpool.cxx $(HFILES2) ../gc.hxx

$(ODIR)/lpool$(O): lpool.cxx $(HFILES2) ../gc.hxx

$(ODIR)/ptrpool$(O): ptrpool.cxx $(HFILES2) ../gc.hxx

$(ODIR)/gcstat$(O): gcstat.cxx $(HFILES2) ../gc.hxx

$(ODIR)/gcerror$(O): gcerror.cxx ../gc.hxx

$(ODIR)/dangling_pointer$(O): dangling_pointer.cxx ../gc.hxx

$(ODIR)/gc_allocation_error$(O): gc_allocation_error.cxx ../gc.hxx

$(ODIR)/gcobj$(O): gcobj.cxx $(HFILES2) ../gc.hxx

$(ODIR)/gcdataobj$(O): gcdataobj.cxx $(HFILES2) ../gc.hxx

