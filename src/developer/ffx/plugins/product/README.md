# Product Bundle tool (plugin)

The Product Bundle Metadata (PBM) holds configuration information for an
individual product. PBM files are also included in Product Bundle container
files. The Product Bundle container holds data for product bundles, virtual
device specification, etc.

## Development of the product plugin

When working on the `fx ffx product` tool, consider using:

```
$ fx set [...] --with-host //src/developer/ffx
```

### Unit Tests

Unit tests can be run with:

```
$ fx test ffx
```
