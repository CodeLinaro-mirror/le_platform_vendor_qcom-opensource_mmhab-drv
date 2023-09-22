# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion %(uname -r)}

%define kmod_name msmhab

%define debug_package %{nil}

Name: %{kmod_name}
Version: 1.0
Release:        1%{?dist}
Summary:hab kernel drivers

License: GPLv2
Source0: %{name}-%{version}.tar.gz

BuildRequires: modules-signkey
BuildRequires: kernel-automotive-devel-uname-r = %{kversion}
Requires: kernel-automotive-core-uname-r = %{kversion}

%description
This is a rpm contains hab out of tree kernel modules.

%prep
%setup -qn %{name}

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
make KERNEL_SRC=${KSRC} modules

%post
depmod %{kversion}

%postun
depmod %{kversion}

%install
KSRC=%{_usrsrc}/kernels/%{kversion}
make KERNEL_SRC=${KSRC} INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install
make KERNEL_SRC=${KSRC} HDR_INSTAL_PATH=$RPM_BUILD_ROOT/usr/include headers_install
%{__install} -d %{buildroot}%{_sysconfdir}/modules-load.d/
%{__install} %{kmod_name}.conf %{buildroot}%{_sysconfdir}/modules-load.d/
rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion}/modules."*

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_includedir}/linux/habmmid.h
%{_includedir}/linux/hab_ioctl.h
/lib/modules/%{kversion}/extra/msm_hab.ko
%{_sysconfdir}/modules-load.d/%{kmod_name}.conf
