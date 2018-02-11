[![CSUS](http://www.csus.edu/Brand/assets/Logos/Core/Primary/Stacked/Primary_Stacked_3_Color_wht_hndTN.png)](http://www.csus.edu/)

# CSUS LLVM
CSU Sacramento compiler project.

We have a [slack channel](https://csusllvm.slack.com/signup) for easier communication. Sign up with your csus email.

## Requirements
```
- Ubuntu 16.04 is highly recommended
- gcc 4.8 built with plugin support
- g++ 4.8
- gfortran 4.8
- cmake 3.4.3 or later
```

## Set up a development environment

1. Clone the repository (CONTRIBUTING.md contains some tips on using git).
2. Download the modified dragonegg from this google drive [link](https://drive.google.com/drive/folders/0B0PcXgBFyHNqcXkyb0pCU3QxY2M?usp=sharing).
3. Extract dragonegg and copy it to the ["Generic"](https://gitlab.com/CSUS_LLVM/LLVM_DRAGONEGG/tree/master/Generic) directory
4. If you are using Ubuntu, run the script [CompileLLVMandDRAGONEGG.sh](https://gitlab.com/CSUS_LLVM/LLVM_DRAGONEGG/blob/master/CompileLLVMandDRAGONEGG.sh) and follow the prompts to build LLVM and dragonegg.
	- Run `./CompileLLVMandDRAGONEGG` in the base of the repository.
5. Our scheduler's code is in the CodeGen directory.
	- Source files can be found in [lib/CodeGen/OptSched](https://gitlab.com/CSUS_LLVM/LLVM_DRAGONEGG/tree/master/Generic/llvmTip/llvm-master/lib/CodeGen/OptSched)
	- Headder files can be found in [include/llvm/CodeGen/OptSched](https://gitlab.com/CSUS_LLVM/LLVM_DRAGONEGG/tree/master/Generic/llvmTip/llvm-master/include/llvm/CodeGen/OptSched)
6. After modifying the code you can use the script [CompileLLVMandDRAGONEGG](https://gitlab.com/CSUS_LLVM/LLVM_DRAGONEGG/blob/master/CompileLLVMandDRAGONEGG.sh) to rebuild LLVM. Note that dragonegg is statically linked and must also be rebuilt after every change.

## Fresh Start Guide

- [**Look here if you need more detailed help getting started**](https://docs.google.com/document/d/1AmqCsN1CJFvuNOf4hm21SVAJCpPygFirjnRFElgmCCY/)

## Invoking our scheduler

Our scheduler, "OptSched", is integrated as a "machine scheduler" within LLVM. To invoke our scheduler use the LLVM option `-misched=optsched`. It is also nessessary to provide LLVM with the path to your configuration files. Do this with the `-optsched-cfg` option.

By default "OptSchedCfg" is located at [Generic/OptSchedCfg](https://gitlab.com/CSUS_LLVM/LLVM_DRAGONEGG/tree/master/Generic/OptSchedCfg).

- For example, to compile a program using our scheduler with clang use the command:

		clang -O3 -mllvm -misched=optsched -mllvm -optsched-cfg=**/path/to/OptSchedCfg/** test.c

## Dragonegg

Dragonegg is a gcc plugin that allows us to compile FORTRAN benchmarks with the LLVM backend.

- To compile a program using dragonegg use the command:

		/usr/bin/gfortran-4.8  -O3 -fplugin=**/path/to/dragonegg/dragonegg.so** -fplugin-arg-dragonegg-llvm-option=-misched:optsched -fplugin-arg-dragonegg-llvm-option=-optsched-cfg:**/path/to/OptSchedCfg/** test.f
