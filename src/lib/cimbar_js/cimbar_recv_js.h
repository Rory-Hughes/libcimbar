/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#ifndef CIMBAR_RECV_JS_API_H
#define CIMBAR_RECV_JS_API_H

// Compatibility API ownership contract:
// The cimbard_* receiver functions below share process-global decoder,
// reassembly, decompression, image, and reporting state. They must be invoked
// serially by one worker/thread. Concurrent callers, or a reset/configuration
// call racing an active decode, are unsupported. The hardened byte-only
// transport profile does not link or expose this compatibility API.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned cimbard_get_report(unsigned char* buff, unsigned maxlen);
unsigned cimbard_get_debug(unsigned char* buff, unsigned maxlen);

#define CIMBARD_MAX_FRAME_PIXELS (UINT64_C(4096) * UINT64_C(4096))

enum cimbard_pixel_format {
	CIMBARD_PIXEL_FORMAT_RGB = 3,
	CIMBARD_PIXEL_FORMAT_RGBA = 4,
	CIMBARD_PIXEL_FORMAT_NV12 = 12,
	CIMBARD_PIXEL_FORMAT_I420 = 420
};

enum cimbard_scan_result {
	CIMBARD_SCAN_INVALID_DIMENSIONS = -1,
	CIMBARD_SCAN_OUTPUT_BUFFER_TOO_SMALL = -2,
	CIMBARD_SCAN_EXTRACT_FAILED = -3,
	CIMBARD_SCAN_NULL_POINTER = -4,
	CIMBARD_SCAN_UNSUPPORTED_FORMAT = -5,
	CIMBARD_SCAN_FRAME_TOO_LARGE = -6,
	CIMBARD_SCAN_INVALID_BUFFER_SIZE = -7,
	CIMBARD_SCAN_PROCESSING_ERROR = -8
};

// imgsize must exactly match the tightly packed frame dimensions and format.
// output of scan is stored in `bufspace`
int cimbard_get_bufsize();
int cimbard_scan_extract_decode_checked(const unsigned char* imgdata, size_t imgsize, unsigned imgw, unsigned imgh, int format, unsigned char* bufspace, unsigned bufsize);

// Compatibility wrapper for callers that cannot provide imgsize. New callers
// should use cimbard_scan_extract_decode_checked().
int cimbard_scan_extract_decode(const unsigned char* imgdata, unsigned imgw, unsigned imgh, int format, unsigned char* bufspace, unsigned bufsize);

// returns id of final file (can be used to get size of `finish_copy`'s buffer) if complete, 0 if success, negative on error
// persists state, the return value (if >0) corresponds to a uint32_t id
int64_t cimbard_fountain_decode(const unsigned char* buffer, unsigned size);

// get compressed filesize from id
// you probably don't need to use this.
unsigned cimbard_get_filesize(uint32_t id);

// if fountain_decode returned a >0 value,
//  get filename and (partial) contents from reassembled file
// wherever a uint32_t id is passed, it should be in the
//  same js shared memory as the fountain_decode() call
// cimbard_decompress_read() will return 0 when all file contents have been read
int cimbard_get_filename(uint32_t id, char* filename, unsigned fnsize);
int cimbard_get_decompress_bufsize();
int cimbard_decompress_read(uint32_t id, unsigned char* buffer, unsigned size);

// Ends the current decoder session and clears transfer and recovered-output state.
int cimbard_reset_decode(void);
int cimbard_configure_decode(int mode_val);

// testing usage only!
unsigned char* cimbard_get_reassembled_file_buff();

#ifdef __cplusplus
}
#endif

#endif // CIMBAR_RECV_JS_API_H
