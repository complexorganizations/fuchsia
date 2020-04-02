// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// This module tests the Cobalt metrics reporting.
use {
    cobalt_client::traits::AsEventCode,
    cobalt_sw_delivery_registry as metrics,
    fidl::endpoints::create_endpoints,
    fidl_fuchsia_cobalt::{CobaltEvent, CountEvent, EventPayload},
    fidl_fuchsia_pkg::UpdatePolicy,
    fuchsia_async as fasync,
    fuchsia_pkg_testing::{
        serve::{handler, UriPathHandler},
        Package, PackageBuilder, RepositoryBuilder,
    },
    fuchsia_zircon::Status,
    lib::{make_repo, make_repo_config, MountsBuilder, TestEnv, TestEnvBuilder, EMPTY_REPO_PATH},
    matches::assert_matches,
    serde_json::json,
    std::sync::Arc,
};

#[fasync::run_singlethreaded(test)]
async fn pkg_resolver_startup_duration() {
    let env = TestEnvBuilder::new().build();

    let events = env
        .mocks
        .logger_factory
        .wait_for_at_least_n_events_with_metric_id(
            1,
            metrics::PKG_RESOLVER_STARTUP_DURATION_METRIC_ID,
        )
        .await;
    assert_matches!(
        events[0],
        CobaltEvent {
            metric_id: metrics::PKG_RESOLVER_STARTUP_DURATION_METRIC_ID,
            ref event_codes,
            component: None,
            payload: EventPayload::ElapsedMicros(_)
        } if event_codes == &vec![0]
    );

    env.stop().await;
}

async fn assert_count_events(
    env: &TestEnv,
    metric_id: u32,
    expected_results: Vec<impl AsEventCode>,
) {
    assert_eq!(
        env.mocks
            .logger_factory
            .wait_for_at_least_n_events_with_metric_id(expected_results.len(), metric_id)
            .await,
        expected_results
            .into_iter()
            .map(|res| CobaltEvent {
                metric_id,
                event_codes: vec![res.as_event_code()],
                component: None,
                payload: EventPayload::EventCount(CountEvent {
                    period_duration_micros: 0,
                    count: 1
                })
            })
            .collect::<Vec<CobaltEvent>>()
    );
}

#[fasync::run_singlethreaded(test)]
async fn repository_manager_load_static_configs_success() {
    let env = TestEnvBuilder::new()
        .mounts(lib::MountsBuilder::new().static_repository(make_repo()).build())
        .build();

    assert_count_events(
        &env,
        metrics::REPOSITORY_MANAGER_LOAD_STATIC_CONFIGS_METRIC_ID,
        vec![metrics::RepositoryManagerLoadStaticConfigsMetricDimensionResult::Success],
    )
    .await;

    env.stop().await;
}

#[fasync::run_singlethreaded(test)]
async fn repository_manager_load_static_configs_io() {
    let env = TestEnvBuilder::new().build();

    assert_count_events(
        &env,
        metrics::REPOSITORY_MANAGER_LOAD_STATIC_CONFIGS_METRIC_ID,
        vec![metrics::RepositoryManagerLoadStaticConfigsMetricDimensionResult::Io],
    )
    .await;

    env.stop().await;
}

#[fasync::run_singlethreaded(test)]
async fn repository_manager_load_static_configs_parse() {
    let env = TestEnvBuilder::new()
        .mounts(
            MountsBuilder::new()
                .custom_config_data("repositories/invalid.json", "invalid-json")
                .build(),
        )
        .build();

    assert_count_events(
        &env,
        metrics::REPOSITORY_MANAGER_LOAD_STATIC_CONFIGS_METRIC_ID,
        vec![metrics::RepositoryManagerLoadStaticConfigsMetricDimensionResult::Parse],
    )
    .await;

    env.stop().await;
}

#[fasync::run_singlethreaded(test)]
async fn repository_manager_load_static_configs_overridden() {
    let json = serde_json::to_string(&make_repo_config(&make_repo())).unwrap();
    let env = TestEnvBuilder::new()
        .mounts(
            MountsBuilder::new()
                .custom_config_data("repositories/1.json", &json)
                .custom_config_data("repositories/2.json", json)
                .build(),
        )
        .build();

    assert_count_events(
        &env,
        metrics::REPOSITORY_MANAGER_LOAD_STATIC_CONFIGS_METRIC_ID,
        vec![metrics::RepositoryManagerLoadStaticConfigsMetricDimensionResult::Overridden],
    )
    .await;

    env.stop().await;
}

async fn verify_resolve_emits_cobalt_events_with_metric_id(
    pkg: Package,
    handler: Option<impl UriPathHandler>,
    expected_resolve_result: Result<(), Status>,
    metric_id: u32,
    expected_events: Vec<impl AsEventCode>,
) {
    let env = TestEnvBuilder::new().build();
    let repo = Arc::new(
        RepositoryBuilder::from_template_dir(EMPTY_REPO_PATH)
            .add_package(&pkg)
            .build()
            .await
            .unwrap(),
    );
    let mut served_repository = repo.server();
    if let Some(handler) = handler {
        served_repository = served_repository.uri_path_override_handler(handler);
    }
    let served_repository = served_repository.start().unwrap();
    let repo_url = "fuchsia-pkg://example.com".parse().unwrap();
    let config = served_repository.make_repo_config(repo_url);
    env.proxies.repo_manager.add(config.clone().into()).await.unwrap();

    assert_eq!(
        env.resolve_package(&format!("fuchsia-pkg://example.com/{}", pkg.name())).await.map(|_| ()),
        expected_resolve_result
    );
    assert_count_events(&env, metric_id, expected_events).await;
    env.stop().await;
}

