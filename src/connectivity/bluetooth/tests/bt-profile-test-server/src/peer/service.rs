// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    bt_rfcomm::ServerChannel,
    fidl_fuchsia_bluetooth_bredr::ServiceClassProfileIdentifier,
    fuchsia_bluetooth::{profile::Psm, types::PeerId},
    log::warn,
    slab::Slab,
    std::collections::{HashMap, HashSet},
};

use crate::types::{RegisteredServiceId, ServiceRecord};

/// The unique handle assigned to a group of registered services.
/// Multiple services that are registered together will be assigned
/// the same RegistrationHandle.
pub type RegistrationHandle = usize;

/// The unique handle assigned to each registered service.
pub type ServiceHandle = usize;

/// Handles the registration and unregistration of services. Stores the registered
/// services as `ServiceRecords` and provides convenience methods to access
/// services indexed by their ServiceClassProfileIdentifier.
/// Despite similarities, `ServiceSet` does not conform to the SDP protocol,
/// and provides no guarantees in that regard.
pub struct ServiceSet {
    /// The Peer Id that this `ServiceSet` represents.
    peer_id: PeerId,

    /// There is one RegistrationHandle assigned to a set of services that are
    /// registered together.
    reg_to_service: HashMap<RegistrationHandle, HashSet<ServiceHandle>>,

    /// Each service, stored as a ServiceRecord, is assigned a unique ServiceHandle.
    records: Slab<ServiceRecord>,

    /// A single PSM can be specified by multiple services. However, these services
    /// must be registered together. Therefore, each PSM is uniquely mapped to a
    /// RegistrationHandle.
    psm_to_handle: HashMap<Psm, RegistrationHandle>,

    /// A single RFCOMM Server Channel can only be specified by a single service and therefore
    /// a RegistrationHandle.
    rfcomm_channel_to_handle: HashMap<ServerChannel, RegistrationHandle>,

    /// The ServiceClassIds supported by each registration. This is used to speed
    /// up the matching to a service search.
    reg_to_svc_ids: HashMap<RegistrationHandle, HashSet<ServiceClassProfileIdentifier>>,
}

impl ServiceSet {
    pub fn new(peer_id: PeerId) -> Self {
        Self {
            peer_id,
            reg_to_service: HashMap::new(),
            records: Slab::new(),
            psm_to_handle: HashMap::new(),
            rfcomm_channel_to_handle: HashMap::new(),
            reg_to_svc_ids: HashMap::new(),
        }
    }

    /// Returns a HashSet of ServiceClassProfileIds that are registered to a particular
    /// RegistrationHandle.
    pub fn get_service_ids_for_registration_handle(
        &self,
        handle: &RegistrationHandle,
    ) -> Option<&HashSet<ServiceClassProfileIdentifier>> {
        self.reg_to_svc_ids.get(handle)
    }

    /// Returns the RegistrationHandle of the service specifying the PSM, or
    /// None if not registered.
    pub fn psm_registered(&self, psm: Psm) -> Option<RegistrationHandle> {
        self.psm_to_handle.get(&psm).cloned()
    }

    /// Returns the RegistrationHandle of the service specifying the RFCOMM `channel` number, or
    /// None if not registered.
    pub fn rfcomm_channel_registered(&self, channel: ServerChannel) -> Option<RegistrationHandle> {
        self.rfcomm_channel_to_handle.get(&channel).cloned()
    }

    /// Returns a map of ServiceRecords (if any), that conform to the provided Service Class `ids`.
    /// Convenience function for speeding up the matching process.
    pub fn get_service_records(
        &self,
        ids: &HashSet<ServiceClassProfileIdentifier>,
    ) -> HashMap<ServiceClassProfileIdentifier, Vec<ServiceRecord>> {
        let mut records_map = HashMap::new();
        for (_, record) in &self.records {
            for id in ids {
                if record.contains_service_class_identifier(id) {
                    records_map.entry(id.clone()).or_insert(Vec::new()).push(record.clone());
                }
            }
        }
        records_map
    }

