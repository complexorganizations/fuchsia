// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Serves Client policy services.
use {
    crate::{
        client::state_machine as client_fsm,
        config_management::{
            self, Credential, NetworkConfigError, NetworkIdentifier, SaveError,
            SavedNetworksManager,
        },
        mode_management::iface_manager_api::IfaceManagerApi,
        util::listener,
    },
    anyhow::{format_err, Error},
    fidl::epitaph::ChannelEpitaphExt,
    fidl_fuchsia_wlan_common as fidl_common, fidl_fuchsia_wlan_policy as fidl_policy,
    fidl_fuchsia_wlan_sme as fidl_sme, fuchsia_zircon as zx,
    futures::{channel::oneshot, lock::Mutex, prelude::*, select, stream::FuturesUnordered},
    log::{error, info},
    std::{convert::TryFrom, sync::Arc},
};

pub mod network_selection;
pub mod scan;
pub mod state_machine;
pub mod types;

/// Max number of network configs that we will send at once through the network config iterator
/// in get_saved_networks. This depends on the maximum size of a FIDL NetworkConfig, so it may
/// need to change if a FIDL NetworkConfig or FIDL Credential changes.
const MAX_CONFIGS_PER_RESPONSE: usize = 100;

// This number was chosen arbitrarily.
const MAX_CONCURRENT_LISTENERS: usize = 1000;

type ClientRequests = fidl::endpoints::ServerEnd<fidl_policy::ClientControllerMarker>;
type SavedNetworksPtr = Arc<SavedNetworksManager>;

/// Serves the ClientProvider protocol.
/// Only one ClientController can be active. Additional requests to register ClientControllers
/// will result in their channel being immediately closed.
pub(crate) async fn serve_provider_requests(
    iface_manager: Arc<Mutex<dyn IfaceManagerApi + Send>>,
    update_sender: listener::ClientListenerMessageSender,
    saved_networks: SavedNetworksPtr,
    network_selector: Arc<network_selection::NetworkSelector>,
    mut requests: fidl_policy::ClientProviderRequestStream,
) {
    let mut controller_reqs = FuturesUnordered::new();

    loop {
        select! {
            // Progress controller requests.
            _ = controller_reqs.select_next_some() => (),
            // Process provider requests.
            req = requests.select_next_some() => if let Ok(req) = req {
                // If there is an active controller - reject new requests.
                // Rust cannot yet send Epitaphs when closing a channel, thus, simply drop the
                // request.
                if controller_reqs.is_empty() {
                    let fut = handle_provider_request(
                        Arc::clone(&iface_manager),
                        update_sender.clone(),
                        Arc::clone(&saved_networks),
                        Arc::clone(&network_selector),
                        req
                    );
                    controller_reqs.push(fut);
                } else {
                    if let Err(e) = reject_provider_request(req) {
                        error!("error sending rejection epitaph");
                    }
                }
            },
            complete => break,
        }
    }
}

/// Serves the ClientListener protocol.
pub async fn serve_listener_requests(
    update_sender: listener::ClientListenerMessageSender,
    requests: fidl_policy::ClientListenerRequestStream,
) {
    let serve_fut = requests
        .try_for_each_concurrent(MAX_CONCURRENT_LISTENERS, |req| {
            handle_listener_request(update_sender.clone(), req)
        })
        .unwrap_or_else(|e| error!("error serving Client Listener API: {}", e));
    serve_fut.await;
}

/// Handle inbound requests to acquire a new ClientController.
async fn handle_provider_request(
    iface_manager: Arc<Mutex<dyn IfaceManagerApi + Send>>,
    update_sender: listener::ClientListenerMessageSender,
    saved_networks: SavedNetworksPtr,
    network_selector: Arc<network_selection::NetworkSelector>,
    req: fidl_policy::ClientProviderRequest,
) -> Result<(), fidl::Error> {
    match req {
        fidl_policy::ClientProviderRequest::GetController { requests, updates, .. } => {
            register_listener(update_sender, updates.into_proxy()?);
            handle_client_requests(iface_manager, saved_networks, network_selector, requests)
                .await?;
            Ok(())
        }
    }
}

/// Logs a message for an incoming ClientControllerRequest
fn log_client_request(request: &fidl_policy::ClientControllerRequest) {
    info!(
        "Received policy client request {}",
        match request {
            fidl_policy::ClientControllerRequest::Connect { .. } => "Connect",
            fidl_policy::ClientControllerRequest::StartClientConnections { .. } =>
                "StartClientConnections",
            fidl_policy::ClientControllerRequest::StopClientConnections { .. } =>
                "StopClientConnections",
            fidl_policy::ClientControllerRequest::ScanForNetworks { .. } => "ScanForNetworks",
            fidl_policy::ClientControllerRequest::SaveNetwork { .. } => "SaveNetwork",
            fidl_policy::ClientControllerRequest::RemoveNetwork { .. } => "RemoveNetwork",
            fidl_policy::ClientControllerRequest::GetSavedNetworks { .. } => "GetSavedNetworks",
        }
    );
}

/// Handles all incoming requests from a ClientController.
async fn handle_client_requests(
    iface_manager: Arc<Mutex<dyn IfaceManagerApi + Send>>,
    saved_networks: SavedNetworksPtr,
    network_selector: Arc<network_selection::NetworkSelector>,
    requests: ClientRequests,
) -> Result<(), fidl::Error> {
    let mut request_stream = requests.into_stream()?;
    while let Some(request) = request_stream.try_next().await? {
        log_client_request(&request);
        match request {
            fidl_policy::ClientControllerRequest::Connect { id, responder, .. } => {
                match handle_client_request_connect(
                    Arc::clone(&iface_manager),
                    Arc::clone(&saved_networks),
                    &id,
                )
                .await
                {
                    Ok(receiver) => {
                        let response = match receiver.await {
                            Ok(()) => fidl_common::RequestStatus::Acknowledged,
                            Err(_) => fidl_common::RequestStatus::RejectedIncompatibleMode,
                        };
                        responder.send(response)?;
                    }
                    Err(error) => {
                        error!("could not connect: {:?}", error);
                        responder.send(fidl_common::RequestStatus::RejectedIncompatibleMode)?;
                    }
                }
            }
            fidl_policy::ClientControllerRequest::StartClientConnections { responder } => {
                let mut iface_manager = iface_manager.lock().await;
                let status = match iface_manager.start_client_connections().await {
                    Ok(()) => fidl_common::RequestStatus::Acknowledged,
                    Err(_) => fidl_common::RequestStatus::RejectedIncompatibleMode,
                };
                responder.send(status)?;
            }
            fidl_policy::ClientControllerRequest::StopClientConnections { responder } => {
                let mut iface_manager = iface_manager.lock().await;
                let status = match iface_manager.stop_client_connections().await {
                    Ok(()) => fidl_common::RequestStatus::Acknowledged,
                    Err(_) => fidl_common::RequestStatus::RejectedIncompatibleMode,
                };
                responder.send(status)?;
            }
            fidl_policy::ClientControllerRequest::ScanForNetworks { iterator, .. } => {
                let fut = handle_client_request_scan(
                    Arc::clone(&iface_manager),
                    iterator,
                    Arc::clone(&network_selector),
                    Arc::clone(&saved_networks),
                );
                // The scan handler is infallible and should not block handling of further request.
                // Detach the future here so we continue responding to other requests.
                fuchsia_async::Task::spawn(fut).detach();
            }
            fidl_policy::ClientControllerRequest::SaveNetwork { config, responder } => {
                // If there is an error saving the network, log it and convert to a FIDL value.
                let mut response = handle_client_request_save_network(
                    Arc::clone(&saved_networks),
                    config,
                    Arc::clone(&iface_manager),
                )
                .await
                .map_err(|e| {
                    error!("Failed to save network: {:?}", e);
                    fidl_policy::NetworkConfigChangeError::from(e)
                });
                responder.send(&mut response)?;
            }
            fidl_policy::ClientControllerRequest::RemoveNetwork { config, responder } => {
                let mut err = handle_client_request_remove_network(
                    Arc::clone(&saved_networks),
                    config,
                    iface_manager.clone(),
                )
                .map_err(|_| SaveError::GeneralError)
                .await;

                responder.send(&mut err)?;
            }
            fidl_policy::ClientControllerRequest::GetSavedNetworks { iterator, .. } => {
                handle_client_request_get_networks(Arc::clone(&saved_networks), iterator).await?;
            }
        }
    }
    Ok(())
}