// Fetching one blob successfully should emit one success fetch blob event.
#[fasync::run_singlethreaded(test)]
async fn pkg_resolver_fetch_blob_success() {
    verify_resolve_emits_cobalt_events_with_metric_id(
        PackageBuilder::new("just_meta_far").build().await.expect("created pkg"),
        Option::<handler::StaticResponseCode>::None,
        Ok(()),
        metrics::FETCH_BLOB_METRIC_ID,
        vec![metrics::FetchBlobMetricDimensionResult::Success],
    )
    .await;
}

// Fetching one blob with an HTTP error should emit 2 failure blob events (b/c of retries).
#[fasync::run_singlethreaded(test)]
async fn pkg_resolver_fetch_blob_failure() {
    let pkg = PackageBuilder::new("just_meta_far").build().await.expect("created pkg");
    let handler = handler::ForPath::new(
        format!("/blobs/{}", pkg.meta_far_merkle_root()),
        handler::StaticResponseCode::not_found(),
    );

    verify_resolve_emits_cobalt_events_with_metric_id(
        pkg,
        Some(handler),
        Err(Status::UNAVAILABLE),
        metrics::FETCH_BLOB_METRIC_ID,
        vec![
            metrics::FetchBlobMetricDimensionResult::BadHttpStatus,
            metrics::FetchBlobMetricDimensionResult::BadHttpStatus,
        ],
    )
    .await;
}

#[fasync::run_singlethreaded(test)]
async fn font_resolver_is_font_package_check_not_font() {
    let env = TestEnvBuilder::new().build();
    let repo =
        Arc::new(RepositoryBuilder::from_template_dir(EMPTY_REPO_PATH).build().await.unwrap());
    let served_repository = repo.server().start().unwrap();
    env.proxies
        .repo_manager
        .add(
            served_repository.make_repo_config("fuchsia-pkg://example.com".parse().unwrap()).into(),
        )
        .await
        .unwrap();

    // No font packages have been registered with the font resolver, so resolves of any packages
    // (existing or not) will fail with NOT_FOUND and emit an event with the NotFont dimension.
    let (_, server) = create_endpoints().unwrap();
    assert_eq!(
        Status::from_raw(
            env.proxies
                .font_resolver
                .resolve(
                    "fuchsia-pkg://example.com/some-nonexistent-pkg",
                    &mut UpdatePolicy { fetch_if_absent: true, allow_old_versions: false },
                    server,
                )
                .await
                .unwrap()
        ),
        Status::NOT_FOUND
    );

    assert_count_events(
        &env,
        metrics::IS_FONT_PACKAGE_CHECK_METRIC_ID,
        vec![metrics::IsFontPackageCheckMetricDimensionResult::NotFont],
    )
    .await;
    env.stop().await;
}

#[fasync::run_singlethreaded(test)]
async fn font_manager_load_static_registry_success() {
    let json = serde_json::to_string(&json!(["fuchsia-pkg://fuchsia.com/font1"])).unwrap();
    let env = TestEnvBuilder::new()
        .mounts(MountsBuilder::new().custom_config_data("font_packages.json", json).build())
        .build();

    assert_count_events(
        &env,
        metrics::FONT_MANAGER_LOAD_STATIC_REGISTRY_METRIC_ID,
        vec![metrics::FontManagerLoadStaticRegistryMetricDimensionResult::Success],
    )
    .await;

    env.stop().await;
}

#[fasync::run_singlethreaded(test)]
async fn font_manager_load_static_registry_failure_io() {
    let env = TestEnvBuilder::new().build();

    assert_count_events(
        &env,
        metrics::FONT_MANAGER_LOAD_STATIC_REGISTRY_METRIC_ID,
        vec![metrics::FontManagerLoadStaticRegistryMetricDimensionResult::Io],
    )
    .await;

    env.stop().await;
}

#[fasync::run_singlethreaded(test)]
async fn font_manager_load_static_registry_failure_parse() {
    let env = TestEnvBuilder::new()
        .mounts(
            MountsBuilder::new().custom_config_data("font_packages.json", "invalid-json").build(),
        )
        .build();

    assert_count_events(
        &env,
        metrics::FONT_MANAGER_LOAD_STATIC_REGISTRY_METRIC_ID,
        vec![metrics::FontManagerLoadStaticRegistryMetricDimensionResult::Parse],
    )
    .await;

    env.stop().await;
}

// We should get a cobalt event for each pkg-url error.
#[fasync::run_singlethreaded(test)]
async fn font_manager_load_static_registry_failure_pkg_url() {
    let json = serde_json::to_string(&json!([
        "fuchsia-pkg://missing-pkg-name.com/",
        "fuchsia-pkg://includes-resource.com/foo#meta/resource.cmx"
    ]))
    .unwrap();
    let env = TestEnvBuilder::new()
        .mounts(MountsBuilder::new().custom_config_data("font_packages.json", json).build())
        .build();

    assert_count_events(
        &env,
        metrics::FONT_MANAGER_LOAD_STATIC_REGISTRY_METRIC_ID,
        vec![
            metrics::FontManagerLoadStaticRegistryMetricDimensionResult::PkgUrl,
            metrics::FontManagerLoadStaticRegistryMetricDimensionResult::PkgUrl,
        ],
    )
    .await;

    env.stop().await;
}
