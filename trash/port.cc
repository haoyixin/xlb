#include "port.h"

//#include "message.h"

namespace xlb {

    /*
Port::PortStats Port::GetPortStats() {
  PortStats ret = port_stats_;

  for (queue_t qid = 0; qid < num_queues[PACKET_DIR_INC]; qid++) {
    const QueueStats &inc = queue_stats[PACKET_DIR_INC][qid];

    ret.inc.packets += inc.packets;
    ret.inc.dropped += inc.dropped;
    ret.inc.bytes += inc.bytes;
    ret.inc.requested_hist += inc.requested_hist;
    ret.inc.actual_hist += inc.actual_hist;
    ret.inc.diff_hist += inc.diff_hist;
  }

  for (queue_t qid = 0; qid < num_queues[PACKET_DIR_OUT]; qid++) {
    const QueueStats &out = queue_stats[PACKET_DIR_OUT][qid];
    ret.out.packets += out.packets;
    ret.out.dropped += out.dropped;
    ret.out.bytes += out.bytes;
    ret.out.requested_hist += out.requested_hist;
    ret.out.actual_hist += out.actual_hist;
    ret.out.diff_hist += out.diff_hist;
  }

  return ret;
}
     */

}
