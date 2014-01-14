# Good tutorials:
# http://freshrpms.net/docs/fight/
# http://rpm.org/api/4.4.2.2/conditionalbuilds.html
# http://fedoraproject.org/wiki/How_to_create_an_RPM_package
# http://backreference.org/2011/09/17/some-tips-on-rpm-conditional-macros/
# Repos:
# http://eureka.ykyuen.info/2010/01/06/opensuse-create-your-own-software-repository-1/
# http://en.opensuse.org/SDB:Creating_YaST_installation_sources

%define feature         dlb

%if 0%{?suse_version}
%define distro        opensuse%{?suse_version}
%else
%define distro        %{?dist}
%endif

%define buildroot    %{_topdir}/%{name}-%{version}-root
# Avoid "*** ERROR: No build ID note found in XXXXXXX"
%global debug_package   %{nil}

# Override prefix if _rpm_prefix is given
%{?_rpm_prefix: %define _prefix  %{_rpm_prefix} }

BuildRoot:           %{buildroot}
Summary:             DLB Dynamic Load Balancing
License:             GPL
Name:                %{feature}-openmpi
Version:             %{version}
Release:             %{release}%{distro}
Source:              %{feature}-%{version}.tar.gz
Prefix:              %{_prefix}
Group:               Development/Tools
Provides:            %{feature}
Requires:            openmpi
BuildRequires:       openmpi-devel

%description
DLB Dynamic Load Balancing

%prep
%setup -q -n %{feature}-%{version}

%build
%if 0%{?suse_version}
%configure --with-mpi=/usr/lib64/mpi/gcc/openmpi CFLAGS=-U_FORTIFY_SOURCE
%else
%configure --with-mpi=/usr/lib64/openmpi --with-mpi-headers=/usr/include/openmpi-x86_64 CFLAGS=-U_FORTIFY_SOURCE
%endif
make -j%{threads}

#%check
#make check

%install
%makeinstall

%files
%defattr(-,root,root)
%{_bindir}/*
%{_includedir}/*
%{_libdir}/*
%{_datarootdir}/paraver_cfgs/*
