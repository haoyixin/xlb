#include "utils/copy.h"

namespace xlb::utils {

void CopyNonInlined(void *__restrict__ dst, const void *__restrict__ src,
                    size_t bytes, bool sloppy) {
  CopyInlined(dst, src, bytes, sloppy);
}

}  // namespace xlb::utils
