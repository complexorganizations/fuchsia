// Generated Code - DO NOT EDIT !!
// generated by 'emugen'
#ifndef __magma_client_context_t_h
#define __magma_client_context_t_h

#include "magma_client_proc.h"

#include "magma_types.h"


struct magma_client_context_t {

	magma_device_import_client_proc_t magma_device_import;
	magma_device_release_client_proc_t magma_device_release;
	magma_query_client_proc_t magma_query;
	magma_create_connection2_client_proc_t magma_create_connection2;
	magma_release_connection_client_proc_t magma_release_connection;
	magma_create_buffer_client_proc_t magma_create_buffer;
	magma_release_buffer_client_proc_t magma_release_buffer;
	magma_get_buffer_id_client_proc_t magma_get_buffer_id;
	magma_get_buffer_size_client_proc_t magma_get_buffer_size;
	magma_get_buffer_handle2_client_proc_t magma_get_buffer_handle2;
	magma_create_semaphore_client_proc_t magma_create_semaphore;
	magma_release_semaphore_client_proc_t magma_release_semaphore;
	magma_get_semaphore_id_client_proc_t magma_get_semaphore_id;
	magma_signal_semaphore_client_proc_t magma_signal_semaphore;
	magma_reset_semaphore_client_proc_t magma_reset_semaphore;
	magma_poll_client_proc_t magma_poll;
	magma_get_error_client_proc_t magma_get_error;
	magma_create_context_client_proc_t magma_create_context;
	magma_release_context_client_proc_t magma_release_context;
	magma_map_buffer_gpu_client_proc_t magma_map_buffer_gpu;
	magma_unmap_buffer_gpu_client_proc_t magma_unmap_buffer_gpu;
	magma_execute_command_client_proc_t magma_execute_command;
	magma_get_notification_channel_handle_client_proc_t magma_get_notification_channel_handle;
	magma_read_notification_channel2_client_proc_t magma_read_notification_channel2;
	virtual ~magma_client_context_t() {}

	typedef magma_client_context_t *CONTEXT_ACCESSOR_TYPE(void);
	static void setContextAccessor(CONTEXT_ACCESSOR_TYPE *f);
	int initDispatchByName( void *(*getProc)(const char *name, void *userData), void *userData);
	virtual void setError(unsigned int  error){ (void)error; }
	virtual unsigned int getError(){ return 0; }
};

#endif