/// Attempts to issue a new connect request to the currently active Client.
/// The network's configuration must have been stored before issuing a connect request.
async fn handle_client_request_connect(
    iface_manager: Arc<Mutex<dyn IfaceManagerApi + Send>>,
    saved_networks: SavedNetworksPtr,
    network: &fidl_policy::NetworkIdentifier,
) -> Result<oneshot::Receiver<()>, Error> {
    let network_config = saved_networks
        .lookup(NetworkIdentifier::new(network.ssid.clone(), network.type_.into()))
        .await
        .pop()
        .ok_or_else(|| {
            format_err!("Network not found: {}", String::from_utf8_lossy(&network.ssid))
        })?;

    let network_id = fidl_policy::NetworkIdentifier {
        ssid: network_config.ssid,
        type_: fidl_policy::SecurityType::from(network_config.security_type),
    };
    let connect_req = client_fsm::ConnectRequest {
        network: network_id,
        credential: network_config.credential.clone(),
        metadata: None,
    };

    let mut iface_manager = iface_manager.lock().await;
    iface_manager.connect(connect_req).await
}

async fn handle_client_request_scan(
    iface_manager: Arc<Mutex<dyn IfaceManagerApi + Send>>,
    output_iterator: fidl::endpoints::ServerEnd<fidl_fuchsia_wlan_policy::ScanResultIteratorMarker>,
    network_selector: Arc<network_selection::NetworkSelector>,
    saved_networks: SavedNetworksPtr,
) {
    let potentially_hidden_saved_networks =
        config_management::select_subset_potentially_hidden_networks(
            saved_networks.get_networks().await,
        );

    scan::perform_scan(
        iface_manager,
        Some(output_iterator),
        network_selector.generate_scan_result_updater(),
        scan::LocationSensorUpdater {},
        |_| {
            if potentially_hidden_saved_networks.is_empty() {
                None
            } else {
                Some(potentially_hidden_saved_networks)
            }
        },
    )
    .await
}

/// This function handles requests to save a network by saving the network and sending back to the
/// controller whether or not we successfully saved the network. There could be a FIDL error in
/// sending the response.
async fn handle_client_request_save_network(
    saved_networks: SavedNetworksPtr,
    network_config: fidl_policy::NetworkConfig,
    iface_manager: Arc<Mutex<dyn IfaceManagerApi + Send>>,
) -> Result<(), NetworkConfigError> {
    // The FIDL network config fields are defined as Options, and we consider it an error if either
    // field is missing (ie None) here.
    let net_id = network_config.id.ok_or_else(|| NetworkConfigError::ConfigMissingId)?;
    let credential = Credential::try_from(
        network_config.credential.ok_or_else(|| NetworkConfigError::ConfigMissingCredential)?,
    )?;
    let evicted_config =
        saved_networks.store(NetworkIdentifier::from(net_id.clone()), credential.clone()).await?;

    // If a config was removed, disconnect from that network.
    let mut iface_manager = iface_manager.lock().await;
    if let Some(config) = evicted_config {
        let net_id = fidl_policy::NetworkIdentifier {
            ssid: config.ssid,
            type_: config.security_type.into(),
        };
        match iface_manager.disconnect(net_id).await {
            Ok(()) => {}
            Err(e) => error!("failed to disconnect from network: {}", e),
        }
    }

    // Attempt to connect to the new network if there is an idle client interface.
    let connect_req =
        client_fsm::ConnectRequest { network: net_id, credential: credential, metadata: None };
    match iface_manager.has_idle_client().await {
        Ok(true) => {
            info!("Idle interface available, will attempt connection to new saved network");
            let _ = iface_manager.connect(connect_req).await;
        }
        Ok(false) => {}
        Err(e) => {
            error!("Unable to query idle client state while saving network: {:?}", e);
        }
    }

    Ok(())
}

/// Will remove the specified network and respond to the remove network request with a network
/// config change error if an error occurs while trying to remove the network.
async fn handle_client_request_remove_network(
    saved_networks: SavedNetworksPtr,
    network_config: fidl_policy::NetworkConfig,
    iface_manager: Arc<Mutex<dyn IfaceManagerApi + Send>>,
) -> Result<(), NetworkConfigError> {
    // The FIDL network config fields are defined as Options, and we consider it an error if either
    // field is missing (ie None) here.
    let net_id = NetworkIdentifier::from(
        network_config.id.ok_or_else(|| NetworkConfigError::ConfigMissingId)?,
    );
    let credential = Credential::try_from(
        network_config.credential.ok_or_else(|| NetworkConfigError::ConfigMissingCredential)?,
    )?;
    saved_networks.remove(net_id.clone(), credential.clone()).await?;

    match iface_manager.lock().await.disconnect(fidl_policy::NetworkIdentifier::from(net_id)).await
    {
        Ok(()) => {}
        Err(e) => error!("failed to disconnect from network: {}", e),
    }
    Ok(())
}

async fn handle_client_request_get_networks(
    saved_networks: SavedNetworksPtr,
    iterator: fidl::endpoints::ServerEnd<fidl_policy::NetworkConfigIteratorMarker>,
) -> Result<(), fidl::Error> {
    // make sufficiently small batches of networks to send and convert configs to FIDL values
    let network_configs = saved_networks.get_networks().await;
    let chunks = network_configs.chunks(MAX_CONFIGS_PER_RESPONSE);
    let fidl_chunks = chunks.into_iter().map(|chunk| {
        chunk
            .iter()
            .map(fidl_policy::NetworkConfig::from)
            .collect::<Vec<fidl_policy::NetworkConfig>>()
    });
    let mut stream = iterator.into_stream()?;
    for chunk in fidl_chunks {
        send_next_chunk(&mut stream, chunk).await?;
    }
    send_next_chunk(&mut stream, vec![]).await
}

/// Send a chunk of saved networks to the specified FIDL iterator
async fn send_next_chunk(
    stream: &mut fidl_policy::NetworkConfigIteratorRequestStream,
    chunk: Vec<fidl_policy::NetworkConfig>,
) -> Result<(), fidl::Error> {
    if let Some(req) = stream.try_next().await? {
        let fidl_policy::NetworkConfigIteratorRequest::GetNext { responder } = req;
        responder.send(&mut chunk.into_iter())
    } else {
        // This will happen if the iterator request stream was closed and we expected to send
        // another response.
        // TODO(fxbug.dev/45113) Test this error path
        info!("Info: peer closed channel for network config results unexpectedly");
        Ok(())
    }
}

