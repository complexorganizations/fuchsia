// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    crate::{
        capability::CapabilityProvider,
        model::{
            error::ModelError,
            events::{
                error::EventsError,
                registry::{
                    EventRegistry, EventSubscription, ExecutionMode, SubscriptionOptions,
                    SubscriptionType,
                },
                serve::serve_event_source_sync,
                stream::EventStream,
                stream_provider::EventStreamProvider,
            },
            model::Model,
        },
    },
    async_trait::async_trait,
    cm_task_scope::TaskScope,
    cm_util::channel,
    fidl::endpoints::ServerEnd,
    fidl_fuchsia_io as fio, fidl_fuchsia_sys2 as fsys, fuchsia_zircon as zx,
    moniker::ExtendedMoniker,
    std::{path::PathBuf, sync::Weak},
};

/// A system responsible for implementing basic events functionality on a scoped realm.
#[derive(Clone)]
pub struct EventSource {
    /// The component model, needed to route events.
    // TODO(fxbug.dev/84729)
    #[allow(unused)]
    model: Weak<Model>,

    /// A shared reference to the event registry used to subscribe and dispatch events.
    registry: Weak<EventRegistry>,

    /// The static EventStreamProvider tracks all static event streams. It can be used to take the
    /// server end of the static event streams.
    stream_provider: Weak<EventStreamProvider>,

    /// The options used to subscribe to events.
    options: SubscriptionOptions,
}

impl EventSource {
    /// Creates a new `EventSource` that will be used by the component identified with the given
    /// `target_moniker`.
    pub async fn new(
        model: Weak<Model>,
        options: SubscriptionOptions,
        registry: Weak<EventRegistry>,
        stream_provider: Weak<EventStreamProvider>,
    ) -> Result<Self, ModelError> {
        Ok(Self { registry, stream_provider, model, options })
    }

    pub async fn new_for_debug(
        model: Weak<Model>,
        registry: Weak<EventRegistry>,
        stream_provider: Weak<EventStreamProvider>,
    ) -> Result<Self, ModelError> {
        Self::new(
            model,
            SubscriptionOptions::new(SubscriptionType::AboveRoot, ExecutionMode::Debug),
            registry,
            stream_provider,
        )
        .await
    }

    /// Subscribes to events provided in the `events` vector.
    ///
    /// The client might request to subscribe to events that it's not allowed to see. Events
    /// that are allowed should have been defined in its manifest and either offered to it or
    /// requested from the current realm.
    pub async fn subscribe(
        &mut self,
        requests: Vec<EventSubscription>,
    ) -> Result<EventStream, ModelError> {
        let registry = self.registry.upgrade().ok_or(EventsError::RegistryNotFound)?;
        // Create an event stream for the given events
        registry.subscribe(&self.options, requests).await
    }

    pub async fn take_static_event_stream(
        &self,
        target_path: String,
    ) -> Option<ServerEnd<fsys::EventStreamMarker>> {
        let moniker = match &self.options.subscription_type {
            SubscriptionType::AboveRoot => ExtendedMoniker::ComponentManager,
            SubscriptionType::Component(abs_moniker) => {
                ExtendedMoniker::ComponentInstance(abs_moniker.clone())
            }
        };
        if let Some(stream_provider) = self.stream_provider.upgrade() {
            return stream_provider.take_static_event_stream(&moniker, target_path).await;
        }
        return None;
    }

    /// Serves a `EventSource` FIDL protocol.
    pub async fn serve(self, stream: fsys::EventSourceRequestStream) {
        serve_event_source_sync(self, stream).await;
    }
}

#[async_trait]
impl CapabilityProvider for EventSource {
    async fn open(
        self: Box<Self>,
        task_scope: TaskScope,
        _flags: fio::OpenFlags,
        _open_mode: u32,
        _relative_path: PathBuf,
        server_end: &mut zx::Channel,
    ) -> Result<(), ModelError> {
        let server_end = channel::take_channel(server_end);
        let stream = ServerEnd::<fsys::EventSourceMarker>::new(server_end)
            .into_stream()
            .expect("could not convert channel into stream");
        task_scope
            .add_task(async move {
                serve_event_source_sync(*self, stream).await;
            })
            .await;
        Ok(())
    }
}
