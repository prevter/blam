# wtf ⁉

Blazing fast LOC counter tool

---

## About

wtf is yet another CLOC (Count Lines Of Code) tool that attempts to squeeze as much performance as possible. This
project is mostly an experiment to see how far I could go trying to optimize the algorithm.

## Contents

- [Features](#features)
- [Usage](#usage)
- [Installation](#installation)
- [Building](#building)
- [Benchmarks](#benchmarks)
- [Deep dive into performance journey](#deep-dive-into-performance-journey)
    - [Custom CLI Parser](#1-custom-compile-time-optimized-cli-parser)
    - [Directory Walking](#2-directory-walking-using-syscalls)
    - [OS Scheduler](#3-os-scheduler-is-our-biggest-enemy-and-friend)
    - [Heap is Sacred, Stack is Our Friend](#4-heap-is-sacred-stack-is-our-friend)
    - [Reimplementing fnmatch](#5-reimplementing-fnmatch)
    - [Language Definitions Codegen](#6-language-definitions-codegen)
    - [Cache Lines and Why They're Important](#7-cache-lines-and-why-theyre-important)
    - [mmap or not mmap](#8-mmap-or-not-mmap)
    - [SIMD Optimizations](#9-simd-optimizations)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Features

- Supports 138 languages (all specifications were taken from [tokei](https://github.com/xampprocky/tokei) repository)
- Handles .gitignore files with optional flag to disable checks
- Plain text and CSV output modes (JSON planned next)
- Cross-platform (supports Linux, macOS and Windows)

## Usage

```
~> wtf -h
wtf - Wasted Time Finder

Usage: wtf [OPTIONS] [...]

Arguments:
  [...]: Files/directories to scan

Options:
  -t, --threads <value>: Number of threads to use (default: number of CPU cores)
  -i, --hidden: Include hidden files and directories
  -g, --no-gitignore: Disable .gitignore support
  -s, --sort <value>: Sorting mode for output (files, lines, code, comments, blanks)
  -n, --top <value>: Show only the top N languages (default: show all)
  -p, --per-file: Collect stats for each file (increases memory usage)
  -o, --output <value>: Output file (default: stdout)
  -f, --format <value>: Output format (text, json, csv)
  -h, --help: Show this help message
  -v, --version: Show version information
```

Example output:

```
~> wtf src
wtf v2.1.0 | analysis: 681.71µs | 30.80K files/s | 4.97M lines/s | 152.00 MiB/s
───────────────────────────────────────────────────────────────────────────                                                                                                                                                           
 Language        Files        Lines         Code     Comments       Blanks                                                                                                                                                            
───────────────────────────────────────────────────────────────────────────                                                                                                                                                           
 C++ Header         13         1579         1301            4          274                                                                                                                                                            
 C++                 7         1556         1294           17          245                                                                                                                                                            
 Python              1          254          177            6           71                                                                                                                                                            
───────────────────────────────────────────────────────────────────────────                                                                                                                                                           
 Total              21         3389         2772           27          590                                                                                                                                                            
───────────────────────────────────────────────────────────────────────────                                                                                                                                                           
Total size: 108,652 bytes (106.11 KiB)
```

## Installation

Builds are available on the [releases](https://github.com/prevter/wtf/releases) page for Linux, macOS and Windows. For
building from source refer to the next section.

## Building

To compile **wtf** you will need:

- Git (for CPM)
- CMake 3.21+
- Python 3.11+
- Clang 19+

Only clang has been confirmed to work, but GCC should work as well.  
Windows also builds using clang with MSVC toolchain.

```shell
git clone https://github.com/prevter/wtf && cd wtf

cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build
```

## Benchmarks

Benchmarks were performed using [hyperfine](https://github.com/sharkdp/hyperfine) comparing `wtf` against popular tools
like `scc`, `tokei`, and `polyglot`.

### Test Environment

All benchmarks were run on my laptop in "Performance" power profile.

Exact hardware used:

- **CPU:** AMD Ryzen 7 7730U (8 Cores / 16 Threads)
- **RAM:** 16 GB DDR4 (2x8 GB 3200 MHz)
- **Storage:** 1 TB M.2 NVMe SSD (btrfs filesystem + zstd:3)
- **OS:** Arch Linux (kernel 7.1.5)

Repositories used:

- [Valkey](https://github.com/valkey-io/valkey) - 530,000 lines of code
- [Sourcegraph](https://github.com/SINTEF/sourcegraph) - 1,300,000 lines of code
- [CPython](https://github.com/python/cpython) - 3,000,000 lines of code
- [Linux](https://github.com/torvalds/linux) - 42,600,000 lines of code

Versions of tools used:

- [scc](https://github.com/boyter/scc): 3.7.0
- [tokei](https://github.com/xampprocky/tokei): 14.0.0
- [polyglot](https://github.com/vmchale/polyglot): 0.5.29

### Warm Cache (Mean Time, ms)

Tested using `hyperfine --warmup 25` to warm the cache for each tool.

| Tool     |   Valkey | Sourcegraph |  CPython |     Linux |
|:---------|---------:|------------:|---------:|----------:|
| **wtf**  |     33.7 |    **43.6** | **50.6** | **181.2** |
| scc      |     31.6 |       114.4 |    133.4 |      2429 |
| scc -c   |     27.5 |       103.4 |    118.3 |      1734 |
| tokei    |     31.6 |       147.1 |    121.9 |      1345 |
| polyglot | **18.5** |        67.0 |     60.6 |      1088 |

### Cold Cache (Mean Time, ms)

Tested using `hyperfine --prepare "sync; echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null"` to drop caches before
each run.

| Tool     |   Valkey | Sourcegraph |   CPython |     Linux |
|:---------|---------:|------------:|----------:|----------:|
| **wtf**  | **55.1** |    **90.3** | **104.1** | **711.8** |
| scc      |     55.7 |       182.3 |     193.2 |      2999 |
| scc -c   |     56.5 |       188.4 |     174.0 |      2586 |
| tokei    |     59.9 |       216.7 |     180.3 |      2410 |
| polyglot |     62.4 |       200.7 |     156.0 |      2391 |

### Detailed Results

<details>
<summary>Click to expand all detailed results</summary>

#### Valkey

```
--> Running WARM cache benchmark...
Benchmark 1: wtf valkey
  Time (mean ± σ):      33.7 ms ±   7.8 ms    [User: 21.5 ms, System: 28.7 ms]
  Range (min … max):    17.2 ms …  54.7 ms    93 runs
 
Benchmark 2: scc valkey
  Time (mean ± σ):      31.6 ms ±   2.3 ms    [User: 212.9 ms, System: 89.8 ms]
  Range (min … max):    28.0 ms …  39.2 ms    91 runs
 
Benchmark 3: scc -c valkey
  Time (mean ± σ):      27.5 ms ±   2.4 ms    [User: 153.3 ms, System: 94.2 ms]
  Range (min … max):    23.7 ms …  36.1 ms    105 runs
 
Benchmark 4: tokei valkey
  Time (mean ± σ):      31.6 ms ±   0.9 ms    [User: 284.1 ms, System: 29.8 ms]
  Range (min … max):    29.4 ms …  34.4 ms    93 runs
 
Benchmark 5: polyglot valkey
  Time (mean ± σ):      18.5 ms ±   1.2 ms    [User: 29.0 ms, System: 32.2 ms]
  Range (min … max):    16.8 ms …  23.9 ms    161 runs
 
Summary
  polyglot valkey ran
    1.49 ± 0.17 times faster than scc -c valkey
    1.71 ± 0.17 times faster than scc valkey
    1.71 ± 0.12 times faster than tokei valkey
    1.82 ± 0.44 times faster than wtf valkey

--> Running COLD cache benchmark...
Benchmark 1: wtf valkey
  Time (mean ± σ):      55.1 ms ±   5.6 ms    [User: 27.3 ms, System: 81.6 ms]
  Range (min … max):    44.7 ms …  63.9 ms    10 runs
 
Benchmark 2: scc valkey
 
  Warning: Statistical outliers were detected. Consider re-running this benchmark on a quiet system without any interferences from other programs.  Time (mean ± σ):      62.7 ms ±  14.8 ms    [User: 199.6 ms, System: 147.3 ms]
  Range (min … max):    55.7 ms … 115.8 ms    15 runs
 It might help to use the '--warmup' or '--prepare' options.
 
Benchmark 3: scc -c valkey
  Time (mean ± σ):      56.5 ms ±   3.2 ms    [User: 144.6 ms, System: 147.3 ms]
  Range (min … max):    53.6 ms …  66.1 ms    17 runs
 
Benchmark 4: tokei valkey
  Time (mean ± σ):      59.9 ms ±   3.0 ms    [User: 255.0 ms, System: 83.6 ms]
  Range (min … max):    57.3 ms …  69.3 ms    17 runs
 
Benchmark 5: polyglot valkey
  Time (mean ± σ):      62.4 ms ±   1.6 ms    [User: 29.6 ms, System: 68.6 ms]
  Range (min … max):    60.4 ms …  66.2 ms    16 runs
 
Summary
  wtf valkey ran
    1.02 ± 0.12 times faster than scc -c valkey
    1.09 ± 0.12 times faster than tokei valkey
    1.13 ± 0.12 times faster than polyglot valkey
    1.14 ± 0.29 times faster than scc valkey
```

#### Sourcegraph

```
--> Running WARM cache benchmark...
Benchmark 1: wtf sourcegraph
  Time (mean ± σ):      43.6 ms ±   6.1 ms    [User: 64.6 ms, System: 80.4 ms]
  Range (min … max):    28.7 ms …  59.0 ms    69 runs
 
Benchmark 2: scc sourcegraph
  Time (mean ± σ):     114.4 ms ±   8.7 ms    [User: 682.0 ms, System: 407.2 ms]
  Range (min … max):   103.3 ms … 133.0 ms    25 runs
 
Benchmark 3: scc -c sourcegraph
  Time (mean ± σ):     103.4 ms ±  10.0 ms    [User: 603.4 ms, System: 399.0 ms]
  Range (min … max):    94.4 ms … 141.1 ms    21 runs
 
Benchmark 4: tokei sourcegraph
  Time (mean ± σ):     147.1 ms ±   4.9 ms    [User: 1622.1 ms, System: 147.1 ms]
  Range (min … max):   141.6 ms … 163.8 ms    20 runs
 
Benchmark 5: polyglot sourcegraph
 
  Warning: Statistical outliers were detected. Consider re-running this benchmark on a quiet system without any interferences from other programs.  Time (mean ± σ):      69.6 ms ±   4.2 ms    [User: 109.2 ms, System: 126.2 ms]
  Range (min … max):    67.0 ms …  88.2 ms    42 runs
 It might help to use the '--warmup' or '--prepare' options.
 
Summary
  wtf sourcegraph ran
    1.60 ± 0.24 times faster than polyglot sourcegraph
    2.37 ± 0.40 times faster than scc -c sourcegraph
    2.62 ± 0.42 times faster than scc sourcegraph
    3.37 ± 0.48 times faster than tokei sourcegraph

--> Running COLD cache benchmark...
Benchmark 1: wtf sourcegraph
  Time (mean ± σ):      90.3 ms ±   7.6 ms    [User: 79.9 ms, System: 330.0 ms]
  Range (min … max):    83.3 ms … 104.0 ms    10 runs
 
Benchmark 2: scc sourcegraph
 
  Warning: Statistical outliers were detected. Consider re-running this benchmark on a quiet system without any interferences from other programs.  Time (mean ± σ):     198.0 ms ±  18.9 ms    [User: 622.3 ms, System: 680.0 ms]
  Range (min … max):   182.3 ms … 246.0 ms    10 runs
 It might help to use the '--warmup' or '--prepare' options.
 
Benchmark 3: scc -c sourcegraph
  Time (mean ± σ):     188.4 ms ±   5.2 ms    [User: 574.6 ms, System: 651.0 ms]
  Range (min … max):   181.7 ms … 196.3 ms    10 runs
 
Benchmark 4: tokei sourcegraph
 
  Warning: Statistical outliers were detected. Consider re-running this benchmark on a quiet system without any interferences from other programs.  Time (mean ± σ):     227.4 ms ±  20.5 ms    [User: 1388.8 ms, System: 394.1 ms]
  Range (min … max):   216.7 ms … 284.6 ms    10 runs
 It might help to use the '--warmup' or '--prepare' options.
 
Benchmark 5: polyglot sourcegraph
  Time (mean ± σ):     200.7 ms ±   3.7 ms    [User: 99.7 ms, System: 282.5 ms]
  Range (min … max):   193.9 ms … 206.8 ms    10 runs
 
Summary
  wtf sourcegraph ran
    2.09 ± 0.18 times faster than scc -c sourcegraph
    2.19 ± 0.28 times faster than scc sourcegraph
    2.22 ± 0.19 times faster than polyglot sourcegraph
    2.52 ± 0.31 times faster than tokei sourcegraph
```

#### CPython

```
--> Running WARM cache benchmark...
Benchmark 1: wtf cpython
  Time (mean ± σ):      50.6 ms ±   7.1 ms    [User: 70.7 ms, System: 63.2 ms]
  Range (min … max):    36.6 ms …  65.5 ms    55 runs
 
Benchmark 2: scc cpython
  Time (mean ± σ):     133.4 ms ±  13.5 ms    [User: 1121.5 ms, System: 319.1 ms]
  Range (min … max):   119.4 ms … 173.4 ms    21 runs
 
Benchmark 3: scc -c cpython
 
  Warning: Statistical outliers were detected. Consider re-running this benchmark on a quiet system without any interferences from other programs. It might help to use the '--warmup' or '--prepare' options.
  Time (mean ± σ):     118.3 ms ±  18.7 ms    [User: 874.2 ms, System: 395.1 ms]
  Range (min … max):   102.8 ms … 196.0 ms    23 runs
 
Benchmark 4: tokei cpython
 
  Warning: Statistical outliers were detected. Consider re-running this benchmark on a quiet system without any interferences from other programs.  Time (mean ± σ):     126.4 ms ±   6.1 ms    [User: 1603.8 ms, System: 99.7 ms]
  Range (min … max):   121.9 ms … 151.0 ms    23 runs
 It might help to use the '--warmup' or '--prepare' options.
 
Benchmark 5: polyglot cpython
  Time (mean ± σ):      60.6 ms ±   2.3 ms    [User: 139.0 ms, System: 124.0 ms]
  Range (min … max):    56.7 ms …  65.9 ms    48 runs
 
Summary
  wtf cpython ran
    1.20 ± 0.17 times faster than polyglot cpython
    2.34 ± 0.49 times faster than scc -c cpython
    2.50 ± 0.37 times faster than tokei cpython
    2.64 ± 0.46 times faster than scc cpython

--> Running COLD cache benchmark...
Benchmark 1: wtf cpython
  Time (mean ± σ):     104.1 ms ±   8.8 ms    [User: 95.4 ms, System: 290.4 ms]
  Range (min … max):    91.6 ms … 118.0 ms    10 runs
 
Benchmark 2: scc cpython
  Time (mean ± σ):     193.2 ms ±  19.2 ms    [User: 1024.4 ms, System: 579.5 ms]
  Range (min … max):   169.1 ms … 230.8 ms    10 runs
 
Benchmark 3: scc -c cpython
  Time (mean ± σ):     174.0 ms ±  13.1 ms    [User: 796.1 ms, System: 602.8 ms]
  Range (min … max):   161.4 ms … 204.0 ms    10 runs
 
Benchmark 4: tokei cpython
 
  Warning:   Time (mean ± σ):     192.1 ms ±  22.0 ms    [User: 1359.0 ms, System: 284.0 ms]
  Range (min … max):   180.3 ms … 254.2 ms    10 runs
Statistical outliers were detected. Consider re-running this benchmark on a quiet system without any interferences from other programs. It might help to use the '--warmup' or '--prepare' options.
 
Benchmark 5: polyglot cpython
  Time (mean ± σ):     156.0 ms ±   5.2 ms    [User: 147.5 ms, System: 274.7 ms]
  Range (min … max):   148.5 ms … 164.4 ms    10 runs
 
Summary
  wtf cpython ran
    1.50 ± 0.14 times faster than polyglot cpython
    1.67 ± 0.19 times faster than scc -c cpython
    1.84 ± 0.26 times faster than tokei cpython
    1.86 ± 0.24 times faster than scc cpython
```

#### Linux

```
--> Running WARM cache benchmark...
Benchmark 1: wtf linux
  Time (mean ± σ):     181.2 ms ±  12.2 ms    [User: 1172.3 ms, System: 791.6 ms]
  Range (min … max):   163.4 ms … 216.1 ms    17 runs
 
Benchmark 2: scc linux
  Time (mean ± σ):      2.429 s ±  0.098 s    [User: 20.861 s, System: 6.257 s]
  Range (min … max):    2.254 s …  2.605 s    10 runs
 
Benchmark 3: scc -c linux
  Time (mean ± σ):      1.734 s ±  0.223 s    [User: 13.287 s, System: 5.652 s]
  Range (min … max):    1.421 s …  1.977 s    10 runs
 
Benchmark 4: tokei linux
  Time (mean ± σ):      1.345 s ±  0.024 s    [User: 15.696 s, System: 2.653 s]
  Range (min … max):    1.319 s …  1.386 s    10 runs
 
Benchmark 5: polyglot linux
  Time (mean ± σ):      1.088 s ±  0.068 s    [User: 5.379 s, System: 1.583 s]
  Range (min … max):    0.987 s …  1.184 s    10 runs
 
Summary
  wtf linux ran
    6.01 ± 0.55 times faster than polyglot linux
    7.42 ± 0.52 times faster than tokei linux
    9.57 ± 1.39 times faster than scc -c linux
   13.40 ± 1.05 times faster than scc linux

--> Running COLD cache benchmark...
Benchmark 1: wtf linux
  Time (mean ± σ):     711.8 ms ±  17.0 ms    [User: 1290.9 ms, System: 4582.8 ms]
  Range (min … max):   691.8 ms … 752.0 ms    10 runs
 
Benchmark 2: scc linux
  Time (mean ± σ):      2.999 s ±  0.117 s    [User: 20.946 s, System: 8.094 s]
  Range (min … max):    2.799 s …  3.199 s    10 runs
 
Benchmark 3: scc -c linux
  Time (mean ± σ):      2.586 s ±  0.123 s    [User: 13.451 s, System: 8.159 s]
  Range (min … max):    2.452 s …  2.810 s    10 runs
 
Benchmark 4: tokei linux
  Time (mean ± σ):      2.410 s ±  0.072 s    [User: 14.929 s, System: 6.607 s]
  Range (min … max):    2.322 s …  2.518 s    10 runs
 
Benchmark 5: polyglot linux
  Time (mean ± σ):      2.391 s ±  0.071 s    [User: 5.235 s, System: 3.876 s]
  Range (min … max):    2.277 s …  2.484 s    10 runs
 
Summary
  wtf linux ran
    3.36 ± 0.13 times faster than polyglot linux
    3.39 ± 0.13 times faster than tokei linux
    3.63 ± 0.19 times faster than scc -c linux
    4.21 ± 0.19 times faster than scc linux
```

</details>

## Deep dive into performance journey

Instead of trying to focus on one specific part, the goal was to optimize the entire process from reading CLI arguments
to actually parsing files. This targeted both cold and hot cache runs, with cold runs getting even more consideration
(since that's the primary way of running CLOC tools).

To break down each thing that was improved:

### 1. Custom compile-time optimized CLI parser

I had three options:

1. Use existing C++ libraries for parsing CLI arguments
2. Parse them manually
3. Make a template based library myself

The first option was not even considered because basically all popular existing options I found online were using heap
allocated strings.

The second option was what I had initially during testing, but it quickly proved to be really annoying to add more
options and overall most of the main () function was focused on handling flags and arguments.

Finally, I decided to quote Thanos and do it myself. I got happy with results, so
now [slic](https://github.com/prevter/slic) is a real single-header library that anyone can use.

This allowed me to both handle the help message and easily add new flags as I was adding features, while also staying
quite fast. Here's a benchmark results table from the slic repo:

```sh
program -v --count 42 --name test --level 3 --output out.txt --debug --threads 8 --timeout 1000 --retry 3 input1.txt input2.txt
```

| Library  | Time (ns) | CPU (ns) | Iterations |
|----------|-----------|----------|------------|
| slic     | 412       | 412      | 2178409    |
| CLI11    | 28640     | 28623    | 25571      |
| cxxopts  | 36981     | 36940    | 22162      |
| argparse | 11283     | 11278    | 59623      |

This by no means my library is better, since other libraries may offer more features, but for my specific use case this
was perfect.

### 2. Directory walking using syscalls

Modern C++ has `std::filesystem` with many ways to iterate directory entries. The problem is that it has to pay the
price of being too generalized, which comes in form of heap allocations for each path string and overall slower
algorithms that don't maximize the OS performance.

Now we need to introduce `getdents64` - a Linux syscall that returns all directory entries without having to allocate
pretty much anything. Each entry looks like this:

```cpp
struct Dirent64 {
    ino64_t d_ino;
    off64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};
```

It contains pretty much everything we need: both the filename and whether it's a directory or file, all neatly placed in
our stack buffer.

macOS has a similar API called `getattrlistbulk`, which might not be as fast, but is still much better than raw C
library functions.

And sadly, Windows is a bit more limited at this, with the closest thing to raw directory iteration being
`FindFirstFileEx`. Although considering how inefficient Windows I/O is, this is hardly a bottleneck anyway.

### 3. OS Scheduler is our biggest enemy and friend

When your application runs at sub-millisecond speeds, standard multi-threading concepts are basically useless. Trying to
balance work between threads becomes hard, since we can't expect all files to be read and parsed at the same speed.

#### Introducing: Work stealing queues!

Now, instead of letting some threads do the work and some just spin waiting, we let the waiting threads steal tasks to
fully saturate the CPU. A generic Chase-Lev queue is perfect for this and was easy to add to existing thread pool.

This still leaves us with by far the biggest bottleneck in the entire program: Disk I/O.

Finding a way to minimize waiting times between starting a file parsing task and actually parsing was very important,
since doing a synchronous `read()` would block the thread until the read completes. The plan was to open the file and
perform a prefetch during directory walk, and then by the time the task is picked up, the file is already fully or
partially loaded into the OS cache.

On Linux this is done using `readahead()` (and `fcntl()` with `F_RDADVISE` flag on macOS), which tells the kernel to
asynchronously preload data into page cache. We can further improve things if we prefetch the next page (for larger
files) right after finishing reading the first one.

While this doesn't improve performance with hot filesystem cache, and even adds a slight overhead when running with hot
cache, ultimately it doesn't matter because CLOC tool is supposed to run once on a cold cache directory.

### 4. Heap is sacred, stack is our friend

There are 3 types of memory allocations:

- Static memory (whatever gets added into the executable binary itself, usually read only)
- Heap memory (every `operator new()` and `malloc()`)
- Stack memory (local variables/arrays)

The most obvious difference is between them is how much memory it can give you and how fast it is. Heap is generally
slower than stack memory due to requiring separate memory pages. The benefit here is you can allocate much larger data
structures.

But how large do we need for a line counter tool? Great question, realistically we only need to store few things:

- Overall progress/stats
- Thread pool
- Per-thread file buffer

By default, Linux gives us whopping 8 MB of stack memory per thread. This is more than enough for our use case, so we
can fit most things on stack. The only things left we can leave to heap (due to not having a predefined limit size or
having to outlive the function) are:

- gitignore rules
- threads / per-thread statistics
- file paths (mostly for per-file stats calculation)

### 5. Reimplementing fnmatch

Initially this wasn't even planned up until I decided to add Windows support. Regular fnmatch was already fast enough
during profiling to not matter much, but not having it on Windows ended up forcing me to rewrite it from scratch (mostly
following existing implementation from musl, but simplifying logic that my code would never use).

#### What even is fnmatch?

In short, it's a POSIX function that is used for path globs almost everywhere. Git also directly uses it, most notably
in .gitignore files to support operations with asterisks (e.g. `*.obj` to remove all object files)
and bracketed matches like `file[0-9].txt`.

Closest Windows alternative to this is `PathMatchSpecEx`, which is more limited and lacks some features we needed for
full-spec .gitignore support.

Now this led me to realize that instead of just copy-pasting some existing implementation, I could rewrite it to be more
efficient at what I needed. Specifically, handle string views (which reduces `strlen` call counts), as well as returning
some pre-parsed information for each entry to avoid calling the function at all in some cases.

As a result, `wtf::glob::Pattern` was made, which stores all information about the pattern you would need to match the
filename, which ended up shaving ~6% of run time on larger repositories (tested on linux kernel) compared to fnmatch.

### 6. Language definitions codegen

One thing I love about C++ is how you can do basically anything at compile time using templates. The idea quickly came
into my head, instead of making a parser that understands all languages or writing a separate one manually, why not just
make a generic version that uses compile time specifications about each language?

For this I made a [Python script](src/gen_langs.py) that reads all language definitions and generates a header
containing all the information about each language, including comment tokens, string tokens, and especially the
"important" tokens (more about that in SIMD section).

This also allowed me to make an efficient extension to language mapper that uses binary search which is a much nicer
`O(log n)` compared to linear search and at current scale cheaper than hashing and map lookup.

### 7. Cache lines and why they're important

This might be the least obvious thing to any beginner dev, but the CPU can predict future. Well not really, but it does
have branch predictor which makes a best guess of which instruction will run next and pre-emptively runs it. To help it,
we can make sure the cache lines are always warm and don't thrash between different threads.

Basic way to avoid constant mutex locking/unlocking or usage of atomics would be to give each thread a unique container
and later combine all data in post-processing stage. But what happens if these containers are too close in memory and/or
live in the same vector?

Thread A will write to its own container and invalidate the lane of 64 bytes. This might be close enough to Thread B's
container, which would result in Thread B having to pull memory again for no reason. The solution is to set alignment of
each container to `std::hardware_destructive_interference_size`. It makes sure each container stays within cache
boundaries by adding enough padding between them.

This trick ended up being used for both the work stealing queue and the LOC buckets for each thread and slightly
improved performance in my benchmarks.

### 8. `mmap()` or not `mmap()`?

Most people might've heard about `mmap()` in Linux and that it allows to open any file without having to copy it into
buffers. In theory this is just what we need - an efficient way of reading files.

In practice, this ended up being bad for most repositories, since mmap introduces page faults and doesn't work great
with sequential reads. Most average source file won't reach above 128 KiB, at which point copying it into a buffer is
way cheaper and efficient.

That's why in my `FileReader` design I ended up having a 256 KiB buffer that lives on stack and optionally allows
fetching more pages when file is larger than the buffer. This resulted in very fast parsing loops, since the data now
lives in RAM / CPU cache and can be iterated over efficiently.

### 9. SIMD optimizations

Probably the most obvious thing one could do to optimize any algorithm is to implement **SIMD** (Single Instruction
Multiple Data). The question is, where and how should we use it?

When performing line counting with comment/blank line checks, the most important thing is to know where the states
change. To do this, we can use SSE2/AVX2 instructions to check blocks of 16/32 bytes at once and find any usage of
"important" characters.

"Important" characters here imply newlines and any language-specific token that changes the state. This could be a quote
to begin a new string, a hash or slash to signalize comment or pretty much anything else that would impact the line
counting result.

In addition to already having the language codegen, each SIMD function is templated too and unrolls the loop for unique
languages. This got to the point where I sometimes fail to see the parsing logic in profiler output, due to it taking
very minuscule amount of time.

## Acknowledgements

- [tokei](https://github.com/xampprocky/tokei) for the languages.json file
- [musl](https://musl.libc.org/) for reference on fnmatch implementation
- [fmtlib](https://github.com/fmtlib/fmt) - fast string formatting library
- [doctest](https://github.com/doctest/doctest) - very easy to use unit testing library

## License

This project is licensed under the MIT license. See [LICENSE.md](LICENSE.md) for more details.