/// convert from policy fidl Credential to sme fidl Credential
pub fn sme_credential_from_policy(cred: &Credential) -> fidl_sme::Credential {
    match cred {
        Credential::Password(pwd) => fidl_sme::Credential::Password(pwd.clone()),
        Credential::Psk(psk) => fidl_sme::Credential::Psk(psk.clone()),
        Credential::None => fidl_sme::Credential::None(fidl_sme::Empty {}),
    }
}

/// Handle inbound requests to register an additional ClientStateUpdates listener.
async fn handle_listener_request(
    update_sender: listener::ClientListenerMessageSender,
    req: fidl_policy::ClientListenerRequest,
) -> Result<(), fidl::Error> {
    match req {
        fidl_policy::ClientListenerRequest::GetListener { updates, .. } => {
            register_listener(update_sender, updates.into_proxy()?);
            Ok(())
        }
    }
}

/// Registers a new update listener.
/// The client's current state will be send to the newly added listener immediately.
fn register_listener(
    update_sender: listener::ClientListenerMessageSender,
    listener: fidl_policy::ClientStateUpdatesProxy,
) {
    let _ignored = update_sender.unbounded_send(listener::Message::NewListener(listener));
}

/// Rejects a ClientProvider request by sending a corresponding Epitaph via the |requests| and
/// |updates| channels.
fn reject_provider_request(req: fidl_policy::ClientProviderRequest) -> Result<(), fidl::Error> {
    match req {
        fidl_policy::ClientProviderRequest::GetController { requests, updates, .. } => {
            requests.into_channel().close_with_epitaph(zx::Status::ALREADY_BOUND)?;
            updates.into_channel().close_with_epitaph(zx::Status::ALREADY_BOUND)?;
            Ok(())
        }
    }
}

#[cfg(test)]
mod tests {
    use {
        super::*,
        crate::{
            access_point::state_machine as ap_fsm,
            config_management::{Credential, NetworkConfig, SecurityType, PSK_BYTE_LEN},
            util::{cobalt::create_mock_cobalt_sender, logger::set_logger_for_test},
        },
        async_trait::async_trait,
        fidl::endpoints::{create_proxy, create_request_stream, Proxy},
        fidl_fuchsia_stash as fidl_stash, fuchsia_async as fasync,
        futures::{channel::mpsc, lock::Mutex, task::Poll},
        pin_utils::pin_mut,
        rand::{distributions::Alphanumeric, thread_rng, Rng},
        tempfile::TempDir,
        wlan_common::assert_variant,
    };

    /// Only used to tell us what disconnect request was given to the IfaceManager so that we
    /// don't need to worry about the implementation logic in the FakeIfaceManager.
    #[derive(Debug)]
    enum IfaceManagerRequest {
        Disconnect(fidl_policy::NetworkIdentifier),
    }

    struct FakeIfaceManager {
        pub sme_proxy: fidl_fuchsia_wlan_sme::ClientSmeProxy,
        pub connect_succeeds: bool,
        pub client_connections_enabled: bool,
        pub disconnected_ifaces: Vec<u16>,
        command_sender: mpsc::Sender<IfaceManagerRequest>,
    }

    impl FakeIfaceManager {
        pub fn new(
            proxy: fidl_fuchsia_wlan_sme::ClientSmeProxy,
            command_sender: mpsc::Sender<IfaceManagerRequest>,
        ) -> Self {
            FakeIfaceManager {
                sme_proxy: proxy,
                connect_succeeds: true,
                client_connections_enabled: false,
                disconnected_ifaces: Vec::new(),
                command_sender: command_sender,
            }
        }
    }

    #[async_trait]
    impl IfaceManagerApi for FakeIfaceManager {
        async fn disconnect(
            &mut self,
            network_id: fidl_fuchsia_wlan_policy::NetworkIdentifier,
        ) -> Result<(), Error> {
            self.command_sender.try_send(IfaceManagerRequest::Disconnect(network_id)).map_err(|e| {
                error!(
                    "Failed to send disconnect: commands_sender's reciever may have
                        been dropped. FakeIfaceManager should be created manually with a sender
                        assigned: {:?}",
                    e
                );
                format_err!("failed to send disconnect: {:?}", e)
            })
        }

        async fn connect(
            &mut self,
            connect_req: client_fsm::ConnectRequest,
        ) -> Result<oneshot::Receiver<()>, Error> {
            let _ = self.disconnected_ifaces.pop();
            let mut req = fidl_sme::ConnectRequest {
                ssid: connect_req.network.ssid,
                credential: sme_credential_from_policy(&connect_req.credential),
                radio_cfg: fidl_sme::RadioConfig {
                    override_phy: false,
                    phy: fidl_common::Phy::Ht,
                    override_cbw: false,
                    cbw: fidl_common::Cbw::Cbw20,
                    override_primary_chan: false,
                    primary_chan: 0,
                },
                deprecated_scan_type: fidl_common::ScanType::Passive,
            };
            self.sme_proxy.connect(&mut req, None)?;

            let (responder, receiver) = oneshot::channel();

            // Drop the responder in the failing case.
            if self.connect_succeeds {
                let _ = responder.send(());
            }
            Ok(receiver)
        }

        async fn record_idle_client(&mut self, iface_id: u16) -> Result<(), Error> {
            Ok(self.disconnected_ifaces.push(iface_id))
        }

        async fn has_idle_client(&mut self) -> Result<bool, Error> {
            Ok(!self.disconnected_ifaces.is_empty())
        }

        async fn handle_added_iface(&mut self, _iface_id: u16) -> Result<(), Error> {
            unimplemented!()
        }

        async fn handle_removed_iface(&mut self, _iface_id: u16) -> Result<(), Error> {
            unimplemented!()
        }

        async fn scan(
            &mut self,
            mut scan_request: fidl_fuchsia_wlan_sme::ScanRequest,
        ) -> Result<fidl_fuchsia_wlan_sme::ScanTransactionProxy, Error> {
            let (local, remote) = fidl::endpoints::create_proxy()?;
            let _ = self.sme_proxy.scan(&mut scan_request, remote);
            Ok(local)
        }

        async fn stop_client_connections(&mut self) -> Result<(), Error> {
            self.client_connections_enabled = false;
            Ok(())
        }

        async fn start_client_connections(&mut self) -> Result<(), Error> {
            self.client_connections_enabled = true;
            Ok(())
        }

        async fn start_ap(
            &mut self,
            _config: ap_fsm::ApConfig,
        ) -> Result<oneshot::Receiver<()>, Error> {
            unimplemented!()
        }

        async fn stop_ap(&mut self, _ssid: Vec<u8>, _password: Vec<u8>) -> Result<(), Error> {
            unimplemented!()
        }

        async fn stop_all_aps(&mut self) -> Result<(), Error> {
            unimplemented!()
        }
    }

    /// Creates an ESS Store holding entries for protected and unprotected networks.
    async fn create_network_store(stash_id: impl AsRef<str>) -> SavedNetworksPtr {
        let temp_dir = TempDir::new().expect("failed to create temp dir");
        let path = temp_dir.path().join("networks.json");
        let tmp_path = temp_dir.path().join("tmp.json");
        let saved_networks = Arc::new(
            SavedNetworksManager::new_with_stash_or_paths(
                stash_id,
                path,
                tmp_path,
                create_mock_cobalt_sender(),
            )
            .await
            .expect("Failed to create a KnownEssStore"),
        );
        let network_id_none = NetworkIdentifier::new(b"foobar".to_vec(), SecurityType::None);
        let network_id_password =
            NetworkIdentifier::new(b"foobar-protected".to_vec(), SecurityType::Wpa2);
        let network_id_psk = NetworkIdentifier::new(b"foobar-psk".to_vec(), SecurityType::Wpa2);

        saved_networks
            .store(network_id_none, Credential::None)
            .await
            .expect("error saving network");
        saved_networks
            .store(network_id_password, Credential::Password(b"supersecure".to_vec()))
            .await
            .expect("error saving network");
        saved_networks
            .store(network_id_psk, Credential::Psk(vec![64; PSK_BYTE_LEN].to_vec()))
            .await
            .expect("error saving network foobar-psk");

        saved_networks
    }

