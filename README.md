## lunatic

![license](https://img.shields.io/github/license/fleroviux/lunatic)
[![CodeFactor](https://www.codefactor.io/repository/github/fleroviux/lunatic/badge)](https://www.codefactor.io/repository/github/fleroviux/lunatic)

lunatic is an ARM (32-bit) dynamic recompiler for low-level emulators.  

## General

lunatic currently supports the following guest and host architectures:

#### Guest architectures
- ARMv4T
- ARMv5TE

See ![MISSING.md](docs/MISSING.md) for a list of missing instructions.

#### Host architectures
- x86_64

## Credit

I would like to thank the following people:
- [merry](https://github.com/merryhime)
- [wheremyfoodat](https://github.com/wheremyfoodat)

for answering my questions (especially related to x86_64) and pointing out bugs and improvements.

## License

lunatic is licensed under the New/Modified BSD license. See [LICENSE](LICENSE) file for details.

lunatic uses [Xbyak](https://github.com/herumi/xbyak/) which is licensed under the New/Modified BSD license.

lunatic uses C++ devirtualization code from [dynarmic](https://github.com/merryhime/dynarmic) (including supporting code from [mcl](https://github.com/merryhime/mcl)).
dynarmic is licensed under the 0BSD license. mcl is licensed under the MIT license.

lunatic uses [{fmt}](https://github.com/fmtlib/fmt/) which is licensed under a modified [MIT license](https://github.com/fmtlib/fmt/blob/master/LICENSE.rst):
```
Copyright (c) 2012 - present, Victor Zverovich

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

--- Optional exception to the license ---

As an exception, if, as a result of your compiling your source code, portions of this Software are embedded into a machine-executable object form of such source code, you may redistribute such embedded portions in such object form without including the above copyright and permission notices.
```
