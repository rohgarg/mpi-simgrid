# Verifying MPI Checkpoint-Restart With SimGrid

## Table of Contents

* [Idea](#idea)
* [High-level Design](#high-level-design)
  * [Start up](#start-up)
  * [Runtime logic](#runtime-logic)
  * [Checkpoint algorithm](#checkpoint-algorithm)
  * [Restart algorithm](#restart-algorithm)
* [TODO](#todo)
* [Results](#results)
* [Appendix](#appendix)
  * [Running the code](#running-the-code)
  * [Known issues](#known-issues)

## Idea

The idea here is to verify the feasibility of checkpoint-restart of MPI
applications using the new "kernel-loader" approach with the simplest
possible experiment, without involving the complexities of DMTCP.

**The experiment:**

## High-level Design

### Start up


### Runtime logic

### Checkpoint algorithm

### Restart algorithm

## TODO

* [x] Add wrappers/trampolines around mmap/sbrk functions for upper-half's
       libc. In addition to lower-half's ld.so, lower-half's libc can also
       make these calls. We want to keep track of mmaps and "virtualize"
       the sbrk calls in order to force it to use the lower-half's heap.
* [x] Add a dlsym-like API in the lower half to figure out addresses of MPI API
* [x] Test calling a MPI function (through lower-half's dlsym API) from the
       upper half
* [x] Add checkpoint-restart logic from mini-DMTCP assignment
* [x] Test full checkpoint-restart functionality
* [x] Add code to restart one rank as thread
* [x] Add code for dummy SimGrid library to be used post restart
* [ ] Add code to map upper-half segments based on MPI rank at launch time
* [x] Add code to restart from checkpoint images of multiple ranks
* [ ] Test restart from checkpoint images of multiple ranks
* [ ] Create a fake VDSO page for the upper half (See [here](#known-issues)
      for more details).

## Results

The basic idea works. See the list of open items in the [TODO](#todo) section.

## Appendix

### Running the code

1. Update the variables at the top of the Makefile to correspond to your system.
2. Run `make restart` to build and run the code.

### Known issues

1. The application might crash after restart if it tries to call
   `gettimeofday()`. Since the upper half is dynamically-linked, it
   needs the VDSO page for making calls such as, `gettimeofday()`, etc.
   And the location of the VDSO page might change post restart.
