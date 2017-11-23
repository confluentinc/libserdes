#!/bin/bash
#

mkl_require cxx
mkl_require lib
mkl_require pic
mkl_require good_cflags
mkl_require socket


function checks {

    # Semi optional libs
    mkl_lib_check "avro-c" ENABLE_AVRO_C fail CC "-lavro" \
		  "#include <avro.h>"
    mkl_lib_check "avro-cpp" ENABLE_AVRO_CPP disable CXX "-lavrocpp" ""
    mkl_lib_check "librdkafka" ENABLE_LIBRDKAFKA disable CXX "-lrdkafka++" \
		  "#include <librdkafka/rdkafkacpp.h>"

    # Required libs
    mkl_lib_check "jansson" "" fail CC "-ljansson"
    mkl_lib_check "libcurl" "" fail CC "-lcurl"
    mkl_lib_check "libpthread" "" fail CC "-lpthread"

    # Older g++ (<=4.1?) gives invalid warnings for the C++ code.
    mkl_mkvar_append CXXFLAGS CXXFLAGS "-Wno-non-virtual-dtor"

    # -lrt is needed on linux for clock_gettime: link it if it exists.
    mkl_lib_check "librt" "" cont CC "-lrt"

    # Required on SunOS
    if [[ $MKL_DISTRO == "SunOS" ]]; then
	mkl_mkvar_append CPPFLAGS CPPFLAGS "-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT -D__EXTENSIONS__"
    fi

    # Figure out what tool to use for dumping public symbols.
    # We rely on configure.cc setting up $NM if it exists.
    if mkl_env_check "nm" "" cont "NM" ; then
	# nm by future mk var
	if [[ $MKL_DISTRO == "osx" || $MKL_DISTRO == "AIX" ]]; then
	    mkl_mkvar_set SYMDUMPER SYMDUMPER '$(NM) -g'
	else
	    mkl_mkvar_set SYMDUMPER SYMDUMPER '$(NM) -D'
	fi
    else
	# Fake symdumper
	mkl_mkvar_set SYMDUMPER SYMDUMPER 'echo'
    fi

    # The linker-script generator (lds-gen.pl) requires perl
    if [[ $WITH_LDS == y ]]; then
        if ! mkl_command_check perl "HAVE_PERL" "disable" "perl -v"; then
            mkl_err "disabling linker-script since perl is not available"
            mkl_mkvar_set WITH_LDS WITH_LDS "n"
        fi
    fi

}

