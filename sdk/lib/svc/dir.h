// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_SVC_DIR_H_
#define LIB_SVC_DIR_H_

#include <lib/async/dispatcher.h>
#include <zircon/availability.h>
#include <zircon/compiler.h>
#include <zircon/types.h>

__BEGIN_CDECLS

typedef void(svc_connector_t)(void* context, const char* service_name, zx_handle_t service_request)
    ZX_AVAILABLE_SINCE(1);

typedef struct svc_dir svc_dir_t ZX_AVAILABLE_SINCE(1);

__EXPORT zx_status_t svc_dir_create(async_dispatcher_t* dispatcher, zx_handle_t directory_request,
                                    svc_dir_t** out_result) ZX_AVAILABLE_SINCE(1);

// Adds a service named |service_name| to the given |dir|.
//
// If |type| is non-NULL, the service will be published in a directory whose
// name matches the |type|. If |type| is NULL, the service will be published in
// the root directory.
//
// The most commonly used values for |type| are "svc", "debug", and "ctrl".
// Services published under "svc" are made available to clients via
// |fuchsia.sys.Lancher#CreateComponent|. The "debug" serivices are exposed via
// the hub. The "ctrl" services are used by the core platform to communicate
// with your program.
//
// When a client requests the service, |handler| will be called on the async_t
// passed to |svc_dir_create|. The |context| will be passed to |handler| as its
// first argument.
//
// This may fail in two ways. If an entry with the given
// |service_name| already exists, this returns
// ZX_ERR_ALREADY_EXISTS. If the provided |service_name| is invalid,
// ZX_ERR_INVALID_ARGS is returned. Otherwise, this returns ZX_OK.
__EXPORT zx_status_t svc_dir_add_service(svc_dir_t* dir, const char* type, const char* service_name,
                                         void* context, svc_connector_t* handler)
    ZX_AVAILABLE_SINCE(1);

// Adds a service named |service_name| to the given |dir| in the provided
// |path|.
//
// |path| should be a directory path delimited by "/". No leading nor trailing
// slash is allowed. If one is encountered, this function will return an
// error. If the path is empty or NULL, then the service will be installed
// under the root of |dir|.
//
// When a client requests the service, |handler| will be called on the async_t
// passed to |svc_dir_create|. The |context| will be passed to |handler| as its
// first argument.
//
// This may fail in the following ways.
// If an entry with the given |service_name| already exists, this returns
// ZX_ERR_ALREADY_EXISTS.
// If |service_name| is invalid, then ZX_ERR_INVALID_ARGS is returned.
// If |path| is malformed, then ZX_ERR_INVALID_ARGS is returned.
// Otherwise, this returns ZX_OK.
__EXPORT zx_status_t svc_dir_add_service_by_path(svc_dir_t* dir, const char* path,
                                                 const char* service_name, void* context,
                                                 svc_connector_t* handler) ZX_AVAILABLE_SINCE(7);

// Removes the service named |service_name| of type |type| from the
// given |dir|. This reports a failure if the entry does not exist, by
// returning ZX_ERR_NOT_FOUND. Otherwise, the service entry is
// removed, and ZX_OK is returned.
__EXPORT zx_status_t svc_dir_remove_service(svc_dir_t* dir, const char* type,
                                            const char* service_name) ZX_AVAILABLE_SINCE(1);

// Remove the service entry named |service_name| from the provided |path| under
// the given |dir|. This reports a failure if the entry does not exist, by
// returning ZX_ERR_NOT_FOUND. If |path| is malformed, or if either |path| or
// |service_name| is NULL, then ZX_ERR_INVALID_ARGS is returned. Otherwise,
// the service entry is removed, and ZX_OK is returned.
__EXPORT zx_status_t svc_dir_remove_service_by_path(svc_dir_t* dir, const char* path,
                                                    const char* service_name) ZX_AVAILABLE_SINCE(7);

// Destroy the provided directory. This currently cannot fail, and
// returns ZX_OK.
__EXPORT zx_status_t svc_dir_destroy(svc_dir_t* dir) ZX_AVAILABLE_SINCE(1);

__END_CDECLS

#endif  // LIB_SVC_DIR_H_
