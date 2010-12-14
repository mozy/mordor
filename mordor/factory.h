#ifndef __MORDOR_FACTORY_H__
#define __MORDOR_FACTORY_H__
// Copyright (c) 2010 - Mozy, Inc.

namespace Mordor {

// Wish I could use boost functional/factory, but that's new in boost 1.43,
// which no version of Debian currently has

struct Dummy;

template <class BaseType, class T, class A1 = Dummy, class A2 = Dummy>
class Creator
{
public:
    BaseType *create0() { return new T(); }
    BaseType *create1(A1 a1) { return new T(a1); }
    BaseType *create2(A1 a1, A2 a2) { return new T(a1, a2); }
};

};

#endif
