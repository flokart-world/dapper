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
    location: "https://example.com/dep1.git"
  dep2:
    require: ">= 0.3.0"
    location: "https://example.com/dep2.git"
```

In this example, the name of the package is "my-package" and version is 0.1.0. It depends on two packages: dep1 1.0.0 or later, and dep2 0.3.0 or later.

When Dapper resolves the dependencies from this package, it clones all repositories declared as locations of dependencies and then looks up all exposed revisions by `git tag` command.
In current implementation, Dapper visits DependencyAwareness.yml file in every exposed revision found from the cloned repositories and builds the dependency graph.
If nested dependencies are discovered, Dapper evaluates them too.

Since there may be version requirement at each dependency, Dapper also ensures that the requirements are satisfied, or fails to process if there are no satisfiable combination.
This resolution is done by iterations of call to "run" mode of the "dappi" command we've bundled in this project.
After iterations are done, the information of the selected packages are passed to the package manager.

dappi is a helper program which takes informations of packages in either YAML("load" mode) or JSON("run" mode) format and emits CMake commands.
In "run" mode, it invokes a basic SAT solver multiple times, in order to prefer higher versions as much as possible.

## How to try demo

```
$ git clone https://github.com/flokart-world/dapper.git
$ cmake -B dapper-demo -S dapper/Examples/demo
$ cd dapper-demo
$ cmake --build . --config Debug -t dapper-install
```
