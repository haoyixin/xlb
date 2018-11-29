// Copyright (c) 2018-2019, Qihoo 360 Technology Co. Ltd.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// * Neither the names of the copyright holders nor the names of their
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "numa.h"

#include "format.h"
#include <fstream>
#include <limits.h>
#include <unistd.h>

namespace xlb {
namespace utils {

/* Check if a cpu is present by the presence of the cpu information for it */
int is_cpu_present(unsigned int core_id) {
  char path[PATH_MAX];
  int len = snprintf(path, sizeof(path), SYS_CPU_DIR "/" CORE_ID_FILE, core_id);
  if (len <= 0 || (unsigned)len >= sizeof(path)) {
    return 0;
  }
  if (access(path, F_OK) != 0) {
    return 0;
  }

  return 1;
}

int NumNumaNodes() {
  static int cached = 0;
  if (cached > 0) {
    return cached;
  }

  std::ifstream fp("/sys/devices/system/node/possible");
  if (fp.is_open()) {
    std::string line;
    if (std::getline(fp, line)) {
      int cnt;
      if (Parse(line, "0-%d", &cnt) == 1) {
        cached = cnt + 1;
        return cached;
      }
    }
  }
}

} // namespace utils
} // namespace xlb
