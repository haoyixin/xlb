#ifndef XLB_OPTS_H
#define XLB_OPTS_H

#include <gflags/gflags.h>

// TODO(barath): Rename these flags to something more intuitive.
DECLARE_bool(t);
DECLARE_string(i);
DECLARE_bool(f);
DECLARE_bool(k);
DECLARE_bool(d);
DECLARE_bool(a);
DECLARE_int32(c);
DECLARE_string(b);
DECLARE_int32(p);
DECLARE_string(grpc_url);
DECLARE_int32(m);
DECLARE_bool(skip_root_check);
DECLARE_string(modules);
DECLARE_bool(core_dump);
DECLARE_bool(no_crashlog);
DECLARE_int32(buffers);

DECLARE_string(n);
/*
DECLARE_bool(dpdk);
 */

#endif  // XLB_OPTS_H
