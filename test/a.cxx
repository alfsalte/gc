
#include "../gc.hxx"
#include "../../format/format.hxx"

struct foo : alf::gc::gcobj {
  std::string pre;
  std::string id;
  foo * a;
  foo * b;

  foo(const std::string & pre_, const std::string & id_, foo * a_, foo * b_)
    : pre(pre_), id(id_), a(a_), b(b_)
  { std::cout << "Making foo " << pre << id << std::endl; }

  std::string full() const { return pre + id; }

  void mka()
  { a = new foo(pre + id, ".a", 0, 0); }

  void mkb()
  { b = new foo(pre + id, ".b", 0, 0); }
  
  virtual ~foo();

  virtual void gc_walker(const std::string & txt);

  virtual void dump_(const std::string & txt) const;

};

void dump(const std::string & txt, const foo * obj);

int main()
{
  foo * root = 0;
  alf::gc::register_root_ptr("root", root);
  std::cout << "building tree" << std::endl;
  root = new foo("", "root", 0, 0);
  root->mka(); root->mkb();
  root->a->mka(); root->a->mkb(); root->b->mka(); root->b->mkb();
  root->a->b->mka();
  dump("1 - root", root);
  alf::gc::report();
  alf::gc::gc();
  alf::gc::report();
  foo * p = root->a->b->a;
  std::cout << "freezing root->a->b->a" << std::endl;

  // freeze p - note that p is not registered as root ptr.
  alf::gc::freeze(p,false);

  alf::gc::report();
  std::cout << "Setting root->a->b to NULL" << std::endl;
  root->a->b = 0;
  dump("2 - root", root);
  alf::gc::report();
  alf::gc::gc();
  alf::gc::report();
  std::cout << "Setting root to NULL" << std::endl;
  root = 0;
  alf::gc::report();
  alf::gc::gc();
  alf::gc::report();
  std::cout << "Unfreezing root->a->b->a" << std::endl;
  alf::gc::unfreeze(p);
  alf::gc::report();
}

// virtual
foo::~foo()
{
  a = 0;
  std::cout << "Destroying foo " << pre << id << std::endl;
}

// virtual
void foo::gc_walker(const std::string & txt)
{
  alf::gc::gc_walk(txt + ".a", a);
  alf::gc::gc_walk(txt + ".b", b);
}

void dump(const std::string & txt, const foo * obj)
{
  if (obj == 0) {
    std::cout << txt << " is NULL" << std::endl;
  } else {
    std::cout << txt << " is:" << std::endl;
    obj->dump_(txt);
  }
}

// virtual
void foo::dump_(const std::string & txt) const
{
  std::cout << "dumping " << txt << " id: " << pre << id << std::endl;
  dump(txt + ".A", a);
  dump(txt + ".B", b);
}