    /// Attempts to register the group of services specified by `records`.
    ///
    /// Returns None if:
    ///   1) No service records are provided.
    ///   2) A ServiceRecord contains an already registered PSM.
    ///
    /// Assigns a unique ServiceHandle to each registered ServiceRecord.
    ///
    /// Returns a RegistrationHandle, uniquely identifying the services that were registered
    /// together. The returned handle should be used to unregister the group of services using
    /// Self::unregister_service().
    pub fn register_service(&mut self, records: Vec<ServiceRecord>) -> Option<RegistrationHandle> {
        if records.is_empty() {
            return None;
        }

        // Any new service must not request an already allocated L2CAP PSM / RFCOMM channel.
        let existing_psms: HashSet<Psm> = self.psm_to_handle.keys().cloned().collect();
        let new_psms = records.iter().map(|record| record.psms()).flatten().collect();
        if !existing_psms.is_disjoint(&new_psms) {
            warn!("PSM already registered");
            return None;
        }
        let existing_rfcomm_channels: HashSet<ServerChannel> =
            self.rfcomm_channel_to_handle.keys().cloned().collect();
        let new_rfcomm_channels =
            records.iter().map(|record| record.rfcomm_channels()).flatten().collect();
        if !existing_rfcomm_channels.is_disjoint(&new_rfcomm_channels) {
            warn!("RFCOMM channel already registered");
            return None;
        }

        let mut assigned_handles = HashSet::new();
        let mut service_class_ids: HashSet<ServiceClassProfileIdentifier> = HashSet::new();

        // Each service is valid now. Register each record and store the relevant metadata.
        for mut record in records {
            let entry = self.records.vacant_entry();
            let next: ServiceHandle = entry.key();

            // Register the ServiceRecord with the unique PeerId,ServiceHandle combination.
            record.register_service_record(RegisteredServiceId::new(self.peer_id, next));

            // Update the set of ServiceClassIds, save the assigned handle, and store the
            // ServiceRecord.
            service_class_ids = service_class_ids.union(record.service_ids()).cloned().collect();
            assigned_handles.insert(next);
            entry.insert(record);
        }

        // The RegistrationHandle is the smallest handle in the (nonempty) `assigned_handles`.
        let registration_handle = assigned_handles.iter().min().cloned().expect("is nonempty");
        // Save metadata to speed up the matching process.
        self.reg_to_svc_ids.insert(registration_handle, service_class_ids);
        self.reg_to_service.insert(registration_handle, assigned_handles);
        new_psms.iter().for_each(|psm| {
            self.psm_to_handle.insert(*psm, registration_handle);
        });
        new_rfcomm_channels.iter().for_each(|sc| {
            self.rfcomm_channel_to_handle.insert(*sc, registration_handle);
        });

        Some(registration_handle)
    }

