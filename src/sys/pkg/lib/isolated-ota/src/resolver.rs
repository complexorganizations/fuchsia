// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    crate::pkgfs::Pkgfs,
    anyhow::{Context, Error},
    fidl_fuchsia_boot::{ArgumentsRequest, ArgumentsRequestStream},
    fidl_fuchsia_pkg::PackageCacheMarker,
    fuchsia_async as fasync,
    fuchsia_component::{
        client::{App, AppBuilder},
        server::{NestedEnvironment, ServiceFs, ServiceObj},
    },
    fuchsia_syslog::fx_log_err,
    fuchsia_zircon::{self as zx, HandleBased},
    futures::prelude::*,
    std::sync::Arc,
};

const CACHE_URL: &str = "fuchsia-pkg://fuchsia.com/isolated-swd#meta/pkg-cache-isolated.cmx";
const RESOLVER_URL: &str = "fuchsia-pkg://fuchsia.com/isolated-swd#meta/pkg-resolver-isolated.cmx";
const SSL_CERTS_PATH: &str = "/config/ssl";

struct IsolatedBootArgs {
    channel: Option<String>,
}

impl IsolatedBootArgs {
    pub fn new(channel: Option<String>) -> Self {
        IsolatedBootArgs { channel }
    }

    pub async fn serve(self: Arc<Self>, mut stream: ArgumentsRequestStream) {
        while let Some(req) = stream.try_next().await.unwrap() {
            match req {
                ArgumentsRequest::GetString { key, responder } => {
                    if key == "tuf_repo_config" {
                        responder.send(self.channel.as_ref().map(|c| c.as_str())).unwrap();
                    } else {
                        fx_log_err!("Unexpected arguments GetString: {}, closing channel.", key);
                    }
                }
                _ => fx_log_err!("Unexpected arguments request, closing channel."),
            }
        }
    }
}

pub struct Resolver {
    _pkg_cache: App,
    _pkg_resolver: App,
    pkg_resolver_directory: Arc<zx::Channel>,
    _env: NestedEnvironment,
}

impl Resolver {
    pub fn launch(pkgfs: &Pkgfs, repo_config: std::fs::File, channel: &str) -> Result<Self, Error> {
        Resolver::launch_with_components(
            pkgfs,
            repo_config,
            Some(channel.to_owned()),
            std::fs::File::open(SSL_CERTS_PATH).context("opening ssl directory")?,
            CACHE_URL,
            RESOLVER_URL,
        )
    }

    fn launch_with_components(
        pkgfs: &Pkgfs,
        repo_config: std::fs::File,
        channel: Option<String>,
        ssl_dir: std::fs::File,
        cache_url: &str,
        resolver_url: &str,
    ) -> Result<Self, Error> {
        let mut pkg_cache = AppBuilder::new(cache_url)
            .add_handle_to_namespace("/pkgfs".to_owned(), pkgfs.root_handle()?.into_handle());

        let mut pkg_resolver = AppBuilder::new(resolver_url)
            .add_handle_to_namespace("/pkgfs".to_owned(), pkgfs.root_handle()?.into_handle())
            .add_dir_to_namespace("/config/data/repositories".to_owned(), repo_config)?
            .add_dir_to_namespace(SSL_CERTS_PATH.to_owned(), ssl_dir)?;

        let boot_args = Arc::new(IsolatedBootArgs::new(channel));

        let mut fs: ServiceFs<ServiceObj<'_, ()>> = ServiceFs::new();
        fs.add_proxy_service::<fidl_fuchsia_net::NameLookupMarker, _>()
            .add_proxy_service::<fidl_fuchsia_posix_socket::ProviderMarker, _>()
            .add_proxy_service::<fidl_fuchsia_logger::LogSinkMarker, _>()
            .add_proxy_service::<fidl_fuchsia_tracing_provider::RegistryMarker, _>();
        fs.add_proxy_service_to::<PackageCacheMarker, _>(
            pkg_cache.directory_request().unwrap().clone(),
        );
        fs.add_fidl_service(move |stream: ArgumentsRequestStream| {
            fasync::spawn(Arc::clone(&boot_args).serve(stream))
        });

        // We use a salt so the unit tests work as expected.
        let env = fs.create_salted_nested_environment("isolated-ota-env")?;
        fasync::spawn(fs.collect());

        let directory =
            pkg_resolver.directory_request().context("getting directory request")?.clone();
        let pkg_cache = pkg_cache.spawn(env.launcher()).context("launching package cache")?;
        let pkg_resolver =
            pkg_resolver.spawn(env.launcher()).context("launching package resolver")?;

        Ok(Resolver {
            _pkg_cache: pkg_cache,
            _pkg_resolver: pkg_resolver,
            pkg_resolver_directory: directory,
            _env: env,
        })
    }

    pub fn directory_request(&self) -> Arc<fuchsia_zircon::Channel> {
        self.pkg_resolver_directory.clone()
    }
}

