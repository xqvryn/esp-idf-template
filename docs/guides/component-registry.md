# Publish components to the registry

Reusable components under `components/` have an `idf_component.yml` ready for the [ESP Component Registry](https://components.espressif.com/). Manifests include a version, description, homepage, documentation, issues URL, Git repository, the path inside this monorepo, the MIT license, and a `files` filter that omits `test/` trees. They are not uploaded until a maintainer sets `IDF_COMPONENT_API_TOKEN` and a stable tag runs the publish job.

This page is about packaging and publishing. Downstream usage, including registry and git examples, is in [Use the components](components.md).

## What CI checks

Pull requests and pushes pack every reusable component with `compote component pack`. That validates the manifest and builds an archive; it does not create a registry version. If `IDF_COMPONENT_API_TOKEN` is present, CI also runs `compote component upload --dry-run` for an extra registry-side check, still without publishing.

On a stable tag such as `v3.3.0`, a separate workflow publishes those archives to the `xqvryn` namespace, but only when `IDF_COMPONENT_API_TOKEN` is set. If the secret is missing, the job succeeds and prints that it skipped the upload. Prerelease tags such as `v3.3.0-dev.1` are skipped. The token is never created or requested by automation.

Pack everything locally after installing the Component Manager:

```sh
python -m pip install idf-component-manager
python3 tools/component_registry.py pack
```

## After a token is configured

Store `IDF_COMPONENT_API_TOKEN` as a repository secret (Actions access is enough). Do not commit it. The next stable tag then uploads `platform`, `configuration`, `networking`, `drivers`, `timed_output`, `runtime`, `parameters`, `channels`, `runtime_task`, `motion`, `motion_port`, and `flight` as `xqvryn/<name>`. Sibling manifests still use local `path` dependencies so this repository builds without a registry copy.

The sensor-node code under `applications/node` is an example application and is not published.
