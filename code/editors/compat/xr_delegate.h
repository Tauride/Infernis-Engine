#pragma once
// IX-Ray's xr_delegate alias over Infernis's fastdelegate (FastDelegate<R(Args...)>, up to 8 parameters).

#include "xrCore/fastdelegate.h"

template <typename Functor>
using xr_delegate = fastdelegate::FastDelegate<Functor>;

#define xr_make_delegate fastdelegate::MakeDelegate