    /// Requests a new ClientController from the given ClientProvider.
    fn request_controller(
        provider: &fidl_policy::ClientProviderProxy,
    ) -> (fidl_policy::ClientControllerProxy, fidl_policy::ClientStateUpdatesRequestStream) {
        let (controller, requests) = create_proxy::<fidl_policy::ClientControllerMarker>()
            .expect("failed to create ClientController proxy");
        let (update_sink, update_stream) =
            create_request_stream::<fidl_policy::ClientStateUpdatesMarker>()
                .expect("failed to create ClientStateUpdates proxy");
        provider.get_controller(requests, update_sink).expect("error getting controller");
        (controller, update_stream)
    }

    struct TestValues {
        saved_networks: SavedNetworksPtr,
        network_selector: Arc<network_selection::NetworkSelector>,
        provider: fidl_policy::ClientProviderProxy,
        requests: fidl_policy::ClientProviderRequestStream,
        iface_manager: Arc<Mutex<dyn IfaceManagerApi + Send>>,
        sme_stream: fidl_sme::ClientSmeRequestStream,
        update_sender: mpsc::UnboundedSender<listener::ClientListenerMessage>,
        listener_updates: mpsc::UnboundedReceiver<listener::ClientListenerMessage>,
    }

    // setup channels and proxies needed for the tests to use use the Client Provider and
    // Client Controller APIs in tests. The stash id should be the test name so that each
    // test will have a unique persistent store behind it.
    fn test_setup(
        stash_id: impl AsRef<str>,
        exec: &mut fasync::Executor,
        connect_succeeds: bool,
    ) -> TestValues {
        let saved_networks = exec.run_singlethreaded(create_network_store(stash_id));
        let network_selector = Arc::new(network_selection::NetworkSelector::new(
            Arc::clone(&saved_networks),
            create_mock_cobalt_sender(),
        ));
        let (provider, requests) = create_proxy::<fidl_policy::ClientProviderMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");

        let (proxy, server) = create_proxy::<fidl_fuchsia_wlan_sme::ClientSmeMarker>()
            .expect("failed to create ClientSmeProxy");
        let (req_sender, _req_recvr) = mpsc::channel(1);
        let mut iface_manager = FakeIfaceManager::new(proxy.clone(), req_sender);
        iface_manager.connect_succeeds = connect_succeeds;
        let iface_manager = Arc::new(Mutex::new(iface_manager));
        let sme_stream = server.into_stream().expect("failed to create ClientSmeRequestStream");

        let (update_sender, listener_updates) = mpsc::unbounded();

        set_logger_for_test();
        TestValues {
            saved_networks,
            network_selector,
            provider,
            requests,
            iface_manager,
            sme_stream,
            update_sender,
            listener_updates,
        }
    }

    /// Move stash requests forward so that a save request can progress.
    fn process_stash_write(
        mut exec: &mut fasync::Executor,
        mut stash_server: &mut fidl_stash::StoreAccessorRequestStream,
    ) {
        assert_variant!(
            exec.run_until_stalled(&mut stash_server.try_next()),
            Poll::Ready(Ok(Some(fidl_stash::StoreAccessorRequest::SetValue{..})))
        );
        process_stash_flush(&mut exec, &mut stash_server);
    }

    /// Move stash requests forward so that a remove request can progress.
    fn process_stash_remove(
        mut exec: &mut fasync::Executor,
        mut stash_server: &mut fidl_stash::StoreAccessorRequestStream,
    ) {
        assert_variant!(
            exec.run_until_stalled(&mut stash_server.try_next()),
            Poll::Ready(Ok(Some(fidl_stash::StoreAccessorRequest::DeletePrefix{..})))
        );
        process_stash_flush(&mut exec, &mut stash_server);
    }

    fn process_stash_flush(
        exec: &mut fasync::Executor,
        stash_server: &mut fidl_stash::StoreAccessorRequestStream,
    ) {
        assert_variant!(
            exec.run_until_stalled(&mut stash_server.try_next()),
            Poll::Ready(Ok(Some(fidl_stash::StoreAccessorRequest::Flush{responder}))) => {
                responder.send(&mut Ok(())).expect("failed to send stash response");
            }
        );
    }

    fn rand_string() -> String {
        thread_rng().sample_iter(&Alphanumeric).take(20).collect()
    }

    #[test]
    fn connect_request_unknown_network() {
        let ssid = b"foobar-unknown".to_vec();
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let test_values = test_setup("connect_request_unknown_network", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue connect request.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: ssid.clone(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::RejectedIncompatibleMode))
        );

