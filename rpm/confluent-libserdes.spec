Name:    confluent-libserdes
Version: %{__version}
Release: %{__release}%{?dist}

Summary: Confluent Schema-registry client library with Avro support
Group:   Development/Libraries/C and C++
License: ASL 2.0
URL:     https://github.com/confluentinc/libserdes
Source:	 confluent-libserdes-%{version}.tar.gz

BuildRequires: libcurl-devel jansson-devel avro-c-devel avro-cpp-devel perl librdkafka-devel
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

%description
This package contains libraries required to run applications using libserdes (C and C++).
.

%package devel
Summary: Confluent Schema-registry client library with Avro support (Development Environment)
Group:   Development/Libraries/C and C++
Requires: %{name} = %{version}

%description devel
This package contains headers and libraries required to build applications using libserdes (C and C++).



%prep
%setup -q -n %{name}-%{version}

%configure

%build
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
DESTDIR=%{buildroot} make install

%clean
rm -rf %{buildroot}

%post   -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%{_libdir}/libserdes.so.*
%{_libdir}/libserdes++.so.*
%defattr(-,root,root)
%doc LICENSE README.md


%files devel
%defattr(444,root,root)
%{_includedir}/libserdes
%{_libdir}/libserdes.a
%{_libdir}/libserdes.so
%{_libdir}/libserdes++.a
%{_libdir}/libserdes++.so
%doc LICENSE README.md


%changelog
* Mon May 16 2016 Magnus Edenhill <magnus@confluent.io> 1.0.0-0
- Initial RPM
