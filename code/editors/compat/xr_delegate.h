#pragma once
// IX-Ray's xr_delegate alias over Infernis's fastdelegate (FastDelegate<R(Args...)>, up to 8 parameters).

#include "xrCore/fastdelegate.h"

template <typename Functor>
using xr_delegate = fastdelegate::FastDelegate<Functor>;

// IX-Ray's MakeDelegate returns FastDelegate<R(Args...)>; Infernis's returns the numbered FastDelegateN types,
// which do not convert to it. These build the IX-Ray form.
template <class X, class Y, class R, class... Args>
fastdelegate::FastDelegate<R(Args...)> xr_make_delegate(Y* Object, R (X::*Function)(Args...))
{
    return fastdelegate::FastDelegate<R(Args...)>(Object, Function);
}

template <class X, class Y, class R, class... Args>
fastdelegate::FastDelegate<R(Args...)> xr_make_delegate(const Y* Object, R (X::*Function)(Args...) const)
{
    return fastdelegate::FastDelegate<R(Args...)>(Object, Function);
}
