# Dapper Dependency Resolver for Package Managers

Dapper is a tool for dependency resolution designed to be used with package managers working on CMake. Currently we support only [CPM](https://github.com/cpm-cmake/CPM.cmake).

This implementation is still experimental and not ready for production use.

## Prerequisites

- git
- CMake 3.18 or later

## What does it do?

We call package with dependency metadata written in our format DAP : Dependency-Aware Package. Each DAP has DependencyAwareness.cmake file under the package root directory. The file would look like below:

```cmake
DAP_INFO(
  NAME my-package
  VERSION 0.1.0
)

DAP(
    NAME dep1
    REQUIRE ">= 1.0.0"
    LOCATION "https://github.com/example/dep1.git#v1.2.0"
)
DAP(
    NAME dep2
    REQUIRE ">= 0.3.0"
    LOCATION "https://github.com/example/dep2.git#v0.3.5"
)
```

`DAP_INFO` declares the information of the package and `DAP` declares a dependency.
In this example, the name of the package is "my-package" and version is 0.1.0. It depends on two packages: dep1 1.0.0 or later, and dep2 0.3.0 or later.

When Dapper resolves the dependencies from this package, it clones all repositories declared as locations of dependencies and then looks up all exposed revisions by `git tag` command.
In current implementation, Dapper visits all DependencyAwareness.cmake files in all exposed revisions found from the cloned repositories and builds the dependency graph.
If nested dependencies are discovered, Dapper evaluates them too.

Since there may be version requirement at each dependency, Dapper also ensures that the requirements are satisfied, or fails to process if there are no satisfiable combination.
This resolution is done by iterations of call to the "dappi" command we've bundled in this project.
After iterations are done, the information of the selected packages are passed to the package manager.

dappi is a simple program which takes informations of all available packages in JSON format from standard input and emits CMake commands to standard output.
It currently invokes just a basic SAT solver, so there's no functionality to prior higher package versions yet.
Further improvements are needed.

## How to try demo

```
$ git clone https://github.com/flokart-world/dapper.git
$ cmake -B dapper-demo -S dapper/Examples/demo
$ cd dapper-demo
$ cmake .
```
