# Dapper Dependency Resolver for Package Managers

Dapper is a tool for dependency resolution designed to be used with package managers working on CMake. Currently we support only [CPM](https://github.com/cpm-cmake/CPM.cmake).

This implementation is still experimental and not ready for production use.

## Prerequisites

- git
- CMake 3.18 or later

## What does it do?

We call package with dependency metadata written in our format DAP : Dependency-Aware Package. Each DAP has DependencyAwareness.yml file under the package root directory. The file would look like below:

```yaml
name: my-package
version: "0.1.0"
dependencies:
  dep1:
    require: ">= 1.0.0"
    location: "example:libs/dep1"
  dep2:
    require: ">= 0.3.0"
    location: "example:libs/dep2"
```

In this example, the name of the package is "my-package" and version is 0.1.0. It depends on two packages: dep1 1.0.0 or later stored at path "libs/dep1" on host "example", and dep2 0.3.0 or later stored at path "libs/dep2" on host "example".

When Dapper resolves the dependencies from this package, it clones all repositories declared as locations of dependencies and then looks up all exposed revisions by `git tag` command.
In current implementation, Dapper visits DependencyAwareness.yml file in every exposed revision found from the cloned repositories and builds the dependency graph.
If nested dependencies are discovered, Dapper evaluates them too.

Since there may be version requirement at each dependency, Dapper also ensures that the requirements are satisfied, or fails to process if there are no satisfiable combination.
This resolution is done by iterations of call to "run" mode of the "dappi" command we've bundled in this project.
After iterations are done, the information of the selected packages are passed to the package manager.

dappi is a helper program which takes informations of packages in either YAML("load" mode) or JSON("run" and "save" mode) format and may emit CMake commands.
In "run" mode, it invokes a basic SAT solver multiple times, in order to keep selecting the locked packages and prefer higher versions as much as possible.

After the resolution is done, Dapper records versions, locations, and integrities of the selected packages into DependencyAwarenessLock.yml file under the source directory on which `DAPPER_INTEGRATE_WITH` is initially called during the configuration phase of CMake.

## Defining hosts

Whereas Dapper also supports physical URLs as locations of dependencies, defining hosts is the recommended way because URLs may change, or may be not accessible from some environments. This can be done by writing a configuration file and instructing Dapper to use the file by specifying the path on `DAPPER_CONFIG_FILE` variable or environment variable.

The configuration file is a CMake script and would look like below:

```cmake
function (HANDLE_EXAMPLE -out)
  cmake_parse_arguments (PARSE_ARGV 1 -arg "" "HOST;USER;PATH" "")
  set ("${-out}" "https://example.com/${-arg_PATH}.git" PARENT_SCOPE)
endfunction ()

DAPPER_DEFINE_PRESET_HOSTS()
DAPPER_REGISTER_HOST(example HANDLE_EXAMPLE)
```

Right now `DAPPER_DEFINE_PRESET_HOSTS` defines the preset host: "github". `DAPPER_REGISTER_HOST` is the function to bind the name of the host with the handler function.

## How to try demo

```
$ git clone https://github.com/flokart-world/dapper.git
$ cmake -B dapper-demo -S dapper/Examples/demo
$ cd dapper-demo
$ cmake --build . --config Debug

# Re-invoking the dependency resolution
$ cmake -D DAPPER_INSTALL=ON .
```
