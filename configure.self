#!/bin/bash
#

mkl_meta_set "description" "name"      "libserdes"
mkl_meta_set "description" "oneline"   "Confluent Schema Registry C/C++ client"
mkl_meta_set "description" "long"      "C/C++ client for interacting with the Confluent Schema Registry."
mkl_meta_set "description" "copyright" "Copyright (c) 2015-2020 Confluent Inc"

# Enable generation of pkg-config .pc file
mkl_mkvar_set "" GEN_PKG_CONFIG y

mkl_require cxx
mkl_require lib
mkl_require pic
mkl_require good_cflags
mkl_require socket


function checks {

    # Required libs
    mkl_lib_check "jansson" "" fail CC "-ljansson"
    mkl_lib_check "libcurl" "" fail CC "-lcurl"
    mkl_lib_check "libpthread" "" fail CC "-lpthread"

    # Older g++ (<=4.1?) gives invalid warnings for the C++ code.
    mkl_mkvar_append CXXFLAGS CXXFLAGS "-Wno-non-virtual-dtor"

    # -lrt is needed on linux for clock_gettime: link it if it exists.
    mkl_lib_check "librt" "" cont CC "-lrt"

    # Required on SunOS
    if [[ $MKL_DISTRO == "sunos" ]]; then
	mkl_mkvar_append CPPFLAGS CPPFLAGS "-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT -D__EXTENSIONS__"
        # Source defines _POSIX_C_SOURCE to 200809L for Solaris, and this is
	# incompatible on that platform with compilers < c99.
	mkl_mkvar_append CFLAGS CFLAGS "-std=c99"
    fi

    # Figure out what tool to use for dumping public symbols.
    # We rely on configure.cc setting up $NM if it exists.
    if mkl_env_check "nm" "" cont "NM" ; then
	# nm by future mk var
	if [[ $MKL_DISTRO == "osx" || $MKL_DISTRO == "aix" ]]; then
	    mkl_mkvar_set SYMDUMPER SYMDUMPER '$(NM) -g'
	else
	    mkl_mkvar_set SYMDUMPER SYMDUMPER '$(NM) -D'
	fi
    else
	# Fake symdumper
	mkl_mkvar_set SYMDUMPER SYMDUMPER 'echo'
    fi

    # Various dependencies needed by some of the examples
    mkl_lib_check --libname="avro-c" "avro_c" ENABLE_AVRO_C fail CC "-lavro" \
		  "#include <avro.h>"
    mkl_lib_check --libname="avro-cpp" "avro_cpp" ENABLE_AVRO_CPP disable CXX "-lavrocpp" \
                  "#include <avro/Compiler.hh>"
    mkl_lib_check "librdkafka" ENABLE_LIBRDKAFKA disable CXX "-lrdkafka++" \
		  "#include <librdkafka/rdkafkacpp.h>"

    # The linker-script generator (lds-gen.py) requires python3
    if [[ $WITH_LDS == y ]]; then
        if ! mkl_command_check python3 "HAVE_PYTHON" "disable" "python3 -V"; then
            mkl_err "disabling linker-script since python3 is not available"
            mkl_mkvar_set WITH_LDS WITH_LDS "n"
        fi
    fi
}