        // Unknown network should not have been saved by saved networks manager
        // since we did not successfully connect.
        let lookup_fut =
            test_values.saved_networks.lookup(NetworkIdentifier::new(ssid, SecurityType::None));
        assert!(exec.run_singlethreaded(lookup_fut).is_empty());
    }

    #[test]
    fn connect_request_open_network() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");

        let mut test_values = test_setup("connect_request_open_network", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue connect request.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar".to_vec(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Verify that the connect call is acknowledged.
        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::Acknowledged))
        );

        assert_variant!(
            exec.run_until_stalled(&mut test_values.sme_stream.next()),
            Poll::Ready(Some(Ok(fidl_sme::ClientSmeRequest::Connect {
                req, ..
            }))) => {
                assert_eq!(b"foobar", &req.ssid[..]);
                assert_eq!(fidl_sme::Credential::None(fidl_sme::Empty), req.credential);
            }
        );
    }

    #[test]
    fn connect_request_protected_network() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");

        let mut test_values = test_setup("connect_request_open_network", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue connect request.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar-protected".to_vec(),
            type_: fidl_policy::SecurityType::Wpa2,
        });
        pin_mut!(connect_fut);

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Verify that the connect call is acknowledged.
        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::Acknowledged))
        );

        assert_variant!(
            exec.run_until_stalled(&mut test_values.sme_stream.next()),
            Poll::Ready(Some(Ok(fidl_sme::ClientSmeRequest::Connect {
                req, ..
            }))) => {
                assert_eq!(b"foobar-protected", &req.ssid[..]);
                assert_eq!(fidl_sme::Credential::Password(b"supersecure".to_vec()), req.credential);
                // TODO(hahnr): Send connection response.
            }
        );
    }

    #[test]
    fn connect_request_protected_psk_network() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");

        let mut test_values = test_setup("connect_request_open_network", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue connect request.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar-psk".to_vec(),
            type_: fidl_policy::SecurityType::Wpa2,
        });
        pin_mut!(connect_fut);

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Verify that the connect call is acknowledged.
        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::Acknowledged))
        );

        assert_variant!(
            exec.run_until_stalled(&mut test_values.sme_stream.next()),
            Poll::Ready(Some(Ok(fidl_sme::ClientSmeRequest::Connect {
                req, ..
            }))) => {
                assert_eq!(b"foobar-psk", &req.ssid[..]);
                assert_eq!(fidl_sme::Credential::Psk([64; PSK_BYTE_LEN].to_vec()), req.credential);
                // TODO(hahnr): Send connection response.
            }
        );
    }

    #[test]
    fn connect_request_success() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let mut test_values = test_setup("connect_request_success", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut test_values.listener_updates.next()),
            Poll::Ready(_)
        );

        // Issue connect request.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar".to_vec(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut connect_fut), Poll::Ready(Ok(_)));
    }

    #[test]
    fn connect_request_failure() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let mut test_values = test_setup("connect_request_failure", &mut exec, false);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut test_values.listener_updates.next()),
            Poll::Ready(_)
        );

        // Issue connect request.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar".to_vec(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut connect_fut), Poll::Ready(Ok(_)));
    }

    #[test]
    fn connect_request_bad_password() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let mut test_values = test_setup("connect_request_bad_password", &mut exec, false);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut test_values.listener_updates.next()),
            Poll::Ready(_)
        );

        // Issue connect request.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar".to_vec(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut connect_fut), Poll::Ready(Ok(_)));
    }

    #[test]
    fn start_and_stop_client_connections() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let test_values = test_setup("start_and_stop_client_connections", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should now be waiting for request.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue request to start client connections.
        let start_fut = controller.start_client_connections();
        pin_mut!(start_fut);

        // Request should be acknowledged.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut start_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::Acknowledged))
        );

        // Perform a connect operation.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar-protected".to_vec(),
            type_: fidl_policy::SecurityType::Wpa2,
        });
        pin_mut!(connect_fut);

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Verify that the connect call is acknowledged.
        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::Acknowledged))
        );

        // The client state machine will immediately query for status.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue request to stop client connections.
        let stop_fut = controller.stop_client_connections();
        pin_mut!(stop_fut);

        // Run the serve future until it stalls and expect the client to disconnect
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request should be acknowledged.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut stop_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::Acknowledged))
        );
    }

    /// End-to-end test of the scan function, verifying that an incoming
    /// FIDL scan request results in a scan in the SME, and that the results
    /// make it back to the requester.
    #[test]
    fn scan_end_to_end() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let mut test_values = test_setup("scan_request_sent_to_sme", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            Arc::clone(&test_values.saved_networks),
            Arc::clone(&test_values.network_selector),
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue a scan request.
        let (_iter, server) = fidl::endpoints::create_proxy().expect("failed to create iterator");
        controller.scan_for_networks(server).expect("Failed to call scan for networks");

        // Process connect request and verify connect response.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Check that a scan request was sent to the sme
        assert_variant!(
            exec.run_until_stalled(&mut test_values.sme_stream.next()),
            Poll::Ready(Some(Ok(fidl_sme::ClientSmeRequest::Scan {
                req, ..
            }))) => {
                assert_eq!(fidl_sme::ScanRequest::Passive(fidl_sme::PassiveScanRequest{}), req);
            }
        );
    }

    #[test]
    fn save_network() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let temp_dir = TempDir::new().expect("failed to create temp dir");
        let path = temp_dir.path().join(rand_string());
        let tmp_path = temp_dir.path().join(rand_string());
        let (saved_networks, mut stash_server) =
            exec.run_singlethreaded(SavedNetworksManager::new_and_stash_server(path, tmp_path));
        let saved_networks = Arc::new(saved_networks);
        let network_selector = Arc::new(network_selection::NetworkSelector::new(
            Arc::clone(&saved_networks),
            create_mock_cobalt_sender(),
        ));

        let (provider, requests) = create_proxy::<fidl_policy::ClientProviderMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");

        let test_values = test_setup("save_network_test", &mut exec, true);
        let (update_sender, mut listener_updates) = mpsc::unbounded();
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            update_sender,
            Arc::clone(&saved_networks),
            network_selector,
            requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut listener_updates.next()), Poll::Ready(_));

        // Save some network
        let network_id = fidl_policy::NetworkIdentifier {
            ssid: b"foo".to_vec(),
            type_: fidl_policy::SecurityType::None,
        };
        let network_config = fidl_policy::NetworkConfig {
            id: Some(network_id.clone()),
            credential: Some(fidl_policy::Credential::None(fidl_policy::Empty)),
        };
        let mut save_fut = controller.save_network(network_config);

        // Run server_provider forward so that it will process the save network request
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Progress forward the save and process request to stash, and also verify that the save
        // network request is not completed until stash responds.
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Pending);
        process_stash_write(&mut exec, &mut stash_server);
        // Allow save network to complete running after calls to stash
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Check that the the response says we succeeded.
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Ready(Ok(Ok(()))));

        // Check that the value was actually saved in the saved networks manager.
        let target_id = NetworkIdentifier::from(network_id);
        let target_config = NetworkConfig::new(target_id.clone(), Credential::None, false)
            .expect("Failed to create network config");
        assert_eq!(exec.run_singlethreaded(saved_networks.lookup(target_id)), vec![target_config]);
    }

    #[test]
    fn save_network_with_disconnected_iface() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let temp_dir = TempDir::new().expect("failed to create temp dir");
        let path = temp_dir.path().join(rand_string());
        let tmp_path = temp_dir.path().join(rand_string());
        let (saved_networks, mut stash_server) =
            exec.run_singlethreaded(SavedNetworksManager::new_and_stash_server(path, tmp_path));
        let saved_networks = Arc::new(saved_networks);
        let network_selector = Arc::new(network_selection::NetworkSelector::new(
            Arc::clone(&saved_networks),
            create_mock_cobalt_sender(),
        ));

        let (provider, requests) = create_proxy::<fidl_policy::ClientProviderMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");

        let test_values = test_setup("save_network_test", &mut exec, true);
        let (update_sender, mut listener_updates) = mpsc::unbounded();
        let serve_fut = serve_provider_requests(
            test_values.iface_manager.clone(),
            update_sender,
            Arc::clone(&saved_networks),
            network_selector,
            requests,
        );
        pin_mut!(serve_fut);

        // Setup the IfaceManager so that it has a disconnected iface.
        {
            let iface_manager = test_values.iface_manager.clone();
            let iface_manager_fut = iface_manager.lock();
            pin_mut!(iface_manager_fut);
            let mut iface_manager = match exec.run_until_stalled(&mut iface_manager_fut) {
                Poll::Ready(iface_manager) => iface_manager,
                Poll::Pending => panic!("expected to acquire iface_manager lock"),
            };
            let record_idle_fut = iface_manager.record_idle_client(0);
            pin_mut!(record_idle_fut);
            assert_variant!(exec.run_until_stalled(&mut record_idle_fut), Poll::Ready(Ok(())));
        }

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut listener_updates.next()), Poll::Ready(_));

        // Save some network
        let network_id = fidl_policy::NetworkIdentifier {
            ssid: b"foo".to_vec(),
            type_: fidl_policy::SecurityType::None,
        };
        let network_config = fidl_policy::NetworkConfig {
            id: Some(network_id.clone()),
            credential: Some(fidl_policy::Credential::None(fidl_policy::Empty)),
        };
        let mut save_fut = controller.save_network(network_config);

        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Pending);
        // Process save network request and requests to stash
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        process_stash_write(&mut exec, &mut stash_server);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Check that the the response says we succeeded.
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Ready(result) => {
            let save_result = result.expect("Failed to get save network response");
            assert_eq!(save_result, Ok(()));
        });

        // Check that the value was actually saved in the saved networks manager.
        let target_id = NetworkIdentifier::from(network_id);
        let target_config = NetworkConfig::new(target_id.clone(), Credential::None, false)
            .expect("Failed to create network config");
        assert_eq!(exec.run_singlethreaded(saved_networks.lookup(target_id)), vec![target_config]);
    }

    #[test]
    fn save_network_overwrite_disconnects() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let temp_dir = TempDir::new().expect("failed to create temp dir");
        let path = temp_dir.path().join(rand_string());
        let tmp_path = temp_dir.path().join(rand_string());

        // Need to create this here so that the temp files will be in scope here.
        let (saved_networks, mut stash_server) =
            exec.run_singlethreaded(SavedNetworksManager::new_and_stash_server(path, tmp_path));
        let saved_networks = Arc::new(saved_networks);
        let network_selector = Arc::new(network_selection::NetworkSelector::new(
            Arc::clone(&saved_networks),
            create_mock_cobalt_sender(),
        ));
        let (provider, requests) = create_proxy::<fidl_policy::ClientProviderMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");

        let (update_sender, mut listener_updates) = mpsc::unbounded();

        // Create a fake IfaceManager to handle client requests.
        let (proxy, _server) = create_proxy::<fidl_fuchsia_wlan_sme::ClientSmeMarker>()
            .expect("failed to create ClientSmeProxy");
        let (req_sender, mut req_recvr) = mpsc::channel(1);
        let mut iface_manager = FakeIfaceManager::new(proxy, req_sender);
        iface_manager.connect_succeeds = true;
        let iface_manager = Arc::new(Mutex::new(iface_manager));

        let serve_fut = serve_provider_requests(
            iface_manager,
            update_sender,
            Arc::clone(&saved_networks),
            network_selector,
            requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut listener_updates.next()), Poll::Ready(_));

        // Save the network directly.
        let network_id = NetworkIdentifier::new("foo", SecurityType::Wpa2);
        let credential = Credential::Password(b"password".to_vec());
        let save_fut = saved_networks.store(network_id.clone(), credential.clone());
        pin_mut!(save_fut);
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Pending);
        process_stash_write(&mut exec, &mut stash_server);
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Ready(Ok(None)));

        // Save a network with the same identifier but a different password
        let network_config = fidl_policy::NetworkConfig {
            id: Some(fidl_policy::NetworkIdentifier::from(network_id.clone())),
            credential: Some(fidl_policy::Credential::Password(b"other-password".to_vec())),
        };
        let mut save_fut = controller.save_network(network_config);

        // Process the remove request on the server side and handle requests to stash on the way.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        process_stash_write(&mut exec, &mut stash_server);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Check that the iface manager was asked to disconnect from some network
        assert_variant!(exec.run_until_stalled(&mut req_recvr.next()), Poll::Ready(Some(IfaceManagerRequest::Disconnect(net_id))) => {
            assert_eq!(net_id,fidl_policy::NetworkIdentifier::from(network_id.clone()));
        });
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Ready(Ok(Ok(()))));
    }

    #[test]
    fn save_bad_network_should_fail() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let stash_id = "save_bad_network_should_fail";
        let temp_dir = TempDir::new().expect("failed to create temp dir");
        let path = temp_dir.path().join("networks.json");
        let tmp_path = temp_dir.path().join("tmp.json");

        // Need to create this here so that the temp files will be in scope here.
        let saved_networks_fut = SavedNetworksManager::new_with_stash_or_paths(
            stash_id,
            path,
            tmp_path,
            create_mock_cobalt_sender(),
        );
        pin_mut!(saved_networks_fut);
        let _saved_networks = Arc::new(
            exec.run_singlethreaded(saved_networks_fut).expect("Failed to create a KnownEssStore"),
        );
        let saved_networks = exec.run_singlethreaded(create_network_store(stash_id));
        let network_selector = Arc::new(network_selection::NetworkSelector::new(
            Arc::clone(&saved_networks),
            create_mock_cobalt_sender(),
        ));

        let (provider, requests) = create_proxy::<fidl_policy::ClientProviderMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");

        let test_values = test_setup("save_bad_network_test", &mut exec, true);
        let (update_sender, mut listener_updates) = mpsc::unbounded();
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            update_sender,
            Arc::clone(&saved_networks),
            network_selector,
            requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut listener_updates.next()), Poll::Ready(_));

        // Create a network config whose password is too short. FIDL network config does not
        // require valid fields unlike our crate define config. We should not be able to
        // successfully save this network through the API.
        let bad_network_id = fidl_policy::NetworkIdentifier {
            ssid: b"foo".to_vec(),
            type_: fidl_policy::SecurityType::Wpa2,
        };
        let network_config = fidl_policy::NetworkConfig {
            id: Some(bad_network_id.clone()),
            credential: Some(fidl_policy::Credential::Password(b"bar".to_vec())),
        };
        // Attempt to save the config
        let mut save_fut = controller.save_network(network_config);

        // Run server_provider forward so that it will process the save network request
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Check that the the response says we failed to save the network.
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Ready(result) => {
            let error = result.expect("Failed to get save network response");
            assert_eq!(error, Err(SaveError::GeneralError));
        });

        // Check that the value was was not saved in saved networks manager.
        let target_id = NetworkIdentifier::from(bad_network_id);
        assert_eq!(exec.run_singlethreaded(saved_networks.lookup(target_id)), vec![]);
    }

    #[test]
    fn test_remove_a_network() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let temp_dir = TempDir::new().expect("failed to create temp dir");
        let path = temp_dir.path().join(rand_string());
        let tmp_path = temp_dir.path().join(rand_string());

        // Need to create this here so that the temp files will be in scope here.
        let (saved_networks, mut stash_server) =
            exec.run_singlethreaded(SavedNetworksManager::new_and_stash_server(path, tmp_path));
        let saved_networks = Arc::new(saved_networks);
        let network_selector = Arc::new(network_selection::NetworkSelector::new(
            Arc::clone(&saved_networks),
            create_mock_cobalt_sender(),
        ));
        let (provider, requests) = create_proxy::<fidl_policy::ClientProviderMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");

        let (update_sender, mut listener_updates) = mpsc::unbounded();

        // Create a fake IfaceManager to handle client requests.
        let (proxy, _server) = create_proxy::<fidl_fuchsia_wlan_sme::ClientSmeMarker>()
            .expect("failed to create ClientSmeProxy");
        let (req_sender, mut req_recvr) = mpsc::channel(1);
        let mut iface_manager = FakeIfaceManager::new(proxy, req_sender);
        iface_manager.connect_succeeds = true;
        let iface_manager = Arc::new(Mutex::new(iface_manager));

        let serve_fut = serve_provider_requests(
            iface_manager,
            update_sender,
            Arc::clone(&saved_networks),
            network_selector,
            requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(exec.run_until_stalled(&mut listener_updates.next()), Poll::Ready(_));

        // Save the network directly.
        let network_id = NetworkIdentifier::new("foo", SecurityType::None);
        let credential = Credential::None;
        let save_fut = saved_networks.store(network_id.clone(), credential.clone());
        pin_mut!(save_fut);
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Pending);
        process_stash_write(&mut exec, &mut stash_server);
        assert_variant!(exec.run_until_stalled(&mut save_fut), Poll::Ready(Ok(None)));

        // Request to remove some network
        let network_config = fidl_policy::NetworkConfig {
            id: Some(fidl_policy::NetworkIdentifier::from(network_id.clone())),
            credential: Some(fidl_policy::Credential::from(credential.clone())),
        };
        let mut remove_fut = controller.remove_network(network_config);

        // Process the remove request on the server side and handle requests to stash on the way.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        process_stash_remove(&mut exec, &mut stash_server);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Removing a network should always request a disconnect from IfaceManager, which will
        // know whether we are connected to the network to disconnect from. This checks that the
        // IfaceManager is told to disconnect (if connected).
        assert_variant!(exec.run_until_stalled(&mut req_recvr.next()), Poll::Ready(Some(IfaceManagerRequest::Disconnect(net_id))) => {
            assert_eq!(net_id,fidl_policy::NetworkIdentifier::from(network_id.clone()));
        });
        assert_variant!(exec.run_until_stalled(&mut remove_fut), Poll::Ready(Ok(Ok(()))));
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert!(exec.run_singlethreaded(saved_networks.lookup(network_id)).is_empty());
    }

    #[test]
    fn get_saved_network() {
        // save a network
        let network_id = NetworkIdentifier::new(b"foobar".to_vec(), SecurityType::Wpa2);
        let credential = Credential::Password(b"password".to_vec());
        let saved_networks = vec![(network_id.clone(), credential.clone())];

        let expected_id = network_id.into();
        let expected_credential = credential.into();
        let expected_configs = vec![fidl_policy::NetworkConfig {
            id: Some(expected_id),
            credential: Some(expected_credential),
        }];

        let expected_num_sends = 1;
        test_get_saved_networks(
            "get_saved_network",
            saved_networks,
            expected_configs,
            expected_num_sends,
        );
    }

    #[test]
    fn get_saved_networks_multiple_chunks() {
        // Save MAX_CONFIGS_PER_RESPONSE + 1 configs so that get_saved_networks should respond with
        // 2 chunks of responses plus one response with an empty vector.
        let mut saved_networks = vec![];
        let mut expected_configs = vec![];
        for index in 0..MAX_CONFIGS_PER_RESPONSE + 1 {
            // Create unique network config to be saved.
            let ssid = format!("some_config{}", index).into_bytes();
            let net_id = NetworkIdentifier::new(ssid.clone(), SecurityType::None);
            saved_networks.push((net_id, Credential::None));

            // Create corresponding FIDL value and add to list of expected configs/
            let ssid = format!("some_config{}", index).into_bytes();
            let net_id = fidl_policy::NetworkIdentifier {
                ssid: ssid,
                type_: fidl_policy::SecurityType::None,
            };
            let credential = fidl_policy::Credential::None(fidl_policy::Empty);
            let network_config =
                fidl_policy::NetworkConfig { id: Some(net_id), credential: Some(credential) };
            expected_configs.push(network_config);
        }

        let expected_num_sends = 2;
        test_get_saved_networks(
            "get_saved_networks_multiple_chunks",
            saved_networks,
            expected_configs,
            expected_num_sends,
        );
    }

    /// Test that get saved networks with the given saved networks
    /// test_id: the name of the test to create a unique persistent store for each test
    /// saved_configs: list of NetworkIdentifier and Credential pairs that are to be stored to the
    ///     SavedNetworksManager in the test.
    /// expected_configs: list of FIDL NetworkConfigs that we expect to get from get_saved_networks
    /// expected_num_sends: number of chunks of results we expect to get from get_saved_networks.
    ///     This is not counting the empty vector that signifies no more results.
    fn test_get_saved_networks(
        test_id: impl AsRef<str> + Copy,
        saved_configs: Vec<(NetworkIdentifier, Credential)>,
        expected_configs: Vec<fidl_policy::NetworkConfig>,
        expected_num_sends: usize,
    ) {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let temp_dir = TempDir::new().expect("failed to create temp dir");
        let path = temp_dir.path().join("networks.json");
        let tmp_path = temp_dir.path().join("tmp.json");
        let saved_networks = Arc::new(
            exec.run_singlethreaded(SavedNetworksManager::new_with_stash_or_paths(
                test_id,
                path,
                tmp_path,
                create_mock_cobalt_sender(),
            ))
            .expect("Failed to create a KnownEssStore"),
        );
        let network_selector = Arc::new(network_selection::NetworkSelector::new(
            Arc::clone(&saved_networks),
            create_mock_cobalt_sender(),
        ));
        let (provider, requests) = create_proxy::<fidl_policy::ClientProviderMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");

        let unused_stash = (*test_id.as_ref()).chars().rev().collect::<String>();
        let test_values = test_setup(&unused_stash, &mut exec, true);
        let (update_sender, _listener_updates) = mpsc::unbounded();
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            update_sender,
            Arc::clone(&saved_networks),
            network_selector,
            requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Save the networks specified for this test.
        for (net_id, credential) in saved_configs {
            exec.run_singlethreaded(saved_networks.store(net_id, credential))
                .expect("failed to store network");
        }

        // Request a new controller.
        let (controller, _update_stream) = request_controller(&provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue request to get the list of saved networks.
        let (iter, server) =
            fidl::endpoints::create_proxy::<fidl_policy::NetworkConfigIteratorMarker>()
                .expect("failed to create iterator");
        controller.get_saved_networks(server).expect("Failed to call get saved networks");

        // Get responses from iterator. Expect to see the specified number of responses with
        // results plus one response of an empty vector indicating the end of results.
        let mut saved_networks_results = vec![];
        for i in 0..expected_num_sends {
            let get_saved_fut = iter.get_next();
            assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
            let results = exec
                .run_singlethreaded(get_saved_fut)
                .expect("Failed to get next chunk of saved networks results");
            // the size of received chunk should either be max chunk size or whatever is left
            // to receive in the last chunk
            if i < expected_num_sends - 1 {
                assert_eq!(results.len(), MAX_CONFIGS_PER_RESPONSE);
            } else {
                assert_eq!(results.len(), expected_configs.len() % MAX_CONFIGS_PER_RESPONSE);
            }
            saved_networks_results.extend(results);
        }
        let get_saved_end_fut = iter.get_next();
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        let results = exec
            .run_singlethreaded(get_saved_end_fut)
            .expect("Failed to get next chunk of saved networks results");
        assert!(results.is_empty());

        // check whether each network we saved is in the results and that nothing else is there.
        for network_config in &expected_configs {
            assert!(saved_networks_results.contains(&network_config));
        }
        assert_eq!(expected_configs.len(), saved_networks_results.len());
    }

    #[test]
    fn register_update_listener() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let mut test_values = test_setup("register_update_listener", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            test_values.saved_networks,
            test_values.network_selector,
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a new controller.
        let (_controller, _update_stream) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut test_values.listener_updates.next()),
            Poll::Ready(Some(listener::Message::NewListener(_)))
        );
    }

    #[test]
    fn get_listener() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let (listener, requests) = create_proxy::<fidl_policy::ClientListenerMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");

        let (update_sender, mut listener_updates) = mpsc::unbounded();
        let serve_fut = serve_listener_requests(update_sender, requests);
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Register listener.
        let (update_sink, _update_stream) =
            create_request_stream::<fidl_policy::ClientStateUpdatesMarker>()
                .expect("failed to create ClientStateUpdates proxy");
        listener.get_listener(update_sink).expect("error getting listener");

        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut listener_updates.next()),
            Poll::Ready(Some(listener::Message::NewListener(_)))
        );
    }

    #[test]
    fn multiple_controllers_write_attempt() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let test_values = test_setup("multiple_controllers_write_attempt", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            test_values.saved_networks,
            test_values.network_selector,
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a controller.
        let (controller1, _) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request another controller.
        let (controller2, _) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Ensure first controller is operable. Issue connect request.
        let connect_fut = controller1.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar".to_vec(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request from first controller. Verify success.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::Acknowledged))
        );

        // Ensure second controller is not operable. Issue connect request.
        let connect_fut = controller2.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar".to_vec(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request from second controller. Verify failure.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Err(fidl::Error::ClientChannelClosed {status: zx::Status::ALREADY_BOUND, .. }))
        );

        // Drop first controller. A new controller can now take control.
        drop(controller1);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request another controller.
        let (controller3, _) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Ensure third controller is operable. Issue connect request.
        let connect_fut = controller3.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar".to_vec(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request from third controller. Verify success.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::Acknowledged))
        );
    }

    #[test]
    fn multiple_controllers_epitaph() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let test_values = test_setup("multiple_controllers_epitaph", &mut exec, true);
        let serve_fut = serve_provider_requests(
            test_values.iface_manager,
            test_values.update_sender,
            test_values.saved_networks.clone(),
            test_values.network_selector,
            test_values.requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a controller.
        let (_controller1, _) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request another controller.
        let (controller2, _) = request_controller(&test_values.provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        let chan = controller2.into_channel().expect("error turning proxy into channel");
        let mut buffer = zx::MessageBuf::new();
        let epitaph_fut = chan.recv_msg(&mut buffer);
        pin_mut!(epitaph_fut);
        assert_variant!(exec.run_until_stalled(&mut epitaph_fut), Poll::Ready(Ok(_)));

        // Verify Epitaph was received.
        use fidl::encoding::{decode_transaction_header, Decodable, Decoder, EpitaphBody};
        let (header, tail) =
            decode_transaction_header(buffer.bytes()).expect("failed decoding header");
        let mut msg = Decodable::new_empty();
        Decoder::decode_into::<EpitaphBody>(&header, tail, &mut [], &mut msg)
            .expect("failed decoding body");
        assert_eq!(msg.error, zx::Status::ALREADY_BOUND);
        assert!(chan.is_closed());
    }

    struct FakeIfaceManagerNoIfaces {}

    #[async_trait]
    impl IfaceManagerApi for FakeIfaceManagerNoIfaces {
        async fn disconnect(
            &mut self,
            _network_id: fidl_fuchsia_wlan_policy::NetworkIdentifier,
        ) -> Result<(), Error> {
            Err(format_err!("No ifaces"))
        }

        async fn connect(
            &mut self,
            _connect_req: client_fsm::ConnectRequest,
        ) -> Result<oneshot::Receiver<()>, Error> {
            Err(format_err!("No ifaces"))
        }

        async fn record_idle_client(&mut self, _iface_id: u16) -> Result<(), Error> {
            unimplemented!()
        }

        async fn has_idle_client(&mut self) -> Result<bool, Error> {
            unimplemented!()
        }

        async fn handle_added_iface(&mut self, _iface_id: u16) -> Result<(), Error> {
            unimplemented!()
        }

        async fn handle_removed_iface(&mut self, _iface_id: u16) -> Result<(), Error> {
            unimplemented!()
        }

        async fn scan(
            &mut self,
            _scan_request: fidl_sme::ScanRequest,
        ) -> Result<fidl_fuchsia_wlan_sme::ScanTransactionProxy, Error> {
            Err(format_err!("No ifaces"))
        }

        async fn stop_client_connections(&mut self) -> Result<(), Error> {
            unimplemented!()
        }

        async fn start_client_connections(&mut self) -> Result<(), Error> {
            unimplemented!()
        }

        async fn start_ap(
            &mut self,
            _config: ap_fsm::ApConfig,
        ) -> Result<oneshot::Receiver<()>, Error> {
            unimplemented!()
        }

        async fn stop_ap(&mut self, _ssid: Vec<u8>, _password: Vec<u8>) -> Result<(), Error> {
            unimplemented!()
        }

        async fn stop_all_aps(&mut self) -> Result<(), Error> {
            unimplemented!()
        }
    }

    #[test]
    fn no_client_interface() {
        let mut exec = fasync::Executor::new().expect("failed to create an executor");
        let stash_id = "no_client_interface";
        let saved_networks = exec.run_singlethreaded(create_network_store(stash_id));
        let network_selector = Arc::new(network_selection::NetworkSelector::new(
            Arc::clone(&saved_networks),
            create_mock_cobalt_sender(),
        ));
        let iface_manager = Arc::new(Mutex::new(FakeIfaceManagerNoIfaces {}));

        let (provider, requests) = create_proxy::<fidl_policy::ClientProviderMarker>()
            .expect("failed to create ClientProvider proxy");
        let requests = requests.into_stream().expect("failed to create stream");
        let (update_sender, _listener_updates) = mpsc::unbounded();
        let serve_fut = serve_provider_requests(
            iface_manager,
            update_sender,
            saved_networks,
            network_selector,
            requests,
        );
        pin_mut!(serve_fut);

        // No request has been sent yet. Future should be idle.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Request a controller.
        let (controller, _) = request_controller(&provider);
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);

        // Issue connect request.
        let connect_fut = controller.connect(&mut fidl_policy::NetworkIdentifier {
            ssid: b"foobar".to_vec(),
            type_: fidl_policy::SecurityType::None,
        });
        pin_mut!(connect_fut);

        // Process connect request from first controller. Verify success.
        assert_variant!(exec.run_until_stalled(&mut serve_fut), Poll::Pending);
        assert_variant!(
            exec.run_until_stalled(&mut connect_fut),
            Poll::Ready(Ok(fidl_common::RequestStatus::RejectedIncompatibleMode))
        );
    }

    // Gets a saved network config with a particular SSID, security type, and credential.
    // If there are more than one configs saved for the same SSID, security type, and credential,
    // the function will panic.
    async fn get_config(
        saved_networks: Arc<SavedNetworksManager>,
        id: NetworkIdentifier,
        cred: Credential,
    ) -> Option<NetworkConfig> {
        let mut cfgs = saved_networks
            .lookup(id)
            .await
            .into_iter()
            .filter(|cfg| cfg.credential == cred)
            .collect::<Vec<_>>();
        // there should not be multiple configs with the same SSID, security type, and credential.
        assert!(cfgs.len() <= 1);
        cfgs.pop()
    }

    #[fasync::run_singlethreaded(test)]
    async fn get_correct_config() {
        let temp_dir = tempfile::TempDir::new().expect("failed to create temp dir");
        let path = temp_dir.path().join("networks.json");
        let tmp_path = temp_dir.path().join("tmp.json");
        let stash_id = "get_correct_config";
        let saved_networks = Arc::new(
            SavedNetworksManager::new_with_stash_or_paths(
                stash_id,
                path,
                tmp_path,
                create_mock_cobalt_sender(),
            )
            .await
            .expect("Failed to create SavedNetworksManager"),
        );
        let network_id = NetworkIdentifier::new(b"foo".to_vec(), SecurityType::Wpa2);
        let cfg = NetworkConfig::new(
            network_id.clone(),
            Credential::Password(b"password".to_vec()),
            false,
        )
        .expect("Failed to create network config");

        saved_networks
            .store(network_id.clone(), Credential::Password(b"password".to_vec()))
            .await
            .expect("Failed to store network config");

        assert_eq!(
            Some(cfg),
            get_config(
                Arc::clone(&saved_networks),
                network_id,
                Credential::Password(b"password".to_vec())
            )
            .await
        );
        assert_eq!(
            None,
            get_config(
                Arc::clone(&saved_networks),
                NetworkIdentifier::new(b"foo".to_vec(), SecurityType::Wpa2),
                Credential::Password(b"not-saved".to_vec())
            )
            .await
        );
    }
}