    /// Unregisters the service(s) associated with the registered `handle`.
    ///
    /// Returns the set of removed services.
    pub fn unregister_service(&mut self, handle: &RegistrationHandle) -> Vec<ServiceRecord> {
        let mut removed_services = Vec::new();
        // Remove the registered handle.
        let removed = self.reg_to_service.remove(handle);
        if removed.is_none() {
            return removed_services;
        }

        // Update the ServiceClassProfileId cache.
        self.reg_to_svc_ids.remove(handle);

        for svc_handle in removed.expect("just checked") {
            if self.records.contains(svc_handle) {
                // Remove the entry for the service record.
                let removed = self.records.remove(svc_handle);
                // Update the PSM and RFCOMM channel caches.
                removed.psms().iter().for_each(|psm| {
                    self.psm_to_handle.remove(psm);
                });
                removed.rfcomm_channels().iter().for_each(|channel| {
                    self.rfcomm_channel_to_handle.remove(channel);
                });
                removed_services.push(removed);
            }
        }
        removed_services
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use {matches::assert_matches, std::convert::TryFrom};

    use crate::{
        profile::tests::{build_a2dp_service_definition, build_rfcomm_service_definition},
        types::Connection,
    };

    fn build_avrcp_service_record(psm: Psm) -> ServiceRecord {
        let mut service_ids = HashSet::new();
        service_ids.insert(ServiceClassProfileIdentifier::AvRemoteControl);
        service_ids.insert(ServiceClassProfileIdentifier::AvRemoteControlController);

        ServiceRecord::new(
            service_ids,
            Some(Connection::L2cap(psm)),
            HashSet::new(),
            vec![],
            vec![],
        )
    }

    #[test]
    fn test_register_service_success() {
        let id = PeerId(123);
        let mut manager = ServiceSet::new(id);

        // Single, valid record, is successful.
        let psm0 = Psm::new(19);
        let (_, single_record) = build_a2dp_service_definition(psm0);
        let reg_handle = manager.register_service(vec![single_record]);
        assert!(reg_handle.is_some());
        assert_eq!(reg_handle, manager.psm_registered(psm0));

        // The relevant ServiceClassProfileIds should be registered for this handle.
        let mut expected_ids = HashSet::new();
        expected_ids.insert(ServiceClassProfileIdentifier::AudioSink);
        let service_ids =
            manager.get_service_ids_for_registration_handle(&reg_handle.unwrap()).unwrap();
        assert_eq!(expected_ids, service_ids.clone());

        // Multiple, valid records, is successful.
        let psm1 = Psm::new(20);
        let psm2 = Psm::new(21);
        let (_, record1) = build_a2dp_service_definition(psm1);
        let (_, record2) = build_a2dp_service_definition(psm2);
        let reg_handle2 = manager.register_service(vec![record1, record2]);
        assert!(reg_handle2.is_some());
        assert_eq!(reg_handle2, manager.psm_registered(psm1));
        assert_eq!(reg_handle2, manager.psm_registered(psm2));

        // Multiple records with overlapping PSMs is successful since they are registered together.
        let (psm3, psm4) = (Psm::new(22), Psm::new(23));
        let (_, record4) = build_a2dp_service_definition(psm4);
        let overlapping =
            vec![build_avrcp_service_record(psm3), build_avrcp_service_record(psm3), record4];
        let reg_handle3 = manager.register_service(overlapping);
        assert!(reg_handle3.is_some());
        assert_eq!(reg_handle3, manager.psm_registered(psm3));
        assert_eq!(reg_handle3, manager.psm_registered(psm4));

        expected_ids.insert(ServiceClassProfileIdentifier::AvRemoteControl);
        expected_ids.insert(ServiceClassProfileIdentifier::AvRemoteControlController);
        let service_ids = manager.get_service_ids_for_registration_handle(&reg_handle3.unwrap());
        assert!(service_ids.is_some());
        let service_ids = service_ids.unwrap();
        assert_eq!(&expected_ids, service_ids);

        // Unregistering a service should succeed. Only the relevant services should be removed.
        assert_eq!(3, manager.unregister_service(&reg_handle3.unwrap()).len());
        assert_eq!(reg_handle, manager.psm_registered(psm0));
        assert_eq!(reg_handle2, manager.psm_registered(psm1));
        assert_eq!(reg_handle2, manager.psm_registered(psm2));
        assert_eq!(None, manager.psm_registered(psm3));
        assert_eq!(None, manager.psm_registered(psm4));

        // Unregistering the same service shouldn't do anything.
        assert_eq!(0, manager.unregister_service(&reg_handle3.unwrap()).len());
    }

    #[test]
    fn register_empty_service_is_error() {
        let mut manager = ServiceSet::new(PeerId(16));
        let empty_records = vec![];
        assert_eq!(manager.register_service(empty_records), None);
    }

    #[test]
    fn register_duplicate_services_is_error() {
        let id = PeerId(123);
        let mut manager = ServiceSet::new(id);

        let psm = Psm::new(19);
        let (_, single_record) = build_a2dp_service_definition(psm);
        assert!(manager.register_service(vec![single_record.clone()]).is_some());
        // Attempting to register the same PSM fails the second time.
        assert_eq!(manager.register_service(vec![single_record]), None);
    }

    #[test]
    fn register_rfcomm_service_is_ok() {
        let id = PeerId(123);
        let mut manager = ServiceSet::new(id);

        let random_channel = ServerChannel::try_from(3).unwrap();
        assert_matches!(manager.rfcomm_channel_registered(random_channel), None);

        // Build an RFCOMM record and assign it a server channel.
        let example_channel = ServerChannel::try_from(17).unwrap();
        let (_, mut rfcomm_record) = build_rfcomm_service_definition(None);
        assert_matches!(rfcomm_record.set_rfcomm_channel(example_channel), Ok(_));

        let handle1 =
            manager.register_service(vec![rfcomm_record.clone()]).expect("should register");
        assert_matches!(manager.rfcomm_channel_registered(example_channel), Some(_));

        // Registering an RFCOMM service with the same channel number should fail.
        assert_matches!(manager.register_service(vec![rfcomm_record]), None);

        // Unregistering services should work - should remove the service.
        assert_matches!(manager.unregister_service(&handle1).len(), 1usize);
        assert_matches!(manager.rfcomm_channel_registered(example_channel), None);
    }
}