#[cfg(test)]
pub mod tests {
    use {
        super::*,
        crate::pkgfs::tests::PkgfsForTest,
        fidl_fuchsia_io::DirectoryProxy,
        fidl_fuchsia_pkg::{PackageResolverMarker, UpdatePolicy},
        fidl_fuchsia_pkg_ext::RepositoryConfigs,
        fuchsia_pkg_testing::{
            serve::ServedRepository, PackageBuilder, Repository, RepositoryBuilder,
        },
    };

    pub struct ResolverForTest {
        pub pkgfs: PkgfsForTest,
        pub resolver: Resolver,
        _served_repo: ServedRepository,
    }

    const TEST_REPO_URL: &str = "fuchsia-pkg://test";
    const SSL_TEST_CERTS_PATH: &str = "/pkg/data/ssl";
    pub const EMPTY_REPO_PATH: &str = "/pkg/empty-repo";

    impl ResolverForTest {
        pub async fn new(
            repo: Arc<Repository>,
            repo_path: &'static str,
            channel: Option<String>,
        ) -> Result<Self, Error> {
            let pkgfs = PkgfsForTest::new().context("Launching pkgfs")?;

            // Set up the repository config for pkg-resolver.
            let served_repo = Arc::clone(&repo).server().start()?;

            let repo_url = repo_path.parse()?;
            let repo_config =
                RepositoryConfigs::Version1(vec![served_repo.make_repo_config(repo_url)]);
            let tempdir = tempfile::tempdir()?;
            let mut temp_path = tempdir.path().to_owned();
            temp_path.push("test.json");
            let path = temp_path.as_path();
            serde_json::to_writer(
                std::io::BufWriter::new(std::fs::File::create(path).context("creating file")?),
                &repo_config,
            )
            .unwrap();

            // Launch the resolver.
            let repo_dir =
                std::fs::File::open(tempdir.into_path()).context("opening repo tmpdir")?;
            let ssl_certs =
                std::fs::File::open(SSL_TEST_CERTS_PATH).context("opening ssl certificates dir")?;
            let resolver = Resolver::launch_with_components(
                &pkgfs.pkgfs,
                repo_dir,
                channel,
                ssl_certs,
                "fuchsia-pkg://fuchsia.com/isolated-ota-tests#meta/pkg-cache.cmx",
                "fuchsia-pkg://fuchsia.com/isolated-ota-tests#meta/pkg-resolver.cmx",
            )
            .context("launching resolver")?;

            Ok(ResolverForTest { pkgfs, resolver, _served_repo: served_repo })
        }

        pub async fn resolve_package(&self, url: &str) -> Result<DirectoryProxy, Error> {
            let resolver = self
                .resolver
                ._pkg_resolver
                .connect_to_service::<PackageResolverMarker>()
                .context("getting resolver")?;
            let selectors: Vec<&str> = vec![];
            let (package, package_remote) =
                fidl::endpoints::create_proxy().context("creating package directory endpoints")?;
            let status = resolver
                .resolve(
                    url,
                    &mut selectors.into_iter(),
                    &mut UpdatePolicy { fetch_if_absent: true, allow_old_versions: false },
                    package_remote,
                )
                .await
                .unwrap();
            zx::Status::ok(status).context("resolving package")?;
            Ok(package)
        }
    }

    #[fasync::run_singlethreaded(test)]
    pub async fn test_resolver() -> Result<(), Error> {
        let name = "test-resolver";
        let package = PackageBuilder::new(name)
            .add_resource_at("data/file1", "hello".as_bytes())
            .add_resource_at("data/file2", "hello two".as_bytes())
            .build()
            .await
            .unwrap();
        let repo = Arc::new(
            RepositoryBuilder::from_template_dir(EMPTY_REPO_PATH)
                .add_package(&package)
                .build()
                .await
                .context("Building repo")
                .unwrap(),
        );

        let resolver =
            ResolverForTest::new(repo, TEST_REPO_URL, None).await.context("launching resolver")?;
        let root_dir =
            resolver.resolve_package(&format!("{}/{}", TEST_REPO_URL, name)).await.unwrap();

        package.verify_contents(&root_dir).await.unwrap();
        Ok(())
    }

    #[fasync::run_singlethreaded(test)]
    pub async fn test_resolver_with_channel() -> Result<(), Error> {
        let name = "test-resolver-channel";
        let package = PackageBuilder::new(name)
            .add_resource_at("data/file1", "hello".as_bytes())
            .add_resource_at("data/file2", "hello two".as_bytes())
            .build()
            .await
            .unwrap();
        let repo = Arc::new(
            RepositoryBuilder::from_template_dir(EMPTY_REPO_PATH)
                .add_package(&package)
                .build()
                .await
                .context("Building repo")
                .unwrap(),
        );

        let resolver = ResolverForTest::new(
            repo,
            "fuchsia-pkg://x64.resolver-test-channel.fuchsia.com",
            Some("resolver-test-channel".to_owned()),
        )
        .await
        .context("launching resolver")?;
        let root_dir =
            resolver.resolve_package(&format!("fuchsia-pkg://fuchsia.com/{}", name)).await.unwrap();

        package.verify_contents(&root_dir).await.unwrap();
        Ok(())
    }
}
