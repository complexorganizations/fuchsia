// Generated Code - DO NOT EDIT !!
// generated by 'emugen'

#ifndef GUARD_magma_encoder_context_t
#define GUARD_magma_encoder_context_t

#include "IOStream.h"
#include "ChecksumCalculator.h"
#include "magma_client_context.h"


#include <stdint.h>
#include "magma_enc_util.h"

struct magma_encoder_context_t : public magma_client_context_t {

	IOStream *m_stream;
	ChecksumCalculator *m_checksumCalculator;

	magma_encoder_context_t(IOStream *stream, ChecksumCalculator *checksumCalculator);
	virtual uint64_t lockAndWriteDma(void*, uint32_t) { return 0; }
};

#endif  // GUARD_magma_encoder_context_t
