#pragma once

/* xbuf and mbuf share the same start address, so that we can avoid conversion.
 *
 * Layout (2560 bytes):
 *    Offset	Size	Field
 *  - 0		128	mbuf (XBUF_MBUF == sizeof(struct rte_mbuf))
 *  - 128	64	some read-only/immutable fields
 *  - 192	128	static/dynamic metadata fields
 *  - 320	64	private area for module/port's internal use
 *  - 384	128	_headroom (XBUF_HEADROOM == RTE_PKTMBUF_HEADROOM)
 *  - 512	2048	_data (XBUF_DATA)
 *
 * Stride will be 2624B, because of mempool's per-object header which takes 64B.
 *
 * Invariants:
 *  * When packets are newly allocated, the data should be filled from _data.
 *  * The packet data may reside in the _headroom + _data areas,
 *    but its size must not exceed 2048 (XBUF_DATA) when passed to a port.
 */
#define XBUF_MBUF 128
#define XBUF_IMMUTABLE 64
#define XBUF_METADATA 128
#define XBUF_SCRATCHPAD 64
#define XBUF_RESERVE (XBUF_IMMUTABLE + XBUF_METADATA + XBUF_SCRATCHPAD)
#define XBUF_HEADROOM 128
#define XBUF_DATA 2048

#define XBUF_MBUF_OFF 0
#define XBUF_IMMUTABLE_OFF XBUF_MBUF
#define XBUF_METADATA_OFF (XBUF_IMMUTABLE_OFF + XBUF_IMMUTABLE)
#define XBUF_SCRATCHPAD_OFF (XBUF_METADATA_OFF + XBUF_METADATA)
#define XBUF_HEADROOM_OFF (XBUF_SCRATCHPAD_OFF + XBUF_SCRATCHPAD)
#define XBUF_DATA_OFF (XBUF_HEADROOM_OFF + XBUF_HEADROOM)

#define XBUF_SIZE (XBUF_DATA_OFF + XBUF_DATA)
