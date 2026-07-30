# Testing Protocol (m.1)

This file contains an outline for the way that tests will operate within this
repository. The contents of this document are not future-proof yet.

## Guidelines

There should be three kinds of tests:

 - Tests of functionality & compatibility
 - Tests against regression

Each of these kinds of tests must be run against every version of the codebase
that is intended to be stable, especially against alphas-betas, and pull
requests.

 ### Tests of Functionality & Compatibility

 Tests must exist for the purpose of confirming that code is able to compile and
 be minimally used, even if not used well, by various different sorts of calling
 code, including but not limited to other C code, Python, and ideally programs
 like D.

### Tests Against Regression

When a fault is found within previously presumed functional code, a test should
be made to exacerbate the problem in order to test against future versions of
the code in order to prevent the code from regressing back into its previous
faulty state.

## Running Tests

Each test has a formula corrosponding to its name, and each test can be compiled
(if it is a test that needs to be compiled) with `make all`. All tests can be
run via `make runall`.

## Writing Tests

Make the name of the test verbose. If it must be compiled, write a formula
corrosponding to the name of the source file in the `Makefile`. Make the
executable be outputted in `tests/bin`; that way it will be detected by the
runall formula. If the test can't be compiled, write a formula in the
`Makefile` that simply copies the source file to the `test/bin` directory. Make
tests such as these (Python, Bash, etc.) executable, and have shebang lines
(i.e. `#!/usr/bin/python3`) at the beginning of the file. Make sure that all
tests are heavily commented, at least at the beginning to explain how the test
works. Make every test assume that liblimeade.so can be found at the top of the
repository filetree, and assume its being run from `test/bin`.

### Return Values

If a test succeeds, it should return 0. If a test fails, it should return 1 or
more. If a test cannot ascertain whether or not it has succeeded, it should
return -1. While it is understandable and inevitable that some tests cannot
guarantee that they succeeded, it should generally be avoided, as it then
requires human assistance every time the test is run to see if the test failed.
On that note, every test must display information that can allow a human to
ascertain its success, even if it cannot ascertain its own success.

## Licensing

All tests are assumed to be Unlicensed. If you contribute a test and don't want
to adopt the Unlicense, explicitly say so, or else your contribution will be
deemed Unlicensed.
